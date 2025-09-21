#include "delta_structures.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

/**
 * @file hash_table.c
 * @brief Hash table implementation for delta compression pattern matching
 *
 * This module provides a hash table implementation optimized for the delta
 * compression algorithm. It uses separate chaining for collision resolution
 * and is designed for efficient pattern matching in large files.
 *
 * @author Fiver Development Team
 * @version 1.0
 */

/**
 * @brief Creates a new hash table with the specified number of buckets
 *
 * Allocates memory for a new hash table and initializes all buckets to NULL.
 * The hash table uses separate chaining for collision resolution, where each
 * bucket contains a linked list of entries with the same hash value.
 *
 * @param bucket_count Number of buckets in the hash table. Must be > 0.
 *                     For optimal performance, use a prime number.
 *
 * @return Pointer to the newly created HashTable on success, NULL on failure.
 *         The caller is responsible for freeing the hash table with hash_table_free().
 *
 * @note Memory allocation failures are logged to stdout with error details.
 *
 * @example
 * ```c
 * HashTable *ht = hash_table_new(1009);  // Use prime number for better distribution
 * if (ht == NULL) {
 *     // Handle allocation failure
 * }
 * ```
 */
HashTable * hash_table_new(uint32_t bucket_count)
{
	// Validate input parameters
	if (bucket_count == 0) {
		printf("Error: bucket_count must be greater than 0\n");
		return NULL;
	}

	HashTable *ht = malloc(sizeof(HashTable));
	if (ht == NULL) {
		printf("Failed to allocate memory for HashTable: %s\n", strerror(errno));
		return NULL;
	}

	ht->bucket_count = bucket_count;
	ht->entry_count = 0;

	ht->buckets = calloc(bucket_count, sizeof(HashEntry *));
	if (ht->buckets == NULL) {
		printf("Failed to allocate memory for buckets: %s\n", strerror(errno));
		free(ht);
		return NULL;
	}

	return ht;
}

/**
 * @brief Finds the first entry with the specified hash value
 *
 * Searches the hash table for an entry with the given hash value. If multiple
 * entries have the same hash (collision), this function returns the first one
 * found. To find all entries with the same hash, traverse the returned entry's
 * chain using the 'next' pointer.
 *
 * @param ht Pointer to the hash table to search. Must not be NULL.
 * @param hash The hash value to search for
 *
 * @return Pointer to the first matching HashEntry on success, NULL if not found
 *         or if ht is NULL or the hash table is empty.
 *
 * @note This function has O(1) average case complexity and O(n) worst case
 *       complexity where n is the number of entries in the bucket.
 *
 * @example
 * ```c
 * HashEntry *entry = hash_table_find(ht, 12345);
 * if (entry != NULL) {
 *     printf("Found entry at offset: %u\n", entry->offset);
 *     // Check for more entries with same hash
 *     while (entry->next != NULL) {
 *         entry = entry->next;
 *         printf("Another entry at offset: %u\n", entry->offset);
 *     }
 * }
 * ```
 */
HashEntry * hash_table_find(HashTable *ht, uint32_t hash)
{
	if (ht == NULL)
		return NULL;

	if (ht->bucket_count == 0)
		return NULL;

	uint32_t bucket_index = hash % ht->bucket_count;
	HashEntry *current_entry = ht->buckets[bucket_index];

	while (current_entry != NULL) {
		if (current_entry->hash == hash)
			return current_entry;
		current_entry = current_entry->next;
	}

	return NULL;
}

/**
 * @brief Inserts a new entry into the hash table
 *
 * Creates a new hash table entry with the specified hash and offset values,
 * then inserts it at the head of the appropriate bucket's chain. This provides
 * O(1) insertion time complexity by avoiding traversal of the entire chain.
 *
 * @param ht Pointer to the hash table. Must not be NULL.
 * @param hash The hash value for the new entry
 * @param offset The file offset associated with this hash value
 *
 * @note This function silently fails if memory allocation fails or if ht is NULL.
 *       The entry_count is incremented only on successful insertion.
 *
 * @note Insertion is performed at the head of the chain for O(1) performance,
 *       which means newer entries with the same hash will be found first by
 *       hash_table_find().
 *
 * @example
 * ```c
 * // Insert a pattern at file offset 1024 with hash 0x12345678
 * hash_table_insert(ht, 0x12345678, 1024);
 * ```
 */
void hash_table_insert(HashTable *ht, uint32_t hash, uint32_t offset)
{
	if (ht == NULL)
		return;
	if (ht->bucket_count == 0)
		return;

	uint32_t bucket_index = hash % ht->bucket_count;

	HashEntry *new_entry = malloc(sizeof(HashEntry));
	if (new_entry == NULL) {
		printf("Failed to allocate memory for HashEntry: %s\n", strerror(errno));
		return;
	}

	new_entry->hash = hash;
	new_entry->offset = offset;
	// Insert at head of chain (O(1) instead of O(n))
	new_entry->next = ht->buckets[bucket_index];
	ht->buckets[bucket_index] = new_entry;

	ht->entry_count++;
}

/**
 * @brief Frees all memory associated with the hash table
 *
 * Recursively frees all hash table entries and the hash table structure itself.
 * This function safely handles NULL pointers and ensures no memory leaks occur.
 * After calling this function, the hash table pointer should not be used again.
 *
 * @param ht Pointer to the hash table to free. Safe to pass NULL.
 *
 * @note This function frees all entries in all buckets, then frees the buckets
 *       array, and finally frees the hash table structure itself.
 *
 * @note After calling this function, the hash table pointer becomes invalid
 *       and should not be dereferenced.
 *
 * @example
 * ```c
 * HashTable *ht = hash_table_new(100);
 * // ... use hash table ...
 * hash_table_free(ht);  // Free all memory
 * // ht is now invalid and should not be used
 * ```
 */
void hash_table_free(HashTable *ht)
{
	if (ht == NULL)
		return;

	// Free all entries in each bucket
	for (uint32_t i = 0; i < ht->bucket_count; i++) {
		HashEntry *current = ht->buckets[i];
		while (current != NULL) {
			HashEntry *next = current->next;
			free(current);
			current = next;
		}
	}

	// Free buckets array and hash table
	free(ht->buckets);
	free(ht);
}

