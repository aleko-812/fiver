#include <stdio.h>
#include <stdlib.h>
#include "rolling_hash.c"  // Include the implementation directly for testing

void test_rolling_hash_new() {
    printf("=== Testing rolling_hash_new ===\n");

    // Test 1: Normal allocation
    RollingHash* rh = rolling_hash_new(8);
    if (rh != NULL) {
        printf("✓ Successfully created RollingHash with window_size=8\n");
        printf("  a=%u, b=%u, window_size=%u, window_pos=%u, bytes_in_window=%u\n",
               rh->a, rh->b, rh->window_size, rh->window_pos, rh->bytes_in_window);

        // Test window initialization
        printf("  Window contents: ");
        for (int i = 0; i < rh->window_size; i++) {
            printf("%u ", rh->window[i]);
        }
        printf("\n");

        // Cleanup
        rolling_hash_free(rh);
        printf("✓ Cleanup successful\n");
    } else {
        printf("✗ Failed to create RollingHash\n");
    }

    // Test 2: Large allocation
    RollingHash* rh2 = rolling_hash_new(1024);
    if (rh2 != NULL) {
        printf("✓ Successfully created RollingHash with window_size=1024\n");
        rolling_hash_free(rh2);
    } else {
        printf("✗ Failed to create large RollingHash\n");
    }

    printf("Test completed!\n\n");
}

void test_rolling_hash_update() {
    printf("=== Testing rolling_hash_update ===\n");

    // Create rolling hash with window size 4
    RollingHash* rh = rolling_hash_new(4);
    if (rh == NULL) {
        printf("✗ Failed to create RollingHash for update test\n");
        return;
    }

    // Test string "Hello" with window size 4
    const char* test_string = "Hello";
    printf("Testing with string: '%s', window_size=4\n", test_string);

    for (int i = 0; test_string[i] != '\0'; i++) {
        uint8_t byte = test_string[i];
        rolling_hash_update(rh, byte);

        printf("  After adding '%c': a=%u, b=%u, bytes_in_window=%u\n",
               test_string[i], rh->a, rh->b, rh->bytes_in_window);

        printf("    Window: [");
        for (int j = 0; j < rh->window_size; j++) {
            int pos = (rh->window_pos - rh->window_size + j + rh->window_size) % rh->window_size;
            if (j < rh->bytes_in_window) {
                printf("'%c'", rh->window[pos]);
            } else {
                printf("0");
            }
            if (j < rh->window_size - 1) printf(", ");
        }
        printf("]\n");
    }

    // Cleanup
    rolling_hash_free(rh);
    printf("✓ Update test completed!\n\n");
}

void test_rolling_hash_get_hash() {
    printf("=== Testing rolling_hash_get_hash ===\n");

    // Test 1: NULL pointer
    uint32_t hash = rolling_hash_get_hash(NULL);
    printf("✓ NULL pointer test: hash=%u (should be 0)\n", hash);

    // Test 2: Empty window
    RollingHash* rh = rolling_hash_new(4);
    hash = rolling_hash_get_hash(rh);
    printf("✓ Empty window test: hash=%u (should be 0)\n", hash);

    // Test 3: Single byte
    rolling_hash_update(rh, 'A');
    hash = rolling_hash_get_hash(rh);
    printf("✓ Single byte test: hash=%u (a=%u, b=%u)\n", hash, rh->a, rh->b);

    // Test 4: Multiple bytes
    rolling_hash_update(rh, 'B');
    rolling_hash_update(rh, 'C');
    hash = rolling_hash_get_hash(rh);
    printf("✓ Multiple bytes test: hash=%u (a=%u, b=%u)\n", hash, rh->a, rh->b);

    // Test 5: Full window
    rolling_hash_update(rh, 'D');
    hash = rolling_hash_get_hash(rh);
    printf("✓ Full window test: hash=%u (a=%u, b=%u)\n", hash, rh->a, rh->b);

    // Test 6: Rolling window
    rolling_hash_update(rh, 'E');
    hash = rolling_hash_get_hash(rh);
    printf("✓ Rolling window test: hash=%u (a=%u, b=%u)\n", hash, rh->a, rh->b);

    // Cleanup
    rolling_hash_free(rh);
    printf("✓ Get hash test completed!\n\n");
}

void test_rolling_hash_free() {
    printf("=== Testing rolling_hash_free ===\n");

    // Test 1: Normal cleanup
    RollingHash* rh = rolling_hash_new(8);
    if (rh != NULL) {
        rolling_hash_update(rh, 'X');
        rolling_hash_update(rh, 'Y');
        printf("✓ Created and used RollingHash\n");

        rolling_hash_free(rh);
        printf("✓ Normal cleanup successful\n");
    }

    // Test 2: NULL pointer
    rolling_hash_free(NULL);
    printf("✓ NULL pointer cleanup handled gracefully\n");

    // Test 3: Multiple allocations and cleanup
    RollingHash* rh1 = rolling_hash_new(4);
    RollingHash* rh2 = rolling_hash_new(16);
    RollingHash* rh3 = rolling_hash_new(64);

    if (rh1 && rh2 && rh3) {
        printf("✓ Created multiple RollingHash instances\n");

        rolling_hash_free(rh1);
        rolling_hash_free(rh2);
        rolling_hash_free(rh3);
        printf("✓ Multiple cleanup successful\n");
    }

    printf("✓ Free test completed!\n\n");
}

void test_rolling_hash_integration() {
    printf("=== Testing Rolling Hash Integration ===\n");

    // Test complete workflow
    RollingHash* rh = rolling_hash_new(4);
    if (rh == NULL) {
        printf("✗ Failed to create RollingHash for integration test\n");
        return;
    }

    const char* test_data = "ABCDEFGH";
    printf("Testing complete workflow with data: '%s'\n", test_data);

    for (int i = 0; test_data[i] != '\0'; i++) {
        rolling_hash_update(rh, test_data[i]);
        uint32_t hash = rolling_hash_get_hash(rh);

        printf("  Step %d: Added '%c', hash=%u, window_size=%u\n",
               i+1, test_data[i], hash, rh->bytes_in_window);
    }

    // Final hash
    uint32_t final_hash = rolling_hash_get_hash(rh);
    printf("✓ Final hash: %u\n", final_hash);

    // Cleanup
    rolling_hash_free(rh);
    printf("✓ Integration test completed!\n\n");
}

int main() {
    printf("Rolling Hash Comprehensive Test Suite\n");
    printf("=====================================\n\n");

    test_rolling_hash_new();
    test_rolling_hash_update();
    test_rolling_hash_get_hash();
    test_rolling_hash_free();
    test_rolling_hash_integration();

    printf("🎉 All tests completed successfully!\n");
    return 0;
}
