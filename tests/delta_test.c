#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "delta_structures.h"

// Simple test to understand delta operations
void test_delta_operations() {
    printf("=== Testing Delta Operations ===\n");

    // Create a simple delta manually
    DeltaInfo* delta = malloc(sizeof(DeltaInfo));
    delta->original_size = 12;  // "Hello World!"
    delta->new_size = 20;       // "Hello Beautiful World!"
    delta->operation_count = 3;
    delta->operations = malloc(3 * sizeof(DeltaOperation));

    // Operation 1: Copy "Hello "
    delta->operations[0].type = DELTA_COPY;
    delta->operations[0].offset = 0;
    delta->operations[0].length = 6;
    delta->operations[0].data = NULL;

    // Operation 2: Insert "Beautiful "
    delta->operations[1].type = DELTA_INSERT;
    delta->operations[1].offset = 0;
    delta->operations[1].length = 10;
    delta->operations[1].data = malloc(10);
    memcpy(delta->operations[1].data, "Beautiful ", 10);

    // Operation 3: Copy "World!"
    delta->operations[2].type = DELTA_COPY;
    delta->operations[2].offset = 6;
    delta->operations[2].length = 6;
    delta->operations[2].data = NULL;

    // Print the delta
    printf("Delta operations:\n");
    for (int i = 0; i < delta->operation_count; i++) {
        DeltaOperation* op = &delta->operations[i];
        switch (op->type) {
            case DELTA_COPY:
                printf("  %d: COPY from offset %u, length %u\n",
                       i, op->offset, op->length);
                break;
            case DELTA_INSERT:
                printf("  %d: INSERT %u bytes: '%.*s'\n",
                       i, op->length, op->length, op->data);
                break;
            case DELTA_REPLACE:
                printf("  %d: REPLACE at offset %u, length %u\n",
                       i, op->offset, op->length);
                break;
        }
    }

    // Test applying the delta
    const char* original = "Hello World!";
    char output[50];

    printf("\nOriginal: '%s'\n", original);

    // Apply delta manually
    int pos = 0;
    for (int i = 0; i < delta->operation_count; i++) {
        DeltaOperation* op = &delta->operations[i];
        switch (op->type) {
            case DELTA_COPY:
                memcpy(output + pos, original + op->offset, op->length);
                pos += op->length;
                break;
            case DELTA_INSERT:
                memcpy(output + pos, op->data, op->length);
                pos += op->length;
                break;
            case DELTA_REPLACE:
                memcpy(output + pos, op->data, op->length);
                pos += op->length;
                break;
        }
    }
    output[pos] = '\0';

    printf("Result: '%s'\n", output);

    // Cleanup
    for (int i = 0; i < delta->operation_count; i++) {
        if (delta->operations[i].data != NULL) {
            free(delta->operations[i].data);
        }
    }
    free(delta->operations);
    free(delta);

    printf("Test completed!\n\n");
}

// Test rolling hash concept
void test_rolling_hash_concept() {
    printf("=== Rolling Hash Concept ===\n");

    // Simple rolling hash demonstration
    const char* text = "Hello World!";
    uint32_t window_size = 4;

    printf("Text: '%s'\n", text);
    printf("Window size: %u\n", window_size);
    printf("Rolling hashes:\n");

    for (int i = 0; i <= strlen(text) - window_size; i++) {
        // Simple hash: sum of characters
        uint32_t hash = 0;
        for (int j = 0; j < window_size; j++) {
            hash += text[i + j];
        }
        printf("  '%c%c%c%c' -> hash: %u\n",
               text[i], text[i+1], text[i+2], text[i+3], hash);
    }

    printf("Rolling hash concept: as we slide the window, we can efficiently\n");
    printf("update the hash by subtracting the old character and adding the new one.\n\n");
}

// Test hash table concept
void test_hash_table_concept() {
    printf("=== Hash Table Concept ===\n");

    // Simple hash table demonstration
    const char* patterns[] = {"Hell", "ello", "llo ", "lo W", "o Wo", " Wor", "Worl", "orld"};
    int pattern_count = 8;

    printf("Building hash table for patterns:\n");
    for (int i = 0; i < pattern_count; i++) {
        uint32_t hash = 0;
        for (int j = 0; j < 4; j++) {
            hash += patterns[i][j];
        }
        printf("  '%s' -> hash: %u -> bucket: %u\n",
               patterns[i], hash, hash % 5);
    }

    printf("\nWhen we find a match in new data, we can quickly look up\n");
    printf("where this pattern appears in the original file.\n\n");
}

int main() {
    printf("Delta Algorithm Structure Test\n");
    printf("==============================\n\n");

    test_delta_operations();
    test_rolling_hash_concept();
    test_hash_table_concept();

    printf("=== Key Concepts Summary ===\n");
    printf("1. Delta operations: COPY, INSERT, REPLACE\n");
    printf("2. Rolling hash: efficient sliding window hashing\n");
    printf("3. Hash table: fast pattern matching\n");
    printf("4. FileBuffer: efficient binary data handling\n\n");

    return 0;
}


