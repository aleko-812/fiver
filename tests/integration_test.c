#include <stdio.h>
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
 * Simulates building a hash table from an "original" file
 * This represents step 1 of the delta algorithm
 */
void build_hash_table_from_original(const uint8_t* original_data, size_t original_size,
                                   HashTable* ht, uint32_t window_size) {
    printf("Building hash table from original file (%zu bytes)...\n", original_size);

    RollingHash* rh = rolling_hash_new(window_size);
    if (rh == NULL) {
        printf("Failed to create rolling hash\n");
        return;
    }

    // Process the original file with sliding window
    for (size_t i = 0; i < original_size; i++) {
        // Add current byte to rolling hash
        rolling_hash_update(rh, original_data[i]);

        // Once window is full, we can start generating hashes
        if (i >= window_size - 1) {
            uint32_t hash = rolling_hash_get_hash(rh);
            uint32_t offset = i - window_size + 1;  // Start position of this window

            // Store in hash table
            hash_table_insert(ht, hash, offset);

            if (i < window_size + 10) {  // Show first few hashes
                printf("  Window at offset %u: hash=%u\n", offset, hash);
            }
        }
    }

    printf("Hash table built with %u entries\n", ht->entry_count);
    rolling_hash_free(rh);
}

/**
 * Simulates finding matches in the original file for a "new" file
 * This represents step 2 of the delta algorithm
 */
void find_matches_in_original(const uint8_t* new_data, size_t new_size,
                             const HashTable* ht, uint32_t window_size) {
    printf("\nFinding matches in original file for new file (%zu bytes)...\n", new_size);

    RollingHash* rh = rolling_hash_new(window_size);
    if (rh == NULL) {
        printf("Failed to create rolling hash\n");
        return;
    }

    int total_matches = 0;

    // Process the new file with sliding window
    for (size_t i = 0; i < new_size; i++) {
        // Add current byte to rolling hash
        rolling_hash_update(rh, new_data[i]);

        // Once window is full, we can start looking for matches
        if (i >= window_size - 1) {
            uint32_t hash = rolling_hash_get_hash(rh);
            uint32_t new_offset = i - window_size + 1;  // Start position in new file

            // Look for matches in original file
            HashEntry* match = hash_table_find((HashTable*)ht, hash);
            if (match != NULL) {
                printf("  Match found at new_offset=%u (hash=%u):\n", new_offset, hash);

                // Find all matches with this hash
                HashEntry* current = match;
                int match_count = 0;
                while (current != NULL) {
                    if (current->hash == hash) {
                        printf("    Original offset=%u\n", current->offset);
                        match_count++;
                    }
                    current = current->next;
                }
                printf("    Total matches for this pattern: %d\n", match_count);
                total_matches += match_count;
            }
        }
    }

    printf("Total pattern matches found: %d\n", total_matches);
    rolling_hash_free(rh);
}

/**
 * Demonstrates the complete integration workflow
 */
void test_integration_workflow() {
    printf("=== Rolling Hash + Hash Table Integration Test ===\n");
    printf("This simulates the core of the delta compression algorithm\n\n");

    // Test data: "original" and "new" files
    const char* original_text = "Hello World Hello Again Hello";
    const char* new_text = "Hello World Hello New Hello";

    size_t original_size = strlen(original_text);
    size_t new_size = strlen(new_text);

    printf("Original file: \"%s\" (%zu bytes)\n", original_text, original_size);
    printf("New file: \"%s\" (%zu bytes)\n", new_text, new_size);
    printf("\n");

    // Step 1: Build hash table from original file
    uint32_t window_size = 5;  // 5-byte sliding window
    uint32_t bucket_count = 16;  // Hash table size

    HashTable* ht = hash_table_new(bucket_count);
    if (ht == NULL) {
        printf("Failed to create hash table\n");
        return;
    }

    build_hash_table_from_original((const uint8_t*)original_text, original_size, ht, window_size);

    // Step 2: Find matches in original for new file
    find_matches_in_original((const uint8_t*)new_text, new_size, ht, window_size);

    // Step 3: Show hash table statistics
    printf("\nHash table statistics:\n");
    printf("  Buckets: %u\n", ht->bucket_count);
    printf("  Total entries: %u\n", ht->entry_count);
    printf("  Load factor: %.2f\n", (float)ht->entry_count / ht->bucket_count);

    // Show bucket distribution
    printf("\nBucket distribution:\n");
    for (uint32_t i = 0; i < ht->bucket_count; i++) {
        HashEntry* current = ht->buckets[i];
        int count = 0;
        while (current != NULL) {
            count++;
            current = current->next;
        }
        if (count > 0) {
            printf("  Bucket %u: %d entries\n", i, count);
        }
    }

    // Cleanup
    hash_table_free(ht);
    printf("\n✓ Integration test completed!\n");
}

/**
 * Tests with binary data to show it works with any file type
 */
void test_binary_integration() {
    printf("\n=== Binary Data Integration Test ===\n");
    printf("Testing with binary data (simulating .pdf, .docx, etc.)\n\n");

    // Create some binary test data
    uint8_t original_binary[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x57, 0x6F, 0x72, 0x6C, 0x64};
    uint8_t new_binary[] = {0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x4E, 0x65, 0x77, 0x20, 0x57, 0x6F, 0x72, 0x6C, 0x64};

    size_t original_size = sizeof(original_binary);
    size_t new_size = sizeof(new_binary);

    printf("Original binary: %zu bytes\n", original_size);
    printf("New binary: %zu bytes\n", new_size);
    printf("\n");

    // Build hash table from original binary
    uint32_t window_size = 4;  // 4-byte sliding window for binary
    uint32_t bucket_count = 8;

    HashTable* ht = hash_table_new(bucket_count);
    if (ht == NULL) {
        printf("Failed to create hash table\n");
        return;
    }

    build_hash_table_from_original(original_binary, original_size, ht, window_size);
    find_matches_in_original(new_binary, new_size, ht, window_size);

    hash_table_free(ht);
    printf("\n✓ Binary integration test completed!\n");
}

int main() {
    printf("Rolling Hash + Hash Table Integration Test Suite\n");
    printf("===============================================\n\n");

    test_integration_workflow();
    test_binary_integration();

    printf("\n🎉 All integration tests completed!\n");
    printf("\nThis demonstrates how your delta algorithm will work:\n");
    printf("1. Build hash table from original file using rolling hash\n");
    printf("2. Process new file with rolling hash to find matching patterns\n");
    printf("3. Use matches to create delta operations (COPY/INSERT/REPLACE)\n");

    return 0;
}
