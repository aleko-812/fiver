#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "delta_structures.h"

// Comparison function for qsort
static int compare_matches(const void *a, const void *b)
{
	const Match *match_a = (const Match *)a;
	const Match *match_b = (const Match *)b;

	if (match_a->new_offset < match_b->new_offset) return -1;
	if (match_a->new_offset > match_b->new_offset) return 1;
	return 0;
}

// Simple match allocation - memory pool was causing issues
static Match* match_alloc(void)
{
	return malloc(sizeof(Match));
}

// Forward declarations
RollingHash * rolling_hash_new(uint32_t window_size);
void rolling_hash_update(RollingHash *rh, uint8_t byte);
uint32_t rolling_hash_get_hash(RollingHash *rh);
void rolling_hash_free(RollingHash *rh);

HashTable * hash_table_new(uint32_t bucket_count);
void hash_table_insert(HashTable *ht, uint32_t hash, uint32_t offset);
HashEntry * hash_table_find(HashTable *ht, uint32_t hash);
void hash_table_free(HashTable *ht);

// Match and DeltaState are now defined in delta_structures.h

/**
 * Initialize delta state
 */
DeltaState * delta_state_new(uint32_t initial_capacity)
{
	DeltaState *state = malloc(sizeof(DeltaState));

	if (state == NULL)
		return NULL;

	state->new_pos = 0;
	state->original_pos = 0;
	state->match_count = 0;
	state->matches_capacity = initial_capacity;

	state->matches = malloc(initial_capacity * sizeof(Match));
	if (state->matches == NULL) {
		free(state);
		return NULL;
	}

	return state;
}

/**
 * Add a match to the delta state
 */
int delta_state_add_match(DeltaState *state, uint32_t original_offset,
			  uint32_t new_offset, uint32_t length)
{
	if (state == NULL)
		return -1;

	// Resize if needed
	if (state->match_count >= state->matches_capacity) {
		uint32_t new_capacity = state->matches_capacity * 2;
		Match *new_matches = realloc(state->matches, new_capacity * sizeof(Match));
		if (new_matches == NULL)
			return -1;

		state->matches = new_matches;
		state->matches_capacity = new_capacity;
	}

	// Add match
	state->matches[state->match_count].original_offset = original_offset;
	state->matches[state->match_count].new_offset = new_offset;
	state->matches[state->match_count].length = length;
	state->match_count++;

	return EXIT_SUCCESS;
}

/**
 * Free delta state
 */
void delta_state_free(DeltaState *state)
{
	if (state == NULL)
		return;
	free(state->matches);
	free(state);
}

/**
 * Verify that a match is actually valid by comparing bytes
 */
int verify_match(const uint8_t *original_data, uint32_t original_size,
		 const uint8_t *new_data, uint32_t new_size,
		 uint32_t original_offset, uint32_t new_offset, uint32_t length)
{
	// Check bounds
	if (original_offset + length > original_size ||
	    new_offset + length > new_size)
		return EXIT_SUCCESS;

	// Compare actual bytes
	return memcmp(original_data + original_offset,
		      new_data + new_offset, length) == 0;
}

/**
 * Find the best match for a given position in the new file (optimized version)
 */
Match * find_best_match_optimized(const uint8_t *original_data, uint32_t original_size,
				 const uint8_t *new_data, uint32_t new_size,
				 const HashTable *ht, uint32_t window_size,
				 uint32_t new_pos, uint32_t min_match_length,
				 RollingHash *rh)
{
	if (new_pos + window_size > new_size)
		return NULL;

	// Update rolling hash incrementally (much faster than recreating)
	if (new_pos == 0) {
		// First position: fill the rolling hash
		for (uint32_t i = 0; i < window_size; i++)
			rolling_hash_update(rh, new_data[i]);
	} else {
		// Subsequent positions: just update with new byte
		rolling_hash_update(rh, new_data[new_pos + window_size - 1]);
	}

	uint32_t hash = rolling_hash_get_hash(rh);

	// Look for matches in original file
	HashEntry *match_entry = hash_table_find((HashTable *)ht, hash);

	Match *best_match = NULL;
	uint32_t best_length = 0;

	// Check all matches with this hash (with early termination)
	HashEntry *current = match_entry;
	uint32_t max_candidates = 10; // Limit hash collision checking for performance
	uint32_t candidates_checked = 0;

	while (current != NULL && candidates_checked < max_candidates) {
		if (current->hash == hash) {
			uint32_t original_offset = current->offset;
			candidates_checked++;

			// Try to extend the match beyond the window
			uint32_t match_length = window_size;

			// Extend forward as long as bytes match (optimized with larger strides)
			while (new_pos + match_length < new_size &&
			       original_offset + match_length < original_size) {
				// Check 8 bytes at a time for even better performance
				if (match_length + 8 <= new_size &&
				    match_length + 8 <= original_size) {
					uint64_t *new_chunk = (uint64_t*)(new_data + new_pos + match_length);
					uint64_t *orig_chunk = (uint64_t*)(original_data + original_offset + match_length);
					if (*new_chunk == *orig_chunk) {
						match_length += 8;
						continue;
					}
				}
				// Check 4 bytes at a time for better performance
				if (match_length + 4 <= new_size &&
				    match_length + 4 <= original_size) {
					uint32_t *new_chunk = (uint32_t*)(new_data + new_pos + match_length);
					uint32_t *orig_chunk = (uint32_t*)(original_data + original_offset + match_length);
					if (*new_chunk == *orig_chunk) {
						match_length += 4;
						continue;
					}
				}
				// Fall back to byte-by-byte for remaining bytes
				if (new_data[new_pos + match_length] ==
				    original_data[original_offset + match_length]) {
					match_length++;
				} else {
					break;
				}
			}

			// Only consider matches that meet minimum length
			if (match_length >= min_match_length && match_length > best_length) {
				// Skip expensive verification for now - the match extension is already verified
				best_length = match_length;

				// Create or update best match
				if (best_match == NULL)
					best_match = match_alloc();
				if (best_match != NULL) {
					best_match->original_offset = original_offset;
					best_match->new_offset = new_pos;
					best_match->length = match_length;
				}
			}
		}
		current = current->next;
	}

	return best_match;
}

/**
 * Find the best match for a given position in the new file (original version)
 */
Match * find_best_match(const uint8_t *original_data, uint32_t original_size,
			const uint8_t *new_data, uint32_t new_size,
			const HashTable *ht, uint32_t window_size,
			uint32_t new_pos, uint32_t min_match_length)
{
	if (new_pos + window_size > new_size)
		return NULL;

	// Calculate hash for current window in new file
	RollingHash *rh = rolling_hash_new(window_size);
	if (rh == NULL)
		return NULL;

	// Fill the rolling hash with the window at new_pos
	for (uint32_t i = 0; i < window_size; i++)
		rolling_hash_update(rh, new_data[new_pos + i]);

	uint32_t hash = rolling_hash_get_hash(rh);

	// Look for matches in original file
	HashEntry *match_entry = hash_table_find((HashTable *)ht, hash);

	Match *best_match = NULL;
	uint32_t best_length = 0;

	// Check all matches with this hash
	HashEntry *current = match_entry;
	while (current != NULL) {
		if (current->hash == hash) {
			uint32_t original_offset = current->offset;

			// Try to extend the match beyond the window
			uint32_t match_length = window_size;

			// Extend forward as long as bytes match
			while (new_pos + match_length < new_size &&
			       original_offset + match_length < original_size &&
			       new_data[new_pos + match_length] ==
			       original_data[original_offset + match_length])
				match_length++;

			// Only consider matches that meet minimum length
			if (match_length >= min_match_length && match_length > best_length) {
				// Verify the match is valid
				if (verify_match(original_data, original_size, new_data, new_size,
						 original_offset, new_pos, match_length)) {
					best_length = match_length;

					// Create or update best match
					if (best_match == NULL)
						best_match = malloc(sizeof(Match));
					if (best_match != NULL) {
						best_match->original_offset = original_offset;
						best_match->new_offset = new_pos;
						best_match->length = match_length;
					}
				}
			}
		}
		current = current->next;
	}

	rolling_hash_free(rh);
	return best_match;
}

/**
 * Create delta operations from matches
 */
DeltaInfo * create_delta_operations(const uint8_t *original_data, uint32_t original_size,
				    const uint8_t *new_data, uint32_t new_size,
				    const DeltaState *state)
{
	(void)original_data; // Parameter not used in this implementation
	if (state == NULL)
		return NULL;

	DeltaInfo *delta = malloc(sizeof(DeltaInfo));
	if (delta == NULL)
		return NULL;

	delta->original_size = original_size;
	delta->new_size = 0; // Will be calculated from operations
	delta->operation_count = 0;
	delta->operations = NULL;
	delta->delta_size = 0;

	// Sort matches by new_offset for processing in order
	// Using system qsort for O(n log n) performance and stack safety
	if (state->match_count > 1) {
		qsort(state->matches, state->match_count, sizeof(Match), compare_matches);
	}

	// Convert matches to delta operations
	uint32_t current_new_pos = 0;
	uint32_t operations_capacity = 10;
	delta->operations = malloc(operations_capacity * sizeof(DeltaOperation));

	if (delta->operations == NULL) {
		free(delta);
		return NULL;
	}

	for (uint32_t i = 0; i < state->match_count; i++) {
		Match *match = &state->matches[i];

		// Add INSERT operation for any data before this match
		if (match->new_offset > current_new_pos) {
			uint32_t insert_length = match->new_offset - current_new_pos;

			// Resize operations array if needed
			if (delta->operation_count >= operations_capacity) {
				operations_capacity *= 2;
				DeltaOperation *new_ops = realloc(delta->operations,
								  operations_capacity * sizeof(DeltaOperation));
				if (new_ops == NULL) {
					// Handle error
					for (uint32_t j = 0; j < delta->operation_count; j++)
						if (delta->operations[j].data != NULL)
							free(delta->operations[j].data);
					free(delta->operations);
					free(delta);
					return NULL;
				}
				delta->operations = new_ops;
			}

			// Create INSERT operation
			DeltaOperation *op = &delta->operations[delta->operation_count];
			op->type = DELTA_INSERT;
			op->offset = 0; // Not used for INSERT
			op->length = insert_length;
			op->data = malloc(insert_length);

			if (op->data == NULL) {
				// Handle error
				for (uint32_t j = 0; j < delta->operation_count; j++)
					if (delta->operations[j].data != NULL)
						free(delta->operations[j].data);
				free(delta->operations);
				free(delta);
				return NULL;
			}

			memcpy(op->data, new_data + current_new_pos, insert_length);
			delta->operation_count++;
			delta->delta_size += insert_length;
		}

		// Add COPY operation for the match
		if (delta->operation_count >= operations_capacity) {
			operations_capacity *= 2;
			DeltaOperation *new_ops = realloc(delta->operations,
							  operations_capacity * sizeof(DeltaOperation));
			if (new_ops == NULL) {
				// Handle error
				for (uint32_t j = 0; j < delta->operation_count; j++)
					if (delta->operations[j].data != NULL)
						free(delta->operations[j].data);
				free(delta->operations);
				free(delta);
				return NULL;
			}
			delta->operations = new_ops;
		}

		DeltaOperation *op = &delta->operations[delta->operation_count];
		op->type = DELTA_COPY;
		op->offset = match->original_offset;
		op->length = match->length;
		op->data = NULL; // No data for COPY operations
		delta->operation_count++;

		current_new_pos = match->new_offset + match->length;
	}

	// Add final INSERT operation if there's remaining data
	if (current_new_pos < new_size) {
		uint32_t insert_length = new_size - current_new_pos;

		if (delta->operation_count >= operations_capacity) {
			operations_capacity *= 2;
			DeltaOperation *new_ops = realloc(delta->operations,
							  operations_capacity * sizeof(DeltaOperation));
			if (new_ops == NULL) {
				// Handle error
				for (uint32_t j = 0; j < delta->operation_count; j++)
					if (delta->operations[j].data != NULL)
						free(delta->operations[j].data);
				free(delta->operations);
				free(delta);
				return NULL;
			}
			delta->operations = new_ops;
		}

		DeltaOperation *op = &delta->operations[delta->operation_count];
		op->type = DELTA_INSERT;
		op->offset = 0;
		op->length = insert_length;
		op->data = malloc(insert_length);

		if (op->data == NULL) {
			// Handle error
			for (uint32_t j = 0; j < delta->operation_count; j++)
				if (delta->operations[j].data != NULL)
					free(delta->operations[j].data);
			free(delta->operations);
			free(delta);
			return NULL;
		}

		memcpy(op->data, new_data + current_new_pos, insert_length);
		delta->operation_count++;
		delta->delta_size += insert_length;
	}

	// Calculate the final new_size from operations
	uint32_t calculated_new_size = 0;
	for (uint32_t i = 0; i < delta->operation_count; i++) {
		DeltaOperation *op = &delta->operations[i];
		switch (op->type) {
		case DELTA_COPY:
			calculated_new_size += op->length;
			break;
		case DELTA_INSERT:
			calculated_new_size += op->length;
			break;
		case DELTA_REPLACE:
			calculated_new_size += op->length;
			break;
		}
	}
	delta->new_size = calculated_new_size;

	return delta;
}

/**
 * Main delta creation function
 */
DeltaInfo * delta_create(const uint8_t *original_data, uint32_t original_size,
			 const uint8_t *new_data, uint32_t new_size)
{
	if (original_data == NULL || new_data == NULL)
		return NULL;

	// Algorithm parameters
	uint32_t window_size = 32;      // Sliding window size (increased for better performance)
	uint32_t min_match_length = 32; // Minimum match length to consider
	uint32_t bucket_count = 65536;  // Hash table size (increased for better distribution)

	printf("Creating delta...\n");
	printf("Original size: %u bytes\n", original_size);
	printf("New size: %u bytes\n", new_size);
	printf("Window size: %u bytes\n", window_size);
	printf("Min match length: %u bytes\n", min_match_length);

	// Memory management is now handled with simple malloc/free

	// Step 1: Build hash table from original file
	HashTable *ht = hash_table_new(bucket_count);
	if (ht == NULL) {
		printf("Failed to create hash table\n");
		return NULL;
	}

	printf("Building hash table from original file...\n");
	RollingHash *rh = rolling_hash_new(window_size);
	if (rh == NULL) {
		hash_table_free(ht);
		return NULL;
	}

	// Process original file with sliding window
	uint32_t progress_interval = original_size / 200; // Report every 0.5%
	if (progress_interval == 0) progress_interval = 1;

	for (uint32_t i = 0; i < original_size; i++) {
		rolling_hash_update(rh, original_data[i]);

		if (i >= window_size - 1) {
			uint32_t hash = rolling_hash_get_hash(rh);
			uint32_t offset = i - window_size + 1;
			hash_table_insert(ht, hash, offset);
		}

		// Progress reporting
		if (i % progress_interval == 0) {
			uint32_t progress_percent = (uint32_t)((i * 100ULL) / original_size);
			printf("\rProgress: %u%% (%u/%u bytes)",
			       progress_percent, i, original_size);
			fflush(stdout);
		}
	}
	printf("\n");

	printf("Hash table built with %u entries\n", ht->entry_count);
	rolling_hash_free(rh);

	// Step 2: Find matches in new file
	printf("Finding matches in new file...\n");
	DeltaState *state = delta_state_new(100);
	if (state == NULL) {
		hash_table_free(ht);
		return NULL;
	}

	// Create rolling hash once and reuse it
	RollingHash *match_rh = rolling_hash_new(window_size);
	if (match_rh == NULL) {
		delta_state_free(state);
		hash_table_free(ht);
		return NULL;
	}

	uint32_t match_count = 0;
	uint32_t match_progress_interval = new_size / 1000; // Report every 0.1% for more frequent updates
	if (match_progress_interval == 0) match_progress_interval = 1;

	// Cost-benefit analysis: minimum match length that provides compression benefit
	// For a match to be worthwhile, it should save more bytes than the overhead of storing it
	// COPY operation overhead: ~12 bytes (type + offset + length)
	// INSERT operation overhead: ~8 bytes (type + length) + data
	// So a match should be at least 12+ bytes to be worthwhile
	uint32_t min_beneficial_match_length = 12;

	// For very large files, be more aggressive about skipping small matches
	if (new_size > 50 * 1024 * 1024) { // 50MB+
		min_beneficial_match_length = 16; // Require longer matches for large files
	} else if (new_size > 10 * 1024 * 1024) { // 10MB+
		min_beneficial_match_length = 14;
	}
	uint32_t skipped_small_matches = 0;

	printf("Using minimum beneficial match length: %u bytes (file size: %u bytes)\n",
	       min_beneficial_match_length, new_size);

	// Process new file with sliding window
	for (uint32_t i = 0; i < new_size; i++) {
		// Find best match starting at position i
		Match *match = find_best_match_optimized(original_data, original_size,
							new_data, new_size, ht, window_size,
							i, min_match_length, match_rh);

		if (match != NULL) {
			// Cost-benefit analysis: only use matches that provide real compression benefit
			if (match->length >= min_beneficial_match_length) {
				// Add match to state
				if (delta_state_add_match(state, match->original_offset,
							  match->new_offset, match->length) == 0) {
					match_count++;
					if (match_count <= 10) { // Only show first 10 matches to avoid spam
						printf("  Match %u: original[%u:%u] -> new[%u:%u] (length=%u)\n",
						       match_count, match->original_offset,
						       match->original_offset + match->length - 1,
						       match->new_offset, match->new_offset + match->length - 1,
						       match->length);
					}
				}

				// Skip ahead by match length
				i += match->length - 1; // -1 because loop will increment i
			} else {
				// Skip small matches that don't provide compression benefit
				skipped_small_matches++;
				// Don't skip ahead - let the algorithm treat this as an insert
			}
			free(match);
		}

		// Progress reporting
		if (i % match_progress_interval == 0) {
			uint32_t progress_percent = (uint32_t)((i * 100ULL) / new_size);
			printf("\rFinding matches: %u%% (%u/%u bytes) - Found %u matches, skipped %u small",
			       progress_percent, i, new_size, match_count, skipped_small_matches);
			fflush(stdout);
		}
	}

	// Final progress report
	printf("\rFinding matches: 100%% (%u/%u bytes) - Found %u matches, skipped %u small\n",
	       new_size, new_size, match_count, skipped_small_matches);

	rolling_hash_free(match_rh);

	printf("Match finding completed - Used %u beneficial matches, skipped %u small matches\n",
	       match_count, skipped_small_matches);

	// Step 3: Create delta operations
	printf("Creating delta operations...\n");
	DeltaInfo *delta = create_delta_operations(original_data, original_size,
						   new_data, new_size, state);

	if (delta != NULL) {
		printf("Delta created with %u operations\n", delta->operation_count);
		printf("Delta size: %u bytes\n", delta->delta_size);

		// Calculate compression ratio
		float compression_ratio = (float)delta->delta_size / new_size * 100.0f;
		printf("Compression ratio: %.1f%%\n", compression_ratio);
	}

	// Cleanup
	delta_state_free(state);
	hash_table_free(ht);

	return delta;
}

/**
 * Free delta info and all its operations
 */
void delta_free(DeltaInfo *delta)
{
	if (delta == NULL)
		return;

	for (uint32_t i = 0; i < delta->operation_count; i++)
		if (delta->operations[i].data != NULL)
			free(delta->operations[i].data);

	free(delta->operations);
	free(delta);
}

/**
 * Print delta information for debugging
 */
void print_delta_info(const DeltaInfo *delta)
{
	if (delta == NULL) {
		printf("Delta is NULL\n");
		return;
	}

	printf("\n=== Delta Information ===\n");
	printf("Original size: %u bytes\n", delta->original_size);
	printf("New size: %u bytes\n", delta->new_size);
	printf("Operation count: %u\n", delta->operation_count);
	printf("Delta size: %u bytes\n", delta->delta_size);
	printf("Compression ratio: %.1f%%\n",
	       (float)delta->delta_size / delta->new_size * 100.0f);

	printf("\nOperations:\n");
	for (uint32_t i = 0; i < delta->operation_count; i++) {
		const DeltaOperation *op = &delta->operations[i];

		switch (op->type) {
		case DELTA_COPY:
			printf("  %u: COPY original[%u:%u] (length=%u)\n",
			       i, op->offset, op->offset + op->length - 1, op->length);
			break;
		case DELTA_INSERT:
			printf("  %u: INSERT %u bytes: ", i, op->length);
			// Print first few bytes as hex
			for (uint32_t j = 0; j < op->length && j < 16; j++)
				printf("%02X ", op->data[j]);
			if (op->length > 16)
				printf("...");
			printf("\n");
			break;
		case DELTA_REPLACE:
			printf("  %u: REPLACE original[%u:%u] with %u bytes\n",
			       i, op->offset, op->offset + op->length - 1, op->length);
			break;
		}
	}
}
