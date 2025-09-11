#include "delta_structures.h"
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

HashTable * hash_table_new(uint32_t bucket_count)
{
	HashTable *ht = malloc(sizeof(HashTable));

	if (ht == NULL) {
		printf("Failed to allocate memory for HashTable: %s\n", strerror(errno));
		return NULL;
	}

	ht->bucket_count = bucket_count;
	ht->entry_count = 0;

	ht->buckets = calloc(bucket_count, sizeof(HashEntry *));
	if (ht->buckets == NULL) {
		printf("Failed to allocate memory for buckets:  %s\n", strerror(errno));
		free(ht);
		ht = NULL;

		return NULL;
	}

	return ht;
}

/**
 * Returns the first matching entry for the given hash.
 * If multiple entries have the same hash, traverse the returned entry's chain
 * to find all matches.
 *
 * @param ht Pointer to the hash table
 * @param hash The hash value to search for
 * @return Pointer to the first matching HashEntry, or NULL if not found
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

// Helper functions
int get_bucket_index(HashTable *ht, uint32_t hash)
{
	if (ht == NULL)
		return -1;

	return hash % ht->bucket_count;
}
