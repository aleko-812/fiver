#include <stdio.h>
#include <string.h>
#include "delta_structures.h"

// Forward declarations
DeltaInfo* delta_create(const uint8_t* original_data, uint32_t original_size,
                       const uint8_t* new_data, uint32_t new_size);
void delta_free(DeltaInfo* delta);
void print_delta_info(const DeltaInfo* delta);

/**
 * Test delta creation with text files
 */
void test_text_delta() {
    printf("=== Text File Delta Test ===\n");

    const char* original_text = "Hello World Hello Again Hello";
    const char* new_text = "Hello World Hello New Hello";

    size_t original_size = strlen(original_text);
    size_t new_size = strlen(new_text);

    printf("Original: \"%s\" (%zu bytes)\n", original_text, original_size);
    printf("New: \"%s\" (%zu bytes)\n", new_text, new_size);
    printf("\n");

    DeltaInfo* delta = delta_create((const uint8_t*)original_text, original_size,
                                   (const uint8_t*)new_text, new_size);

    if (delta != NULL) {
        print_delta_info(delta);
        delta_free(delta);
        printf("\n✓ Text delta test completed!\n");
    } else {
        printf("✗ Failed to create delta\n");
    }
}

/**
 * Test delta creation with binary data
 */
void test_binary_delta() {
    printf("\n=== Binary File Delta Test ===\n");

    // Create binary test data
    uint8_t original_binary[] = {
        0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x57, 0x6F, 0x72, 0x6C, 0x64,
        0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x41, 0x67, 0x61, 0x69, 0x6E,
        0x48, 0x65, 0x6C, 0x6C, 0x6F
    };

    uint8_t new_binary[] = {
        0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x57, 0x6F, 0x72, 0x6C, 0x64,
        0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x4E, 0x65, 0x77, 0x20,
        0x48, 0x65, 0x6C, 0x6C, 0x6F
    };

    size_t original_size = sizeof(original_binary);
    size_t new_size = sizeof(new_binary);

    printf("Original binary: %zu bytes\n", original_size);
    printf("New binary: %zu bytes\n", new_size);
    printf("\n");

    DeltaInfo* delta = delta_create(original_binary, original_size,
                                   new_binary, new_size);

    if (delta != NULL) {
        print_delta_info(delta);
        delta_free(delta);
        printf("\n✓ Binary delta test completed!\n");
    } else {
        printf("✗ Failed to create delta\n");
    }
}

/**
 * Test delta creation with minimal changes
 */
void test_minimal_changes() {
    printf("\n=== Minimal Changes Test ===\n");

    const char* original_text = "This is a very long text that has minimal changes";
    const char* new_text = "This is a very long text that has minimal changes!";

    size_t original_size = strlen(original_text);
    size_t new_size = strlen(new_text);

    printf("Original: \"%s\" (%zu bytes)\n", original_text, original_size);
    printf("New: \"%s\" (%zu bytes)\n", new_text, new_size);
    printf("\n");

    DeltaInfo* delta = delta_create((const uint8_t*)original_text, original_size,
                                   (const uint8_t*)new_text, new_size);

    if (delta != NULL) {
        print_delta_info(delta);
        delta_free(delta);
        printf("\n✓ Minimal changes test completed!\n");
    } else {
        printf("✗ Failed to create delta\n");
    }
}

/**
 * Test delta creation with no common patterns
 */
void test_no_common_patterns() {
    printf("\n=== No Common Patterns Test ===\n");

    const char* original_text = "ABCDEFGHIJKLMNOP";
    const char* new_text = "QRSTUVWXYZ123456";

    size_t original_size = strlen(original_text);
    size_t new_size = strlen(new_text);

    printf("Original: \"%s\" (%zu bytes)\n", original_text, original_size);
    printf("New: \"%s\" (%zu bytes)\n", new_text, new_size);
    printf("\n");

    DeltaInfo* delta = delta_create((const uint8_t*)original_text, original_size,
                                   (const uint8_t*)new_text, new_size);

    if (delta != NULL) {
        print_delta_info(delta);
        delta_free(delta);
        printf("\n✓ No common patterns test completed!\n");
    } else {
        printf("✗ Failed to create delta\n");
    }
}

/**
 * Test delta creation with identical files
 */
void test_identical_files() {
    printf("\n=== Identical Files Test ===\n");

    const char* text = "This file is identical to itself";
    size_t size = strlen(text);

    printf("File: \"%s\" (%zu bytes)\n", text, size);
    printf("\n");

    DeltaInfo* delta = delta_create((const uint8_t*)text, size,
                                   (const uint8_t*)text, size);

    if (delta != NULL) {
        print_delta_info(delta);
        delta_free(delta);
        printf("\n✓ Identical files test completed!\n");
    } else {
        printf("✗ Failed to create delta\n");
    }
}

int main() {
    printf("Delta Algorithm Test Suite\n");
    printf("=========================\n\n");

    test_text_delta();
    test_binary_delta();
    test_minimal_changes();
    test_no_common_patterns();
    test_identical_files();

    printf("\n🎉 All delta algorithm tests completed!\n");
    printf("\nThis demonstrates the complete delta compression workflow:\n");
    printf("1. ✅ Rolling hash for efficient pattern detection\n");
    printf("2. ✅ Hash table for fast pattern lookup\n");
    printf("3. ✅ Delta creation algorithm for compression\n");
    printf("4. ✅ Multiple file type support (text, binary)\n");

    return 0;
}

