#ifndef DELTA_STRUCTURES_H
#define DELTA_STRUCTURES_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

// ============================================================================
// Core Data Structures
// ============================================================================

// Represents a single operation in the delta
typedef enum {
	DELTA_COPY,     // Copy bytes from original file
	DELTA_INSERT,   // Insert new bytes
	DELTA_REPLACE   // Replace bytes in original with new bytes
} DeltaOperationType;

// A single delta operation
typedef struct {
	DeltaOperationType	type;
	uint32_t		offset; // Offset in original file (for COPY/REPLACE)
	uint32_t		length; // Length of data
	uint8_t *		data;   // New data (for INSERT/REPLACE), NULL for COPY
} DeltaOperation;

// Complete delta information
typedef struct {
	uint32_t	original_size;          // Size of original file
	uint32_t	new_size;               // Size of new file
	uint32_t	operation_count;        // Number of operations
	DeltaOperation *operations;             // Array of operations
	uint32_t	delta_size;             // Total size of delta data
} DeltaInfo;

// ============================================================================
// Rolling Hash Structures (for rsync-like algorithm)
// ============================================================================

// Rolling hash state
typedef struct {
	uint32_t	a, b;                   // Rolling hash values
	uint32_t	window_size;            // Size of sliding window
	uint8_t *	window;                 // Circular buffer for window
	uint32_t	window_pos;             // Current position in circular buffer
	uint32_t	bytes_in_window;        // How many bytes are currently in the window
} RollingHash;

// Hash table entry for finding matches
typedef struct HashEntry {
	uint32_t		hash;   // Hash value
	uint32_t		offset; // Offset in original file
	struct HashEntry *	next;   // Next entry (for collision resolution)
} HashEntry;

// Hash table for finding matches
typedef struct {
	HashEntry **	buckets;        // Array of hash buckets
	uint32_t	bucket_count;   // Number of buckets
	uint32_t	entry_count;    // Total number of entries
} HashTable;

// Match structure for delta algorithm
typedef struct {
	uint32_t	original_offset; // Offset in original file
	uint32_t	new_offset;      // Offset in new file
	uint32_t	length;          // Length of match
} Match;

// Delta state for tracking matches during creation
typedef struct {
	uint32_t	new_pos;                // Current position in new file
	uint32_t	original_pos;           // Current position in original file
	uint32_t	match_count;            // Number of matches found
	Match *		matches;                // Array of matches
	uint32_t	matches_capacity;       // Capacity of matches array
} DeltaState;

// ============================================================================
// File Buffer (reusing from your exercises)
// ============================================================================

typedef struct {
	uint8_t *	data;
	size_t		size;
	size_t		capacity;
} FileBuffer;

// ============================================================================
// Function Prototypes
// ============================================================================

// Delta creation and application
DeltaInfo * delta_create(const uint8_t *original_data, uint32_t original_size, const uint8_t *new_data, uint32_t new_size);
int delta_apply(const uint8_t *original_data, uint32_t original_size, const DeltaInfo *delta, uint8_t *output_buffer);
void delta_free(DeltaInfo *delta);

// Rolling hash functions
RollingHash * rolling_hash_new(uint32_t window_size);
void rolling_hash_update(RollingHash *rh, uint8_t byte);
uint32_t rolling_hash_get(RollingHash *rh);
void rolling_hash_free(RollingHash *rh);

// Hash table functions
HashTable * hash_table_new(uint32_t bucket_count);
void hash_table_insert(HashTable *ht, uint32_t hash, uint32_t offset);
HashEntry * hash_table_find(HashTable *ht, uint32_t hash);
void hash_table_free(HashTable *ht);

// Delta state functions
DeltaState * delta_state_new(uint32_t initial_capacity);
int delta_state_add_match(DeltaState *state, uint32_t original_offset, uint32_t new_offset, uint32_t length);
void delta_state_free(DeltaState *state);

// File buffer functions (from your exercises)
FileBuffer * file_buffer_new(size_t initial_capacity);
int file_buffer_append(FileBuffer *fb, const uint8_t *data, size_t data_size);
void file_buffer_free(FileBuffer *fb);

// ============================================================================
// Storage System Structures
// ============================================================================

// File metadata structure for storage
typedef struct {
	char		filename[256];          // Original filename
	uint32_t	version;                // Version number
	uint32_t	original_size;          // Size of original file
	uint32_t	delta_size;             // Size of delta data
	uint32_t	operation_count;        // Number of delta operations
	time_t		timestamp;              // Creation timestamp
	char		checksum[64];           // File checksum (for integrity)
	char		message[256];           // Message associated with the version
} FileMetadata;

// Storage system configuration
typedef struct {
	char		storage_dir[512];       // Base directory for storage
	uint32_t	max_versions;           // Maximum versions to keep per file
	int		compression_enabled;    // Whether to compress deltas
} StorageConfig;

// ============================================================================
// Storage System Functions
// ============================================================================

// Storage management
StorageConfig * storage_init(const char *storage_dir);
void storage_free(StorageConfig *config);

// Delta storage operations
int save_delta(StorageConfig *config, const char *filename, uint32_t version, const DeltaInfo *delta, const uint8_t *original_data, const char *message);
DeltaInfo * load_delta(StorageConfig *config, const char *filename, uint32_t version);

// Version management
int get_file_versions(StorageConfig *config, const char *filename, uint32_t *versions, uint32_t max_versions);
int delete_version(StorageConfig *config, const char *filename, uint32_t version);
int track_file_version(StorageConfig *config, const char *filename, const uint8_t *file_data, uint32_t file_size, const char *message);

// Delta application
int apply_delta(const DeltaInfo *delta, const uint8_t *original_data, uint8_t *output_buffer, uint32_t output_buffer_size);
uint8_t * apply_delta_alloc(const uint8_t *original_data, uint32_t original_size, const DeltaInfo *delta);
uint8_t * reconstruct_file_from_deltas(StorageConfig *config, const char *filename, uint32_t target_version, uint32_t *final_size);

// Utility functions
uint32_t calculate_hash(const uint8_t *data, uint32_t length);
void print_delta_info(const DeltaInfo *delta);

#endif // DELTA_STRUCTURES_H
