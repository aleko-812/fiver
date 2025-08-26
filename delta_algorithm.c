#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "delta_structures.h"

// Forward declarations
RollingHash* rolling_hash_new(uint32_t window_size);
void rolling_hash_update(RollingHash* rh, uint8_t byte);
uint32_t rolling_hash_get_hash(RollingHash* rh);
void rolling_hash_free(RollingHash* rh);

HashTable* hash_table_new(uint32_t bucket_count);
void hash_table_insert(HashTable* ht, uint32_t hash, uint32_t offset);
HashEntry* hash_table_find(HashTable* ht, uint32_t hash);
void hash_table_free(HashTable* ht);

/**
 * Represents a match found between original and new file
 */
typedef struct {
    uint32_t original_offset;  // Position in original file
    uint32_t new_offset;       // Position in new file
    uint32_t length;           // Length of the match
} Match;

/**
 * Represents the current state of delta generation
 */
typedef struct {
    uint32_t new_pos;          // Current position in new file
    uint32_t original_pos;     // Current position in original file
    uint32_t match_count;      // Number of matches found
    Match* matches;            // Array of matches
    uint32_t matches_capacity; // Capacity of matches array
} DeltaState;

/**
 * Initialize delta state
 */
DeltaState* delta_state_new(uint32_t initial_capacity) {
    DeltaState* state = malloc(sizeof(DeltaState));
    if (state == NULL) return NULL;

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
int delta_state_add_match(DeltaState* state, uint32_t original_offset,
                         uint32_t new_offset, uint32_t length) {
    if (state == NULL) return -1;

    // Resize if needed
    if (state->match_count >= state->matches_capacity) {
        uint32_t new_capacity = state->matches_capacity * 2;
        Match* new_matches = realloc(state->matches, new_capacity * sizeof(Match));
        if (new_matches == NULL) return -1;

        state->matches = new_matches;
        state->matches_capacity = new_capacity;
    }

    // Add match
    state->matches[state->match_count].original_offset = original_offset;
    state->matches[state->match_count].new_offset = new_offset;
    state->matches[state->match_count].length = length;
    state->match_count++;

    return 0;
}

/**
 * Free delta state
 */
void delta_state_free(DeltaState* state) {
    if (state == NULL) return;
    free(state->matches);
    free(state);
}

/**
 * Verify that a match is actually valid by comparing bytes
 */
int verify_match(const uint8_t* original_data, uint32_t original_size,
                 const uint8_t* new_data, uint32_t new_size,
                 uint32_t original_offset, uint32_t new_offset, uint32_t length) {
    // Check bounds
    if (original_offset + length > original_size ||
        new_offset + length > new_size) {
        return 0;
    }

    // Compare actual bytes
    return memcmp(original_data + original_offset,
                  new_data + new_offset, length) == 0;
}

/**
 * Find the best match for a given position in the new file
 */
Match* find_best_match(const uint8_t* original_data, uint32_t original_size,
                       const uint8_t* new_data, uint32_t new_size,
                       const HashTable* ht, uint32_t window_size,
                       uint32_t new_pos, uint32_t min_match_length) {
    if (new_pos + window_size > new_size) return NULL;

    // Calculate hash for current window in new file
    RollingHash* rh = rolling_hash_new(window_size);
    if (rh == NULL) return NULL;

    // Fill the rolling hash with the window at new_pos
    for (uint32_t i = 0; i < window_size; i++) {
        rolling_hash_update(rh, new_data[new_pos + i]);
    }

    uint32_t hash = rolling_hash_get_hash(rh);

    // Look for matches in original file
    HashEntry* match_entry = hash_table_find((HashTable*)ht, hash);

    Match* best_match = NULL;
    uint32_t best_length = 0;

    // Check all matches with this hash
    HashEntry* current = match_entry;
    while (current != NULL) {
        if (current->hash == hash) {
            uint32_t original_offset = current->offset;

            // Try to extend the match beyond the window
            uint32_t match_length = window_size;

            // Extend forward as long as bytes match
            while (new_pos + match_length < new_size &&
                   original_offset + match_length < original_size &&
                   new_data[new_pos + match_length] == original_data[original_offset + match_length]) {
                match_length++;
            }

            // Only consider matches that meet minimum length
            if (match_length >= min_match_length && match_length > best_length) {
                // Verify the match is valid
                if (verify_match(original_data, original_size, new_data, new_size,
                                original_offset, new_pos, match_length)) {
                    best_length = match_length;

                    // Create or update best match
                    if (best_match == NULL) {
                        best_match = malloc(sizeof(Match));
                    }
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
DeltaInfo* create_delta_operations(const uint8_t* original_data, uint32_t original_size,
                                  const uint8_t* new_data, uint32_t new_size,
                                  const DeltaState* state) {
    if (state == NULL) return NULL;

    DeltaInfo* delta = malloc(sizeof(DeltaInfo));
    if (delta == NULL) return NULL;

    delta->original_size = original_size;
    delta->new_size = new_size;
    delta->operation_count = 0;
    delta->operations = NULL;
    delta->delta_size = 0;

    // Sort matches by new_offset for processing in order
    // (This is a simple bubble sort - could be optimized)
    if (state->match_count > 1) {
        for (uint32_t i = 0; i < state->match_count - 1; i++) {
            for (uint32_t j = 0; j < state->match_count - i - 1; j++) {
                if (state->matches[j].new_offset > state->matches[j + 1].new_offset) {
                    Match temp = state->matches[j];
                    state->matches[j] = state->matches[j + 1];
                    state->matches[j + 1] = temp;
                }
            }
        }
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
        Match* match = &state->matches[i];

        // Add INSERT operation for any data before this match
        if (match->new_offset > current_new_pos) {
            uint32_t insert_length = match->new_offset - current_new_pos;

            // Resize operations array if needed
            if (delta->operation_count >= operations_capacity) {
                operations_capacity *= 2;
                DeltaOperation* new_ops = realloc(delta->operations,
                                                 operations_capacity * sizeof(DeltaOperation));
                if (new_ops == NULL) {
                    // Handle error
                    for (uint32_t j = 0; j < delta->operation_count; j++) {
                        if (delta->operations[j].data != NULL) {
                            free(delta->operations[j].data);
                        }
                    }
                    free(delta->operations);
                    free(delta);
                    return NULL;
                }
                delta->operations = new_ops;
            }

            // Create INSERT operation
            DeltaOperation* op = &delta->operations[delta->operation_count];
            op->type = DELTA_INSERT;
            op->offset = 0; // Not used for INSERT
            op->length = insert_length;
            op->data = malloc(insert_length);

            if (op->data == NULL) {
                // Handle error
                for (uint32_t j = 0; j < delta->operation_count; j++) {
                    if (delta->operations[j].data != NULL) {
                        free(delta->operations[j].data);
                    }
                }
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
            DeltaOperation* new_ops = realloc(delta->operations,
                                             operations_capacity * sizeof(DeltaOperation));
            if (new_ops == NULL) {
                // Handle error
                for (uint32_t j = 0; j < delta->operation_count; j++) {
                    if (delta->operations[j].data != NULL) {
                        free(delta->operations[j].data);
                    }
                }
                free(delta->operations);
                free(delta);
                return NULL;
            }
            delta->operations = new_ops;
        }

        DeltaOperation* op = &delta->operations[delta->operation_count];
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
            DeltaOperation* new_ops = realloc(delta->operations,
                                             operations_capacity * sizeof(DeltaOperation));
            if (new_ops == NULL) {
                // Handle error
                for (uint32_t j = 0; j < delta->operation_count; j++) {
                    if (delta->operations[j].data != NULL) {
                        free(delta->operations[j].data);
                    }
                }
                free(delta->operations);
                free(delta);
                return NULL;
            }
            delta->operations = new_ops;
        }

        DeltaOperation* op = &delta->operations[delta->operation_count];
        op->type = DELTA_INSERT;
        op->offset = 0;
        op->length = insert_length;
        op->data = malloc(insert_length);

        if (op->data == NULL) {
            // Handle error
            for (uint32_t j = 0; j < delta->operation_count; j++) {
                if (delta->operations[j].data != NULL) {
                    free(delta->operations[j].data);
                }
            }
            free(delta->operations);
            free(delta);
            return NULL;
        }

        memcpy(op->data, new_data + current_new_pos, insert_length);
        delta->operation_count++;
        delta->delta_size += insert_length;
    }

    return delta;
}

/**
 * Main delta creation function
 */
DeltaInfo* delta_create(const uint8_t* original_data, uint32_t original_size,
                       const uint8_t* new_data, uint32_t new_size) {
    if (original_data == NULL || new_data == NULL) return NULL;

    // Algorithm parameters
    uint32_t window_size = 8;  // Sliding window size
    uint32_t min_match_length = 8;  // Minimum match length to consider
    uint32_t bucket_count = 1024;  // Hash table size

    printf("Creating delta...\n");
    printf("Original size: %u bytes\n", original_size);
    printf("New size: %u bytes\n", new_size);
    printf("Window size: %u bytes\n", window_size);
    printf("Min match length: %u bytes\n", min_match_length);

    // Step 1: Build hash table from original file
    HashTable* ht = hash_table_new(bucket_count);
    if (ht == NULL) {
        printf("Failed to create hash table\n");
        return NULL;
    }

    printf("Building hash table from original file...\n");
    RollingHash* rh = rolling_hash_new(window_size);
    if (rh == NULL) {
        hash_table_free(ht);
        return NULL;
    }

    // Process original file with sliding window
    for (uint32_t i = 0; i < original_size; i++) {
        rolling_hash_update(rh, original_data[i]);

        if (i >= window_size - 1) {
            uint32_t hash = rolling_hash_get_hash(rh);
            uint32_t offset = i - window_size + 1;
            hash_table_insert(ht, hash, offset);
        }
    }

    printf("Hash table built with %u entries\n", ht->entry_count);
    rolling_hash_free(rh);

    // Step 2: Find matches in new file
    printf("Finding matches in new file...\n");
    DeltaState* state = delta_state_new(100);
    if (state == NULL) {
        hash_table_free(ht);
        return NULL;
    }

    uint32_t match_count = 0;

    // Process new file with sliding window
    for (uint32_t i = 0; i < new_size; i++) {
        // Find best match starting at position i
        Match* match = find_best_match(original_data, original_size,
                                      new_data, new_size, ht, window_size,
                                      i, min_match_length);

        if (match != NULL) {
            // Add match to state
            if (delta_state_add_match(state, match->original_offset,
                                     match->new_offset, match->length) == 0) {
                match_count++;
                printf("  Match %u: original[%u:%u] -> new[%u:%u] (length=%u)\n",
                       match_count, match->original_offset,
                       match->original_offset + match->length - 1,
                       match->new_offset, match->new_offset + match->length - 1,
                       match->length);
            }

            // Skip ahead by match length
            i += match->length - 1;  // -1 because loop will increment i
            free(match);
        }
    }

    printf("Found %u matches\n", match_count);

    // Step 3: Create delta operations
    printf("Creating delta operations...\n");
    DeltaInfo* delta = create_delta_operations(original_data, original_size,
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
void delta_free(DeltaInfo* delta) {
    if (delta == NULL) return;

    for (uint32_t i = 0; i < delta->operation_count; i++) {
        if (delta->operations[i].data != NULL) {
            free(delta->operations[i].data);
        }
    }

    free(delta->operations);
    free(delta);
}

/**
 * Print delta information for debugging
 */
void print_delta_info(const DeltaInfo* delta) {
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
        const DeltaOperation* op = &delta->operations[i];

        switch (op->type) {
            case DELTA_COPY:
                printf("  %u: COPY original[%u:%u] (length=%u)\n",
                       i, op->offset, op->offset + op->length - 1, op->length);
                break;
            case DELTA_INSERT:
                printf("  %u: INSERT %u bytes: ", i, op->length);
                // Print first few bytes as hex
                for (uint32_t j = 0; j < op->length && j < 16; j++) {
                    printf("%02X ", op->data[j]);
                }
                if (op->length > 16) printf("...");
                printf("\n");
                break;
            case DELTA_REPLACE:
                printf("  %u: REPLACE original[%u:%u] with %u bytes\n",
                       i, op->offset, op->offset + op->length - 1, op->length);
                break;
        }
    }
}
