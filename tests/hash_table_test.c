#include <stdio.h>
#include "delta_structures.h"

void test_hash_table_new() {
  printf("=== Testing hash_table_new ===\n");

  HashTable* ht = hash_table_new(10);
  if (ht != NULL) {
    printf("✓ Successfully created HashTable with bucket_count=10\n");
    printf("  bucket_count=%u, entry_count=%u\n", ht->bucket_count, ht->entry_count);

    // Cleanup
    hash_table_free(ht);
    printf("✓ Cleanup successful\n");
  } else {
    printf("✗ Failed to create HashTable\n");
  }
}

void test_hash_table_insert() {
  printf("=== Testing hash_table_insert ===\n");

  HashTable* ht = hash_table_new(4);  // Small bucket count to test collisions
  if (ht == NULL) {
    printf("✗ Failed to create HashTable for insert test\n");
    return;
  }

  // Test 1: Basic insert
  hash_table_insert(ht, 12345, 100);
  printf("✓ Inserted hash=12345, offset=100\n");
  printf("  entry_count=%u\n", ht->entry_count);

  // Test 2: Another insert
  hash_table_insert(ht, 67890, 200);
  printf("✓ Inserted hash=67890, offset=200\n");
  printf("  entry_count=%u\n", ht->entry_count);

  // Test 3: Collision test (same bucket)
  hash_table_insert(ht, 1, 10);    // bucket = 1 % 4 = 1
  hash_table_insert(ht, 5, 20);    // bucket = 5 % 4 = 1 (collision!)
  printf("✓ Inserted collision entries: hash=1,5 (both in bucket 1)\n");
  printf("  entry_count=%u\n", ht->entry_count);

  // Test 4: Multiple inserts
  for (int i = 0; i < 10; i++) {
    hash_table_insert(ht, i * 1000, i * 100);
  }
  printf("✓ Inserted 10 more entries\n");
  printf("  entry_count=%u\n", ht->entry_count);

  // Cleanup
  hash_table_free(ht);
  printf("✓ Insert test completed!\n");
}

void test_hash_table_find() {
  printf("=== Testing hash_table_find ===\n");

  HashTable* ht = hash_table_new(4);
  if (ht == NULL) {
    printf("✗ Failed to create HashTable for find test\n");
    return;
  }

  // Insert some test data
  hash_table_insert(ht, 12345, 100);
  hash_table_insert(ht, 67890, 200);
  hash_table_insert(ht, 1, 10);
  hash_table_insert(ht, 5, 20);

  // Test 1: Find existing entry
  HashEntry* found = hash_table_find(ht, 12345);
  if (found != NULL && found->offset == 100) {
    printf("✓ Found hash=12345 at offset=%u\n", found->offset);
  } else {
    printf("✗ Failed to find hash=12345\n");
  }

  // Test 2: Find another existing entry
  found = hash_table_find(ht, 67890);
  if (found != NULL && found->offset == 200) {
    printf("✓ Found hash=67890 at offset=%u\n", found->offset);
  } else {
    printf("✗ Failed to find hash=67890\n");
  }

  // Test 3: Find non-existent entry
  found = hash_table_find(ht, 99999);
  if (found == NULL) {
    printf("✓ Correctly returned NULL for non-existent hash=99999\n");
  } else {
    printf("✗ Unexpectedly found non-existent hash=99999\n");
  }

  // Test 4: Test multiple matches (same hash, different offsets)
  hash_table_insert(ht, 12345, 500);  // Same hash, different offset
  printf("✓ Inserted second entry with hash=12345, offset=500\n");

  found = hash_table_find(ht, 12345);
  if (found != NULL) {
    printf("✓ Found first match for hash=12345 at offset=%u\n", found->offset);

    // Traverse to find all matches (traverse entire bucket)
    HashEntry* current = found;
    int match_count = 0;
    while (current != NULL) {
      if (current->hash == 12345) {
        printf("  Match %d: offset=%u\n", ++match_count, current->offset);
      }
      current = current->next;
    }
    printf("✓ Found %d total matches for hash=12345\n", match_count);

    // Verify we found both entries
    if (match_count == 2) {
      printf("✓ Correctly found both entries with same hash\n");
    } else {
      printf("✗ Expected 2 matches, found %d\n", match_count);
    }
  } else {
    printf("✗ Failed to find any matches for hash=12345\n");
  }

  // Test 5: Test collision resolution
  found = hash_table_find(ht, 1);
  if (found != NULL && found->offset == 10) {
    printf("✓ Found hash=1 at offset=%u (collision resolved)\n", found->offset);
  } else {
    printf("✗ Failed to find hash=1\n");
  }

  found = hash_table_find(ht, 5);
  if (found != NULL && found->offset == 20) {
    printf("✓ Found hash=5 at offset=%u (collision resolved)\n", found->offset);
  } else {
    printf("✗ Failed to find hash=5\n");
  }

  // Cleanup
  hash_table_free(ht);
  printf("✓ Find test completed!\n");
}

int main() {
  printf("Hash Table Test Suite\n");
  printf("====================\n\n");

  test_hash_table_new();
  test_hash_table_insert();
  test_hash_table_find();

  printf("🎉 All tests completed!\n");
  return EXIT_SUCCESS;
}
