#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "delta_structures.h"

/**
 * @file delta_algorithm.c
 * @brief Delta compression algorithm implementation for file versioning
 *
 * This module implements a sophisticated three-tier delta compression algorithm
 * that automatically chooses the best compression strategy based on the nature
 * of changes between files. It supports simple end-of-file changes, chunk-based
 * changes, and complex rolling hash-based pattern matching.
 *
 * The algorithm uses:
 * - Simple approach: For small end-of-file changes (95%+ identical)
 * - Chunk-based approach: For small changes anywhere in the file (<1% of file)
 * - Rolling hash algorithm: For complex changes with rsync-like pattern matching
 *
 * @author Fiver Development Team
 * @version 1.0
 */

/**
 * @brief Comparison function for sorting matches by new file offset
 *
 * Used by qsort() to sort matches in ascending order by their position in the
 * new file. This ensures that delta operations are processed in the correct
 * sequence when creating the final delta.
 *
 * @param a Pointer to first Match structure
 * @param b Pointer to second Match structure
 *
 * @return Negative value if a < b, positive if a > b, 0 if equal
 */
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
static Match * match_alloc(void)
{
	return malloc(sizeof(Match));
}

// Forward declarations
RollingHash * rolling_hash_new(uint32_t window_size);
void rolling_hash_update(RollingHash *rh, uint8_t byte);
uint32_t rolling_hash_get(RollingHash *rh);
void rolling_hash_free(RollingHash *rh);

HashTable * hash_table_new(uint32_t bucket_count);
void hash_table_insert(HashTable *ht, uint32_t hash, uint32_t offset);
HashEntry * hash_table_find(HashTable *ht, uint32_t hash);
void hash_table_free(HashTable *ht);

// Match and DeltaState are now defined in delta_structures.h

/**
 * @brief Creates a new delta state for tracking matches during delta creation
 *
 * Allocates and initializes a new DeltaState structure with the specified
 * initial capacity for storing matches. The delta state is used to collect
 * and manage matches found during the pattern matching phase of delta creation.
 *
 * @param initial_capacity Initial capacity for the matches array. Must be > 0.
 *                        The array will be automatically resized as needed.
 *
 * @return Pointer to the newly created DeltaState on success, NULL on failure.
 *         The caller is responsible for freeing the state with delta_state_free().
 *
 * @note Memory allocation failures are handled gracefully and return NULL.
 *
 * @example
 * ```c
 * DeltaState *state = delta_state_new(100);
 * if (state == NULL) {
 *     // Handle allocation failure
 * }
 * ```
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
 * @brief Adds a match to the delta state
 *
 * Adds a new match to the delta state's matches array. The function automatically
 * resizes the matches array if it reaches capacity. Matches represent identical
 * sequences of bytes found in both the original and new files.
 *
 * @param state Pointer to the delta state. Must not be NULL.
 * @param original_offset Offset in the original file where the match starts
 * @param new_offset Offset in the new file where the match starts
 * @param length Length of the matching sequence in bytes
 *
 * @return EXIT_SUCCESS on success, -1 on failure (NULL state or memory allocation failure)
 *
 * @note The function automatically doubles the capacity when the matches array is full.
 *
 * @note Matches are stored in the order they are added and will be sorted later
 *       by new_offset when creating delta operations.
 *
 * @example
 * ```c
 * DeltaState *state = delta_state_new(100);
 * int result = delta_state_add_match(state, 1024, 2048, 64);
 * if (result != EXIT_SUCCESS) {
 *     // Handle error
 * }
 * ```
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
 * @brief Frees all memory associated with the delta state
 *
 * Recursively frees all memory allocated for the delta state structure,
 * including the matches array. This function safely handles NULL pointers
 * and ensures no memory leaks occur.
 *
 * @param state Pointer to the delta state to free. Safe to pass NULL.
 *
 * @note After calling this function, the delta state pointer becomes invalid
 *       and should not be dereferenced.
 *
 * @example
 * ```c
 * DeltaState *state = delta_state_new(100);
 * // ... use delta state ...
 * delta_state_free(state);  // Free all memory
 * // state is now invalid and should not be used
 * ```
 */
void delta_state_free(DeltaState *state)
{
	if (state == NULL)
		return;
	free(state->matches);
	free(state);
}

/**
 * @brief Verifies that a match is actually valid by comparing bytes
 *
 * Performs a byte-by-byte comparison to verify that the proposed match
 * is actually valid. This is used as a safety check to ensure that hash
 * collisions don't result in false matches.
 *
 * @param original_data Pointer to the original file data
 * @param original_size Size of the original file in bytes
 * @param new_data Pointer to the new file data
 * @param new_size Size of the new file in bytes
 * @param original_offset Offset in original file where match should start
 * @param new_offset Offset in new file where match should start
 * @param length Length of the proposed match in bytes
 *
 * @return 1 if the match is valid (bytes are identical), 0 if invalid or out of bounds
 *
 * @note This function performs bounds checking to prevent buffer overflows.
 *
 * @note The function uses memcmp() for efficient byte comparison.
 *
 * @example
 * ```c
 * int is_valid = verify_match(orig_data, orig_size, new_data, new_size, 1024, 2048, 64);
 * if (is_valid) {
 *     // Match is confirmed valid
 * }
 * ```
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

	uint32_t hash = rolling_hash_get(rh);

	// Look for matches in original file
	HashEntry *match_entry = hash_table_find((HashTable *)ht, hash);

	Match *best_match = NULL;
	uint32_t best_length = 0;

	// Check all matches with this hash (simplified approach)
	HashEntry *current = match_entry;
	uint32_t max_candidates = 20; // Allow more candidates for better matches
	uint32_t candidates_checked = 0;

	while (current != NULL && candidates_checked < max_candidates) {
		if (current->hash == hash) {
			uint32_t original_offset = current->offset;
			candidates_checked++;

			// Try to extend the match beyond the window
			uint32_t match_length = window_size;

			// Extend forward as long as bytes match (optimized with larger strides)
			// Limit maximum match size to prevent extremely long matches
			uint32_t max_match_size = 1024 * 1024; // 1MB maximum match size
			while (new_pos + match_length < new_size &&
			       original_offset + match_length < original_size &&
			       match_length < max_match_size) {
				// Check 8 bytes at a time for even better performance
				if (match_length + 8 <= new_size &&
				    match_length + 8 <= original_size) {
					uint64_t *new_chunk = (uint64_t *)(new_data + new_pos + match_length);
					uint64_t *orig_chunk = (uint64_t *)(original_data + original_offset + match_length);
					if (*new_chunk == *orig_chunk) {
						match_length += 8;
						continue;
					}
				}
				// Check 4 bytes at a time for better performance
				if (match_length + 4 <= new_size &&
				    match_length + 4 <= original_size) {
					uint32_t *new_chunk = (uint32_t *)(new_data + new_pos + match_length);
					uint32_t *orig_chunk = (uint32_t *)(original_data + original_offset + match_length);
					if (*new_chunk == *orig_chunk) {
						match_length += 4;
						continue;
					}
				}
				// Fall back to byte-by-byte for remaining bytes
				if (new_data[new_pos + match_length] ==
				    original_data[original_offset + match_length])
					match_length++;
				else
					break;
			}

			// Only consider matches that meet minimum length
			if (match_length >= min_match_length && match_length > best_length) {
				// Simple approach: just take the longest match
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

	uint32_t hash = rolling_hash_get(rh);

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
	if (state->match_count > 1)
		qsort(state->matches, state->match_count, sizeof(Match), compare_matches);

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
 * @brief Main delta creation function implementing three-tier compression strategy
 *
 * Creates a delta between two files using a sophisticated three-tier approach
 * that automatically chooses the best compression strategy based on the nature
 * of changes. This is the primary entry point for delta compression.
 *
 * The algorithm uses three strategies:
 * 1. Simple approach: For small end-of-file changes (95%+ identical)
 * 2. Chunk-based approach: For small changes anywhere in the file (<1% of file)
 * 3. Rolling hash algorithm: For complex changes with rsync-like pattern matching
 *
 * @param original_data Pointer to the original file data
 * @param original_size Size of the original file in bytes
 * @param new_data Pointer to the new file data
 * @param new_size Size of the new file in bytes
 *
 * @return Pointer to DeltaInfo structure containing the delta operations on success,
 *         NULL on failure. The caller is responsible for freeing the delta with delta_free().
 *
 * @note The function automatically chooses the most efficient compression strategy.
 *
 * @note For large files (>50MB), the algorithm uses more aggressive optimization
 *       to prevent excessive memory usage and processing time.
 *
 * @note The function includes progress reporting and detailed logging for monitoring.
 *
 * @example
 * ```c
 * DeltaInfo *delta = delta_create(orig_data, orig_size, new_data, new_size);
 * if (delta != NULL) {
 *     // Use delta...
 *     delta_free(delta);
 * }
 * ```
 */
DeltaInfo * delta_create(const uint8_t *original_data, uint32_t original_size,
			 const uint8_t *new_data, uint32_t new_size)
{
	if (original_data == NULL || new_data == NULL)
		return NULL;

	// For small changes, use a simple approach
	if (new_size > original_size && (new_size - original_size) < 1000) {
		// Small addition - check if it's just appended data
		uint32_t common_prefix = 0;
		uint32_t min_size = (original_size < new_size) ? original_size : new_size;

		// Find common prefix
		for (uint32_t i = 0; i < min_size; i++) {
			if (original_data[i] == new_data[i])
				common_prefix++;
			else
				break;
		}

		// If most of the file is identical, use simple approach
		if (common_prefix > original_size * 0.95) { // 95% identical
			printf("Detected small change (%.1f%% identical) - using simple approach\n",
			       (common_prefix * 100.0) / original_size);

			DeltaInfo *delta = malloc(sizeof(DeltaInfo));
			if (delta == NULL) return NULL;

			delta->original_size = original_size;
			delta->new_size = new_size;
			delta->operation_count = 2; // COPY + INSERT
			delta->delta_size = new_size - common_prefix;

			delta->operations = malloc(2 * sizeof(DeltaOperation));
			if (delta->operations == NULL) {
				free(delta);
				return NULL;
			}

			// COPY operation for the common prefix
			delta->operations[0].type = DELTA_COPY;
			delta->operations[0].offset = 0;
			delta->operations[0].length = common_prefix;
			delta->operations[0].data = NULL;

			// INSERT operation for the new data
			uint32_t insert_length = new_size - common_prefix;
			delta->operations[1].type = DELTA_INSERT;
			delta->operations[1].offset = 0;
			delta->operations[1].length = insert_length;
			delta->operations[1].data = malloc(insert_length);
			if (delta->operations[1].data == NULL) {
				free(delta->operations);
				free(delta);
				return NULL;
			}
			memcpy(delta->operations[1].data, new_data + common_prefix, insert_length);

			printf("Simple delta: COPY %u bytes + INSERT %u bytes\n", common_prefix, insert_length);
			return delta;
		}
	}

	// For any file, try to find large matching chunks to avoid huge deltas
	// This handles changes in the middle of files
	uint32_t total_identical_bytes = 0;
	uint32_t min_size = (original_size < new_size) ? original_size : new_size;

	// Find common prefix
	uint32_t common_prefix = 0;
	for (uint32_t i = 0; i < min_size; i++) {
		if (original_data[i] == new_data[i])
			common_prefix++;
		else
			break;
	}
	total_identical_bytes += common_prefix;

	// Find common suffix (from the end)
	uint32_t common_suffix = 0;
	uint32_t orig_end = original_size;
	uint32_t new_end = new_size;
	while (orig_end > common_prefix && new_end > common_prefix &&
	       original_data[orig_end - 1] == new_data[new_end - 1]) {
		common_suffix++;
		orig_end--;
		new_end--;
	}
	total_identical_bytes += common_suffix;

	// If we have a large amount of identical content, use chunk-based approach
	// Also trigger for small changes regardless of percentage
	uint32_t change_size = (new_size > original_size) ? (new_size - original_size) : (original_size - new_size);
	int change_size_less_than_1_percent = change_size < original_size * 0.01;
	if (total_identical_bytes > original_size * 0.8 || change_size_less_than_1_percent) {
		if (change_size_less_than_1_percent)
			printf("Detected small change (%u bytes, %.3f%% of file) - using chunk-based approach\n",
			       change_size, (change_size * 100.0) / original_size);
		else
			printf("Detected large matching chunks (%.1f%% identical) - using chunk-based approach\n",
			       (total_identical_bytes * 100.0) / original_size);

		DeltaInfo *delta = malloc(sizeof(DeltaInfo));
		if (delta == NULL) return NULL;

		delta->original_size = original_size;
		delta->new_size = new_size;
		delta->operation_count = 0;
		delta->delta_size = 0;
		delta->operations = NULL;

		// Allocate operations array (we'll need at most 4 operations: COPY + INSERT + COPY)
		uint32_t operations_capacity = 4;
		delta->operations = malloc(operations_capacity * sizeof(DeltaOperation));
		if (delta->operations == NULL) {
			free(delta);
			return NULL;
		}

		// COPY operation for the common prefix
		if (common_prefix > 0) {
			delta->operations[delta->operation_count].type = DELTA_COPY;
			delta->operations[delta->operation_count].offset = 0;
			delta->operations[delta->operation_count].length = common_prefix;
			delta->operations[delta->operation_count].data = NULL;
			delta->operation_count++;
		}

		// INSERT operation for the middle part (if any)
		uint32_t middle_start = common_prefix;
		uint32_t middle_end = new_size - common_suffix;
		if (middle_start < middle_end) {
			uint32_t insert_length = middle_end - middle_start;
			delta->operations[delta->operation_count].type = DELTA_INSERT;
			delta->operations[delta->operation_count].offset = 0;
			delta->operations[delta->operation_count].length = insert_length;
			delta->operations[delta->operation_count].data = malloc(insert_length);
			if (delta->operations[delta->operation_count].data == NULL) {
				// Cleanup on error
				for (uint32_t i = 0; i < delta->operation_count; i++)
					if (delta->operations[i].data != NULL)
						free(delta->operations[i].data);
				free(delta->operations);
				free(delta);
				return NULL;
			}
			memcpy(delta->operations[delta->operation_count].data, new_data + middle_start, insert_length);
			delta->delta_size += insert_length;
			delta->operation_count++;
		}

		// COPY operation for the common suffix
		if (common_suffix > 0) {
			delta->operations[delta->operation_count].type = DELTA_COPY;
			delta->operations[delta->operation_count].offset = original_size - common_suffix;
			delta->operations[delta->operation_count].length = common_suffix;
			delta->operations[delta->operation_count].data = NULL;
			delta->operation_count++;
		}

		printf("Chunk-based delta: %u operations, %u bytes\n", delta->operation_count, delta->delta_size);
		return delta;
	}

	// Algorithm parameters for complex changes
	uint32_t window_size = 32;      // Sliding window size (increased for better performance)
	uint32_t min_match_length = 32; // Minimum match length to consider (increased to reduce noise)
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
			uint32_t hash = rolling_hash_get(rh);
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
	if (new_size > 50 * 1024 * 1024)                // 50MB+
		min_beneficial_match_length = 32;       // More reasonable for large files
	else if (new_size > 10 * 1024 * 1024)           // 10MB+
		min_beneficial_match_length = 16;       // More reasonable for medium files
	uint32_t skipped_small_matches = 0;

	printf("Using minimum beneficial match length: %u bytes (file size: %u bytes)\n",
	       min_beneficial_match_length, new_size);

	// Process new file with sliding window
	uint32_t i = 0;
	uint32_t last_match_end = 0; // Track the end of the last match to prevent overlaps
	while (i < new_size) {
		// Skip positions that are already covered by previous matches
		if (i < last_match_end) {
			i++;
			continue;
		}

		// Find best match starting at position i
		Match *match = find_best_match_optimized(original_data, original_size,
							 new_data, new_size, ht, window_size,
							 i, min_match_length, match_rh);

		if (match != NULL) {
			// Cost-benefit analysis: only use matches that provide real compression benefit
			if (match->length >= min_beneficial_match_length) {
				// Check if this match overlaps with the previous match
				if (match->new_offset >= last_match_end) {
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

					// Update last match end and skip ahead by match length
					last_match_end = match->new_offset + match->length;
					i += match->length;
				} else {
					// Match overlaps with previous match, skip it
					skipped_small_matches++;
					i++;
				}
			} else {
				// Skip small matches that don't provide compression benefit
				skipped_small_matches++;
				// Move forward by 1 byte for small matches
				i++;
			}
			free(match);
		} else {
			// No match found, move forward by 1 byte
			i++;
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

	// If we have very few matches, try a more lenient approach
	if (match_count < 10 && new_size > 1024 * 1024) { // Less than 10 matches for files > 1MB
		printf("Too few matches found, trying more lenient approach...\n");

		// Reset and try again with more lenient parameters
		delta_state_free(state);
		state = delta_state_new(1000);

		// Create a new rolling hash for the lenient approach
		RollingHash *lenient_rh = rolling_hash_new(window_size);
		if (lenient_rh == NULL) {
			printf("Failed to create rolling hash for lenient approach\n");
			// Continue with original state
		} else {
			// Try again with lower minimum beneficial match length
			uint32_t lenient_min_beneficial = 32; // More lenient
			uint32_t lenient_match_count = 0;
			uint32_t lenient_skipped = 0;

			// Process new file with more lenient parameters
			uint32_t lenient_i = 0;
			uint32_t lenient_last_match_end = 0;

			while (lenient_i < new_size) {
				// Skip positions that are already covered by previous matches
				if (lenient_i < lenient_last_match_end) {
					lenient_i++;
					continue;
				}

				// Find best match starting at position i
				Match *match = find_best_match_optimized(original_data, original_size,
									 new_data, new_size, ht, window_size,
									 lenient_i, min_match_length, lenient_rh);

				if (match != NULL) {
					// More lenient cost-benefit analysis
					if (match->length >= lenient_min_beneficial) {
						// Check if this match overlaps with the previous match
						if (match->new_offset >= lenient_last_match_end) {
							// Add match to state
							if (delta_state_add_match(state, match->original_offset,
										  match->new_offset, match->length) == 0) {
								lenient_match_count++;
								if (lenient_match_count <= 10) {
									printf("  Lenient Match %u: original[%u:%u] -> new[%u:%u] (length=%u)\n",
									       lenient_match_count, match->original_offset,
									       match->original_offset + match->length - 1,
									       match->new_offset, match->new_offset + match->length - 1,
									       match->length);
								}
							}

							// Update last match end and skip ahead by match length
							lenient_last_match_end = match->new_offset + match->length;
							lenient_i += match->length;
						} else {
							// Match overlaps with previous match, skip it
							lenient_skipped++;
							lenient_i++;
						}
					} else {
						// Skip small matches that don't provide compression benefit
						lenient_skipped++;
						lenient_i++;
					}
					free(match);
				} else {
					// No match found, move forward by 1 byte
					lenient_i++;
				}
			}

			printf("Lenient approach found %u matches, skipped %u small\n",
			       lenient_match_count, lenient_skipped);

			// Use the lenient results if they're better
			if (lenient_match_count > match_count) {
				match_count = lenient_match_count;
				skipped_small_matches = lenient_skipped;
			}

			rolling_hash_free(lenient_rh);
		}
	}

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
 * @brief Frees all memory associated with a delta and its operations
 *
 * Recursively frees all memory allocated for the DeltaInfo structure,
 * including all delta operations and their associated data. This function
 * safely handles NULL pointers and ensures no memory leaks occur.
 *
 * @param delta Pointer to the delta to free. Safe to pass NULL.
 *
 * @note This function frees all operation data arrays and the delta structure itself.
 *
 * @note After calling this function, the delta pointer becomes invalid
 *       and should not be dereferenced.
 *
 * @example
 * ```c
 * DeltaInfo *delta = delta_create(orig_data, orig_size, new_data, new_size);
 * // ... use delta ...
 * delta_free(delta);  // Free all memory
 * // delta is now invalid and should not be used
 * ```
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
 * @brief Prints detailed delta information for debugging and analysis
 *
 * Outputs comprehensive information about a delta including operation counts,
 * sizes, compression ratios, and detailed operation breakdowns. This function
 * is primarily used for debugging and performance analysis.
 *
 * @param delta Pointer to the delta to analyze. Must not be NULL.
 *
 * @note This function outputs to stdout and is intended for debugging purposes.
 *
 * @note The function safely handles NULL deltas by printing an appropriate message.
 *
 * @example
 * ```c
 * DeltaInfo *delta = delta_create(orig_data, orig_size, new_data, new_size);
 * print_delta_info(delta);  // Debug output
 * ```
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
