/**
 * @file storage_system.c
 * @brief File versioning storage system implementation
 *
 * This module provides a complete file versioning storage system that manages
 * delta compression, metadata storage, and file reconstruction. It handles
 * the persistence layer for the fiver application, storing delta operations
 * and metadata in a structured format on disk.
 *
 * The storage system supports:
 * - Delta compression storage and retrieval
 * - Metadata management with timestamps and checksums
 * - File reconstruction from delta chains
 * - Version tracking and management
 * - Safe filename generation and path handling
 *
 * Storage Format:
 * - Delta files: Binary format with operation headers and data
 * - Metadata files: Binary FileMetadata structure with file information
 * - Directory structure: Organized by filename with version suffixes
 *
 * @author Fiver Development Team
 * @version 1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "delta_structures.h"

// Forward declarations
DeltaInfo * delta_create(const uint8_t *original_data, uint32_t original_size, const uint8_t *new_data, uint32_t new_size);
void delta_free(DeltaInfo *delta);

/**
 * @brief Initializes the storage system with default configuration
 *
 * Creates and initializes a new StorageConfig structure with default settings
 * and ensures the storage directory exists. This function sets up the storage
 * system for file versioning operations.
 *
 * @param storage_dir Path to the storage directory. If NULL, uses "./blob_diff_storage".
 *                    The directory will be created if it doesn't exist.
 *
 * @return Pointer to the newly created StorageConfig on success, NULL on failure.
 *         The caller is responsible for freeing the config with storage_free().
 *
 * @note The function creates the storage directory with 0755 permissions if needed.
 *
 * @note Default configuration:
 *       - max_versions: 100
 *       - compression_enabled: 0 (disabled)
 *
 * @example
 * ```c
 * StorageConfig *config = storage_init("/path/to/storage");
 * if (config == NULL) {
 *     // Handle initialization failure
 * }
 * ```
 */
StorageConfig * storage_init(const char *storage_dir)
{
	StorageConfig *config = malloc(sizeof(StorageConfig));

	if (config == NULL)
		return NULL;

	// Set default storage directory
	if (storage_dir != NULL) {
		strncpy(config->storage_dir, storage_dir, sizeof(config->storage_dir) - 1);
		config->storage_dir[sizeof(config->storage_dir) - 1] = '\0';
	} else {
		strcpy(config->storage_dir, "./blob_diff_storage");
	}

	config->max_versions = 100;
	config->compression_enabled = 0; // Disabled for now

	// Create storage directory if it doesn't exist
	struct stat st = { 0 };
	if (stat(config->storage_dir, &st) == -1) {
		if (mkdir(config->storage_dir, 0755) == -1) {
			printf("Failed to create storage directory: %s\n", strerror(errno));
			free(config);
			return NULL;
		}
		printf("Created storage directory: %s\n", config->storage_dir);
	}

	return config;
}

/**
 * @brief Frees memory associated with storage configuration
 *
 * Safely frees all memory allocated for the StorageConfig structure.
 * This function handles NULL pointers gracefully.
 *
 * @param config Pointer to the StorageConfig to free. Safe to pass NULL.
 *
 * @note After calling this function, the config pointer becomes invalid
 *       and should not be dereferenced.
 *
 * @example
 * ```c
 * StorageConfig *config = storage_init("/path/to/storage");
 * // ... use config ...
 * storage_free(config);  // Free memory
 * // config is now invalid
 * ```
 */
void storage_free(StorageConfig *config)
{
	if (config != NULL)
		free(config);
}

/**
 * @brief Calculates a simple checksum for data integrity verification
 *
 * Computes a basic checksum by summing all bytes in the data buffer.
 * This provides a simple integrity check for stored files and deltas.
 *
 * @param data Pointer to the data buffer to checksum. Must not be NULL.
 * @param size Number of bytes to process. Must be > 0.
 * @param checksum Output buffer for the checksum string. Must be at least 64 bytes.
 *                 The checksum is formatted as an 8-character hexadecimal string.
 *
 * @note This is a simple checksum algorithm suitable for basic integrity checking.
 *       For production use, consider implementing CRC32 or SHA-256.
 *
 * @note The checksum string is null-terminated.
 *
 * @example
 * ```c
 * char checksum[64];
 * calculate_checksum(file_data, file_size, checksum);
 * printf("Checksum: %s\n", checksum);
 * ```
 */
void calculate_checksum(const uint8_t *data, uint32_t size, char *checksum)
{
	if (data == NULL || checksum == NULL) {
		printf("Error: Invalid parameters for checksum calculation\n");
		return;
	}

	if (size == 0) {
		strcpy(checksum, "00000000");
		return;
	}

	uint32_t sum = 0;

	for (uint32_t i = 0; i < size; i++)
		sum += data[i];
	snprintf(checksum, 64, "%08x", sum);
}

/**
 * @brief Generates a safe storage filename for a specific version
 *
 * Creates a filesystem-safe filename by replacing problematic characters
 * and appending the version number. This ensures compatibility across
 * different operating systems and filesystems.
 *
 * @param original_filename The original filename to convert. Must not be NULL.
 * @param version Version number to append. Must be > 0.
 * @param storage_filename Output buffer for the generated filename. Must not be NULL.
 * @param max_len Maximum length of the output buffer. Must be > 0.
 *
 * @note Problematic characters (/, \, :) are replaced with underscores.
 *
 * @note The output filename format is: "safe_name_v{version}.delta"
 *
 * @note The function ensures the output string is null-terminated.
 *
 * @example
 * ```c
 * char filename[256];
 * generate_storage_filename("my/file.txt", 5, filename, sizeof(filename));
 * // Result: "my_file.txt_v5.delta"
 * ```
 */
void generate_storage_filename(const char *original_filename, uint32_t version,
			       char *storage_filename, size_t max_len)
{
	if (original_filename == NULL || storage_filename == NULL || max_len == 0) {
		printf("Error: Invalid parameters for filename generation\n");
		return;
	}

	if (version == 0) {
		printf("Error: Version must be greater than 0\n");
		return;
	}

	// Create a safe filename by replacing problematic characters
	char safe_name[256];

	strncpy(safe_name, original_filename, sizeof(safe_name) - 1);
	safe_name[sizeof(safe_name) - 1] = '\0';

	// Replace problematic characters
	for (int i = 0; safe_name[i]; i++)
		if (safe_name[i] == '/' || safe_name[i] == '\\' || safe_name[i] == ':')
			safe_name[i] = '_';

	snprintf(storage_filename, max_len, "%s_v%u.delta", safe_name, version);
}

/**
 * @brief Generates a safe metadata filename for a specific version
 *
 * Creates a filesystem-safe metadata filename by replacing problematic
 * characters and appending the version number. This ensures compatibility
 * across different operating systems and filesystems.
 *
 * @param original_filename The original filename to convert. Must not be NULL.
 * @param version Version number to append. Must be > 0.
 * @param metadata_filename Output buffer for the generated filename. Must not be NULL.
 * @param max_len Maximum length of the output buffer. Must be > 0.
 *
 * @note Problematic characters (/, \, :) are replaced with underscores.
 *
 * @note The output filename format is: "safe_name_v{version}.meta"
 *
 * @note The function ensures the output string is null-terminated.
 *
 * @example
 * ```c
 * char filename[256];
 * generate_metadata_filename("my/file.txt", 5, filename, sizeof(filename));
 * // Result: "my_file.txt_v5.meta"
 * ```
 */
void generate_metadata_filename(const char *original_filename, uint32_t version,
				char *metadata_filename, size_t max_len)
{
	if (original_filename == NULL || metadata_filename == NULL || max_len == 0) {
		printf("Error: Invalid parameters for metadata filename generation\n");
		return;
	}

	if (version == 0) {
		printf("Error: Version must be greater than 0\n");
		return;
	}

	char safe_name[256];

	strncpy(safe_name, original_filename, sizeof(safe_name) - 1);
	safe_name[sizeof(safe_name) - 1] = '\0';

	for (int i = 0; safe_name[i]; i++)
		if (safe_name[i] == '/' || safe_name[i] == '\\' || safe_name[i] == ':')
			safe_name[i] = '_';

	snprintf(metadata_filename, max_len, "%s_v%u.meta", safe_name, version);
}

/**
 * @brief Saves a delta and its metadata to persistent storage
 *
 * Writes delta operations and associated metadata to disk in a structured
 * binary format. This function handles both the delta data and metadata
 * files, ensuring atomicity by cleaning up on failure.
 *
 * @param config Storage configuration. Must not be NULL.
 * @param filename Original filename for versioning. Must not be NULL.
 * @param version Version number to save. Must be > 0.
 * @param delta Delta information to save. Must not be NULL.
 * @param original_data Original file data for checksum calculation. Can be NULL.
 * @param message Optional commit message. Can be NULL.
 *
 * @return EXIT_SUCCESS on success, -1 on failure.
 *
 * @note The function creates two files: .delta (operations) and .meta (metadata).
 *
 * @note On failure, any partially created files are cleaned up.
 *
 * @note The function calculates a checksum of the original data for integrity.
 *
 * @example
 * ```c
 * int result = save_delta(config, "file.txt", 2, delta, orig_data, "Updated file");
 * if (result != EXIT_SUCCESS) {
 *     // Handle save failure
 * }
 * ```
 */
int save_delta(StorageConfig *config, const char *filename, uint32_t version,
	       const DeltaInfo *delta, const uint8_t *original_data, const char *message)
{
	if (config == NULL || filename == NULL || delta == NULL) {
		printf("Error: Invalid parameters for delta save\n");
		return -1;
	}

	if (version == 0) {
		printf("Error: Version must be greater than 0\n");
		return -1;
	}

	if (delta->operation_count == 0) {
		printf("Error: Delta has no operations\n");
		return -1;
	}

	char storage_filename[512];
	char metadata_filename[512];
	char full_storage_path[1024];
	char full_metadata_path[1024];

	// Generate filenames
	generate_storage_filename(filename, version, storage_filename, sizeof(storage_filename));
	generate_metadata_filename(filename, version, metadata_filename, sizeof(metadata_filename));

	// Create full paths
	snprintf(full_storage_path, sizeof(full_storage_path), "%s/%s",
		 config->storage_dir, storage_filename);
	snprintf(full_metadata_path, sizeof(full_metadata_path), "%s/%s",
		 config->storage_dir, metadata_filename);

	// Save delta data
	FILE *delta_file = fopen(full_storage_path, "wb");
	if (delta_file == NULL) {
		printf("Failed to open delta file for writing: %s\n", strerror(errno));
		return -1;
	}

	// Write delta operations
	for (uint32_t i = 0; i < delta->operation_count; i++) {
		const DeltaOperation *op = &delta->operations[i];

		// Write operation header
		fwrite(&op->type, sizeof(DeltaOperationType), 1, delta_file);
		fwrite(&op->offset, sizeof(uint32_t), 1, delta_file);
		fwrite(&op->length, sizeof(uint32_t), 1, delta_file);

		// Write operation data if present
		if (op->data != NULL)
			fwrite(op->data, sizeof(uint8_t), op->length, delta_file);
	}

	fclose(delta_file);

	// Create and save metadata
	FileMetadata metadata;
	memset(&metadata, 0, sizeof(FileMetadata)); // Initialize to avoid uninitialized bytes
	strncpy(metadata.filename, filename, sizeof(metadata.filename) - 1);
	metadata.filename[sizeof(metadata.filename) - 1] = '\0';
	metadata.version = version;
	metadata.original_size = delta->original_size;
	metadata.delta_size = delta->delta_size;
	metadata.operation_count = delta->operation_count;
	metadata.timestamp = time(NULL);
	if (message != NULL) {
		strncpy(metadata.message, message, sizeof(metadata.message) - 1);
		metadata.message[sizeof(metadata.message) - 1] = '\0';
	} else {
		metadata.message[0] = '\0'; // Empty message
	}

	// Calculate checksum of original data
	if (original_data != NULL)
		calculate_checksum(original_data, delta->original_size, metadata.checksum);
	else
		strcpy(metadata.checksum, "00000000");

	FILE *meta_file = fopen(full_metadata_path, "wb");
	if (meta_file == NULL) {
		printf("Failed to open metadata file for writing: %s\n", strerror(errno));
		unlink(full_storage_path); // Clean up delta file
		return -1;
	}

	fwrite(&metadata, sizeof(FileMetadata), 1, meta_file);
	fclose(meta_file);

	printf("Saved delta version %u for '%s' (%u operations, %u bytes)\n",
	       version, filename, delta->operation_count, delta->delta_size);

	return EXIT_SUCCESS;
}

/**
 * @brief Loads a delta and its metadata from persistent storage
 *
 * Reads delta operations and metadata from disk, reconstructing the
 * DeltaInfo structure. This function handles both binary delta data
 * and metadata files.
 *
 * @param config Storage configuration. Must not be NULL.
 * @param filename Original filename to load. Must not be NULL.
 * @param version Version number to load. Must be > 0.
 *
 * @return Pointer to DeltaInfo structure on success, NULL on failure.
 *         The caller is responsible for freeing the delta with delta_free().
 *
 * @note The function loads both .delta and .meta files for the specified version.
 *
 * @note Memory allocation failures are handled gracefully and return NULL.
 *
 * @note The function calculates the new_size from operations during loading.
 *
 * @example
 * ```c
 * DeltaInfo *delta = load_delta(config, "file.txt", 2);
 * if (delta != NULL) {
 *     // Use delta...
 *     delta_free(delta);
 * }
 * ```
 */
DeltaInfo * load_delta(StorageConfig *config, const char *filename, uint32_t version)
{
	if (config == NULL || filename == NULL) {
		printf("Error: Invalid parameters for delta load\n");
		return NULL;
	}

	if (version == 0) {
		printf("Error: Version must be greater than 0\n");
		return NULL;
	}

	char storage_filename[512];
	char metadata_filename[512];
	char full_storage_path[1024];
	char full_metadata_path[1024];

	// Generate filenames
	generate_storage_filename(filename, version, storage_filename, sizeof(storage_filename));
	generate_metadata_filename(filename, version, metadata_filename, sizeof(metadata_filename));

	// Create full paths
	snprintf(full_storage_path, sizeof(full_storage_path), "%s/%s",
		 config->storage_dir, storage_filename);
	snprintf(full_metadata_path, sizeof(full_metadata_path), "%s/%s",
		 config->storage_dir, metadata_filename);

	// Load metadata first
	FILE *meta_file = fopen(full_metadata_path, "rb");
	if (meta_file == NULL) {
		printf("Failed to open metadata file: %s\n", strerror(errno));
		return NULL;
	}

	FileMetadata metadata;
	memset(&metadata, 0, sizeof(FileMetadata)); // Initialize to avoid uninitialized bytes
	if (fread(&metadata, sizeof(FileMetadata), 1, meta_file) != 1) {
		printf("Failed to read metadata\n");
		fclose(meta_file);
		return NULL;
	}
	fclose(meta_file);

	// Create delta structure
	DeltaInfo *delta = malloc(sizeof(DeltaInfo));
	if (delta == NULL) {
		printf("Failed to allocate delta structure\n");
		return NULL;
	}

	delta->original_size = metadata.original_size;
	delta->new_size = 0; // Will be calculated from operations
	delta->operation_count = metadata.operation_count;
	delta->delta_size = metadata.delta_size;

	// Allocate operations array
	delta->operations = malloc(metadata.operation_count * sizeof(DeltaOperation));
	if (delta->operations == NULL) {
		printf("Failed to allocate operations array\n");
		free(delta);
		return NULL;
	}

	// Load delta operations
	FILE *delta_file = fopen(full_storage_path, "rb");
	if (delta_file == NULL) {
		printf("Failed to open delta file: %s\n", strerror(errno));
		free(delta->operations);
		free(delta);
		return NULL;
	}

	for (uint32_t i = 0; i < metadata.operation_count; i++) {
		DeltaOperation *op = &delta->operations[i];

		// Read operation header
		if (fread(&op->type, sizeof(DeltaOperationType), 1, delta_file) != 1 ||
		    fread(&op->offset, sizeof(uint32_t), 1, delta_file) != 1 ||
		    fread(&op->length, sizeof(uint32_t), 1, delta_file) != 1) {
			printf("Failed to read operation %u\n", i);
			// Clean up partial operations
			for (uint32_t j = 0; j < i; j++)
				if (delta->operations[j].data != NULL)
					free(delta->operations[j].data);
			free(delta->operations);
			free(delta);
			fclose(delta_file);
			return NULL;
		}

		// Read operation data if present
		if (op->type == DELTA_INSERT || op->type == DELTA_REPLACE) {
			op->data = malloc(op->length);
			if (op->data == NULL) {
				printf("Failed to allocate data for operation %u\n", i);
				// Clean up
				for (uint32_t j = 0; j < i; j++)
					if (delta->operations[j].data != NULL)
						free(delta->operations[j].data);
				free(delta->operations);
				free(delta);
				fclose(delta_file);
				return NULL;
			}

			if (fread(op->data, sizeof(uint8_t), op->length, delta_file) != op->length) {
				printf("Failed to read data for operation %u\n", i);
				free(op->data);
				// Clean up
				for (uint32_t j = 0; j < i; j++)
					if (delta->operations[j].data != NULL)
						free(delta->operations[j].data);
				free(delta->operations);
				free(delta);
				fclose(delta_file);
				return NULL;
			}
		} else {
			op->data = NULL; // No data for COPY operations
		}
	}

	fclose(delta_file);

	// Calculate the new_size from operations
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

	printf("Loaded delta version %u for '%s' (%u operations, %u bytes)\n",
	       version, filename, delta->operation_count, delta->delta_size);

	return delta;
}

/**
 * @brief Retrieves a list of available versions for a specific file
 *
 * Scans the storage directory for metadata files belonging to the specified
 * filename and returns a list of available version numbers. This function
 * provides version discovery capabilities for the storage system.
 *
 * @param config Storage configuration. Must not be NULL.
 * @param filename Original filename to scan for. Must not be NULL.
 * @param versions Output array for version numbers. Must not be NULL.
 * @param max_versions Maximum number of versions to return. Must be > 0.
 *
 * @return Number of versions found on success, -1 on failure.
 *
 * @note This is a simplified implementation that checks versions 1-100.
 *       A production system would use glob() or readdir() for efficiency.
 *
 * @note The function only checks for the existence of metadata files.
 *
 * @example
 * ```c
 * uint32_t versions[100];
 * int count = get_file_versions(config, "file.txt", versions, 100);
 * if (count > 0) {
 *     printf("Found %d versions\n", count);
 * }
 * ```
 */
int get_file_versions(StorageConfig *config, const char *filename,
		      uint32_t *versions, uint32_t max_versions)
{
	if (config == NULL || filename == NULL || versions == NULL) {
		printf("Error: Invalid parameters for version listing\n");
		return -1;
	}

	if (max_versions == 0) {
		printf("Error: max_versions must be greater than 0\n");
		return -1;
	}

	uint32_t version_count = 0;

	// Scan storage directory for metadata files
	char pattern[1024];
	snprintf(pattern, sizeof(pattern), "%s/%s_v*.meta", config->storage_dir, filename);

	// This is a simplified implementation - in a real system, you'd use glob() or readdir()
	// For now, we'll check for versions 1-100
	for (uint32_t v = 1; v <= 100 && version_count < max_versions; v++) {
		char metadata_filename[512];
		char full_metadata_path[1024];

		generate_metadata_filename(filename, v, metadata_filename, sizeof(metadata_filename));
		snprintf(full_metadata_path, sizeof(full_metadata_path), "%s/%s",
			 config->storage_dir, metadata_filename);

		if (access(full_metadata_path, F_OK) == 0)
			versions[version_count++] = v;
	}

	return version_count;
}

/**
 * @brief Deletes a specific version of a file from storage
 *
 * Removes both the delta file and metadata file for the specified version.
 * This function provides version cleanup capabilities for the storage system.
 *
 * @param config Storage configuration. Must not be NULL.
 * @param filename Original filename to delete. Must not be NULL.
 * @param version Version number to delete. Must be > 0.
 *
 * @return EXIT_SUCCESS on success, -1 on failure.
 *
 * @note The function attempts to delete both .delta and .meta files.
 *
 * @note Partial failures are reported but don't prevent the function from
 *       attempting to delete both files.
 *
 * @example
 * ```c
 * int result = delete_version(config, "file.txt", 3);
 * if (result == EXIT_SUCCESS) {
 *     printf("Version 3 deleted\n");
 * }
 * ```
 */
int delete_version(StorageConfig *config, const char *filename, uint32_t version)
{
	if (config == NULL || filename == NULL) {
		printf("Error: Invalid parameters for version deletion\n");
		return -1;
	}

	if (version == 0) {
		printf("Error: Version must be greater than 0\n");
		return -1;
	}

	char storage_filename[512];
	char metadata_filename[512];
	char full_storage_path[1024];
	char full_metadata_path[1024];

	generate_storage_filename(filename, version, storage_filename, sizeof(storage_filename));
	generate_metadata_filename(filename, version, metadata_filename, sizeof(metadata_filename));

	snprintf(full_storage_path, sizeof(full_storage_path), "%s/%s",
		 config->storage_dir, storage_filename);
	snprintf(full_metadata_path, sizeof(full_metadata_path), "%s/%s",
		 config->storage_dir, metadata_filename);

	// Delete both files
	int result = 0;
	if (unlink(full_storage_path) == -1) {
		printf("Failed to delete delta file: %s\n", strerror(errno));
		result = -1;
	}

	if (unlink(full_metadata_path) == -1) {
		printf("Failed to delete metadata file: %s\n", strerror(errno));
		result = -1;
	}

	if (result == 0)
		printf("Deleted version %u for '%s'\n", version, filename);

	return result;
}

/**
 * @brief Applies delta operations to reconstruct a file
 *
 * Processes delta operations in sequence to reconstruct the target file
 * from the original file data. This function handles COPY, INSERT, and
 * REPLACE operations with proper bounds checking.
 *
 * @param delta Delta information containing operations. Must not be NULL.
 * @param original_data Original file data for COPY operations. Can be NULL for first version.
 * @param output_buffer Buffer to write reconstructed data. Must not be NULL.
 * @param output_buffer_size Size of the output buffer. Must be >= delta->new_size.
 *
 * @return Number of bytes written on success, -1 on failure.
 *
 * @note The function performs bounds checking to prevent buffer overflows.
 *
 * @note For first versions (original_data == NULL), only INSERT operations are allowed.
 *
 * @note The function processes operations in the order they appear in the delta.
 *
 * @example
 * ```c
 * uint8_t buffer[1024];
 * int size = apply_delta(delta, orig_data, buffer, sizeof(buffer));
 * if (size > 0) {
 *     // File reconstructed successfully
 * }
 * ```
 */
int apply_delta(const DeltaInfo *delta, const uint8_t *original_data,
		uint8_t *output_buffer, uint32_t output_buffer_size)
{
	if (delta == NULL || output_buffer == NULL) {
		printf("Error: Invalid parameters for delta application\n");
		return -1;
	}

	if (output_buffer_size < delta->new_size) {
		printf("Error: Output buffer too small (%u < %u)\n", output_buffer_size, delta->new_size);
		return -1;
	}

	// original_data can be NULL for first version (where original_size = 0)

	uint32_t output_pos = 0;

	for (uint32_t i = 0; i < delta->operation_count; i++) {
		const DeltaOperation *op = &delta->operations[i];

		switch (op->type) {
		case DELTA_COPY:
			// Check buffer bounds
			if (output_pos + op->length > output_buffer_size) {
				printf("Output buffer too small for COPY operation\n");
				return -1;
			}

			// For first version, there should be no COPY operations
			if (original_data == NULL) {
				printf("COPY operation not allowed when original_data is NULL\n");
				return -1;
			}

			// Copy from original file
			memcpy(output_buffer + output_pos,
			       original_data + op->offset, op->length);
			output_pos += op->length;
			break;

		case DELTA_INSERT:
			// Check buffer bounds
			if (output_pos + op->length > output_buffer_size) {
				printf("Output buffer too small for INSERT operation\n");
				return -1;
			}

			// Insert new data
			memcpy(output_buffer + output_pos, op->data, op->length);
			output_pos += op->length;
			break;

		case DELTA_REPLACE:
			// Check buffer bounds
			if (output_pos + op->length > output_buffer_size) {
				printf("Output buffer too small for REPLACE operation\n");
				return -1;
			}

			// Replace with new data
			memcpy(output_buffer + output_pos, op->data, op->length);
			output_pos += op->length;
			break;
		}
	}

	return output_pos; // Return the size of reconstructed data
}

/**
 * @brief Applies delta operations and allocates the result buffer
 *
 * Convenience function that combines delta application with buffer allocation.
 * This function automatically allocates the correct size buffer and applies
 * the delta operations, returning the reconstructed data.
 *
 * @param original_data Original file data for COPY operations. Can be NULL for first version.
 * @param original_size Size of the original data. Not used in current implementation.
 * @param delta Delta information containing operations. Must not be NULL.
 *
 * @return Pointer to allocated buffer containing reconstructed data on success,
 *         NULL on failure. The caller is responsible for freeing the buffer.
 *
 * @note The function allocates exactly delta->new_size bytes for the output.
 *
 * @note Memory allocation failures are handled gracefully and return NULL.
 *
 * @example
 * ```c
 * uint8_t *reconstructed = apply_delta_alloc(orig_data, orig_size, delta);
 * if (reconstructed != NULL) {
 *     // Use reconstructed data...
 *     free(reconstructed);
 * }
 * ```
 */
uint8_t * apply_delta_alloc(const uint8_t *original_data, uint32_t original_size,
			    const DeltaInfo *delta)
{
	(void)original_size; // Parameter not used in this implementation
	if (delta == NULL) {
		printf("Error: Delta is NULL\n");
		return NULL;
	}

	if (delta->new_size == 0) {
		printf("Error: Delta has zero new size\n");
		return NULL;
	}

	// Allocate output buffer
	uint8_t *output_buffer = malloc(delta->new_size);
	if (output_buffer == NULL) {
		printf("Failed to allocate output buffer\n");
		return NULL;
	}

	// Apply delta
	int result = apply_delta(delta, original_data, output_buffer, delta->new_size);
	if (result < 0) {
		printf("Failed to apply delta\n");
		free(output_buffer);
		return NULL;
	}

	return output_buffer;
}

/**
 * @brief Reconstructs a file from its complete delta chain
 *
 * Reconstructs a specific version by loading and applying all deltas from
 * version 1 up to the target version. This function handles the complete
 * reconstruction process for any version in the chain.
 *
 * @param config Storage configuration. Must not be NULL.
 * @param filename Original filename to reconstruct. Must not be NULL.
 * @param target_version Target version to reconstruct. Must be > 0.
 * @param final_size Output parameter for the final file size. Must not be NULL.
 *
 * @return Pointer to allocated buffer containing reconstructed file on success,
 *         NULL on failure. The caller is responsible for freeing the buffer.
 *
 * @note The function loads version 1 as the base and applies subsequent deltas.
 *
 * @note Memory allocation failures are handled gracefully and return NULL.
 *
 * @note The function frees intermediate data to prevent memory leaks.
 *
 * @example
 * ```c
 * uint32_t size;
 * uint8_t *file = reconstruct_file_from_deltas(config, "file.txt", 5, &size);
 * if (file != NULL) {
 *     // Use reconstructed file...
 *     free(file);
 * }
 * ```
 */
uint8_t * reconstruct_file_from_deltas(StorageConfig *config, const char *filename,
				       uint32_t target_version, uint32_t *final_size)
{
	if (config == NULL || filename == NULL || final_size == NULL) {
		printf("Error: Invalid parameters for file reconstruction\n");
		return NULL;
	}

	if (target_version == 0) {
		printf("Error: Target version must be greater than 0\n");
		return NULL;
	}

	// Start with version 1 (which is a full file)
	uint8_t *current_data = NULL;
	uint32_t current_size = 0;

	// Load version 1 delta (which contains the full file)
	DeltaInfo *delta = load_delta(config, filename, 1);
	if (delta == NULL) {
		printf("Failed to load version 1 delta\n");
		return NULL;
	}

	// Apply version 1 delta to get the initial file
	current_data = apply_delta_alloc(NULL, 0, delta);
	current_size = delta->new_size;
	delta_free(delta);

	if (current_data == NULL) {
		printf("Failed to apply version 1 delta\n");
		return NULL;
	}

	// If we only need version 1, return it
	if (target_version == 1) {
		*final_size = current_size;
		return current_data;
	}

	// Apply subsequent deltas to reach the target version
	for (uint32_t version = 2; version <= target_version; version++) {
		delta = load_delta(config, filename, version);
		if (delta == NULL) {
			printf("Failed to load version %u delta\n", version);
			free(current_data);
			return NULL;
		}

		// Apply the delta to get the next version
		uint8_t *new_data = apply_delta_alloc(current_data, current_size, delta);
		if (new_data == NULL) {
			printf("Failed to apply version %u delta\n", version);
			delta_free(delta);
			free(current_data);
			return NULL;
		}

		// Update current data and size
		free(current_data);
		current_data = new_data;
		current_size = delta->new_size;
		delta_free(delta);
	}

	*final_size = current_size;
	return current_data;
}

/**
 * @brief Tracks a new version of a file in the storage system
 *
 * Creates and stores a new version of a file by generating a delta from
 * the previous version and saving it to persistent storage. This is the
 * main entry point for file versioning operations.
 *
 * @param config Storage configuration. Must not be NULL.
 * @param filename Original filename to track. Must not be NULL.
 * @param file_data New file data to store. Must not be NULL.
 * @param file_size Size of the new file data. Must be > 0.
 * @param message Optional commit message. Can be NULL.
 *
 * @return New version number on success, -1 on failure.
 *
 * @note For the first version, creates a delta containing the entire file.
 *
 * @note The function automatically determines the next version number.
 *
 * @note Memory allocation failures are handled gracefully and return -1.
 *
 * @example
 * ```c
 * int version = track_file_version(config, "file.txt", data, size, "Updated file");
 * if (version > 0) {
 *     printf("Tracked as version %d\n", version);
 * }
 * ```
 */
int track_file_version(StorageConfig *config, const char *filename,
		       const uint8_t *file_data, uint32_t file_size, const char *message)
{
	if (config == NULL || filename == NULL || file_data == NULL) {
		printf("Error: Invalid parameters for file tracking\n");
		return -1;
	}

	if (file_size == 0) {
		printf("Error: File size must be greater than 0\n");
		return -1;
	}

	// Get current version number
	uint32_t versions[100];
	int version_count = get_file_versions(config, filename, versions, 100);

	uint32_t new_version = 1;
	if (version_count > 0) {
		// Find the highest version number
		uint32_t max_version = versions[0];
		for (int i = 1; i < version_count; i++)
			if (versions[i] > max_version)
				max_version = versions[i];
		new_version = max_version + 1;
	}

	// Load the previous version if it exists
	uint8_t *original_data = NULL;
	uint32_t original_size = 0;

	if (version_count > 0) {
		// Reconstruct the previous version from the delta chain
		original_data = reconstruct_file_from_deltas(config, filename, new_version - 1,
							     &original_size);
		if (original_data == NULL) {
			printf("Failed to reconstruct previous version %u\n", new_version - 1);
			return -1;
		}
	}

	// Create delta from previous version (or empty if first version)
	DeltaInfo *delta;
	if (original_data != NULL) {
		delta = delta_create(original_data, original_size, file_data, file_size);
	} else {
		// First version - create a delta that represents the entire file
		delta = malloc(sizeof(DeltaInfo));
		if (delta == NULL)
			return -1;

		delta->original_size = 0;
		delta->new_size = file_size;
		delta->operation_count = 1;
		delta->delta_size = file_size;

		delta->operations = malloc(sizeof(DeltaOperation));
		if (delta->operations == NULL) {
			free(delta);
			return -1;
		}

		DeltaOperation *op = &delta->operations[0];
		op->type = DELTA_INSERT;
		op->offset = 0;
		op->length = file_size;
		op->data = malloc(file_size);

		if (op->data == NULL) {
			free(delta->operations);
			free(delta);
			return -1;
		}

		memcpy(op->data, file_data, file_size);
	}

	if (delta == NULL) {
		printf("Failed to create delta\n");
		return -1;
	}

	// Save the delta
	int result = save_delta(config, filename, new_version, delta, original_data, message);

	// Cleanup
	delta_free(delta);
	if (original_data != NULL)
		free(original_data);

	return result == 0 ? (int)new_version : -1;
}
