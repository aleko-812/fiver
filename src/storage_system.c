#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "delta_structures.h"

// Forward declarations
DeltaInfo* delta_create(const uint8_t* original_data, uint32_t original_size,
                       const uint8_t* new_data, uint32_t new_size);
void delta_free(DeltaInfo* delta);



/**
 * Initialize storage system with default configuration
 */
StorageConfig* storage_init(const char* storage_dir) {
    StorageConfig* config = malloc(sizeof(StorageConfig));
    if (config == NULL) return NULL;

    // Set default storage directory
    if (storage_dir != NULL) {
        strncpy(config->storage_dir, storage_dir, sizeof(config->storage_dir) - 1);
        config->storage_dir[sizeof(config->storage_dir) - 1] = '\0';
    } else {
        strcpy(config->storage_dir, "./blob_diff_storage");
    }

    config->max_versions = 100;
    config->compression_enabled = 0;  // Disabled for now

    // Create storage directory if it doesn't exist
    struct stat st = {0};
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
 * Free storage configuration
 */
void storage_free(StorageConfig* config) {
    if (config != NULL) {
        free(config);
    }
}

/**
 * Calculate simple checksum for data integrity
 */
void calculate_checksum(const uint8_t* data, uint32_t size, char* checksum) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < size; i++) {
        sum += data[i];
    }
    snprintf(checksum, 64, "%08x", sum);
}

/**
 * Generate storage filename for a specific version
 */
void generate_storage_filename(const char* original_filename, uint32_t version,
                              char* storage_filename, size_t max_len) {
    // Create a safe filename by replacing problematic characters
    char safe_name[256];
    strncpy(safe_name, original_filename, sizeof(safe_name) - 1);
    safe_name[sizeof(safe_name) - 1] = '\0';

    // Replace problematic characters
    for (int i = 0; safe_name[i]; i++) {
        if (safe_name[i] == '/' || safe_name[i] == '\\' || safe_name[i] == ':') {
            safe_name[i] = '_';
        }
    }

    snprintf(storage_filename, max_len, "%s_v%u.delta", safe_name, version);
}

/**
 * Generate metadata filename
 */
void generate_metadata_filename(const char* original_filename, uint32_t version,
                               char* metadata_filename, size_t max_len) {
    char safe_name[256];
    strncpy(safe_name, original_filename, sizeof(safe_name) - 1);
    safe_name[sizeof(safe_name) - 1] = '\0';

    for (int i = 0; safe_name[i]; i++) {
        if (safe_name[i] == '/' || safe_name[i] == '\\' || safe_name[i] == ':') {
            safe_name[i] = '_';
        }
    }

    snprintf(metadata_filename, max_len, "%s_v%u.meta", safe_name, version);
}

/**
 * Save delta to storage
 */
int save_delta(StorageConfig* config, const char* filename, uint32_t version,
               const DeltaInfo* delta, const uint8_t* original_data, const char* message) {
    if (config == NULL || filename == NULL || delta == NULL) return -1;

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
    FILE* delta_file = fopen(full_storage_path, "wb");
    if (delta_file == NULL) {
        printf("Failed to open delta file for writing: %s\n", strerror(errno));
        return -1;
    }

    // Write delta operations
    for (uint32_t i = 0; i < delta->operation_count; i++) {
        const DeltaOperation* op = &delta->operations[i];

        // Write operation header
        fwrite(&op->type, sizeof(DeltaOperationType), 1, delta_file);
        fwrite(&op->offset, sizeof(uint32_t), 1, delta_file);
        fwrite(&op->length, sizeof(uint32_t), 1, delta_file);

        // Write operation data if present
        if (op->data != NULL) {
            fwrite(op->data, sizeof(uint8_t), op->length, delta_file);
        }
    }

    fclose(delta_file);

    // Create and save metadata
    FileMetadata metadata;
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
        metadata.message[0] = '\0';  // Empty message
    }


    // Calculate checksum of original data
    if (original_data != NULL) {
        calculate_checksum(original_data, delta->original_size, metadata.checksum);
    } else {
        strcpy(metadata.checksum, "00000000");
    }

    FILE* meta_file = fopen(full_metadata_path, "wb");
    if (meta_file == NULL) {
        printf("Failed to open metadata file for writing: %s\n", strerror(errno));
        unlink(full_storage_path);  // Clean up delta file
        return -1;
    }

    fwrite(&metadata, sizeof(FileMetadata), 1, meta_file);
    fclose(meta_file);

    printf("Saved delta version %u for '%s' (%u operations, %u bytes)\n",
           version, filename, delta->operation_count, delta->delta_size);

    return 0;
}

/**
 * Load delta from storage
 */
DeltaInfo* load_delta(StorageConfig* config, const char* filename, uint32_t version) {
    if (config == NULL || filename == NULL) return NULL;

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
    FILE* meta_file = fopen(full_metadata_path, "rb");
    if (meta_file == NULL) {
        printf("Failed to open metadata file: %s\n", strerror(errno));
        return NULL;
    }

    FileMetadata metadata;
    if (fread(&metadata, sizeof(FileMetadata), 1, meta_file) != 1) {
        printf("Failed to read metadata\n");
        fclose(meta_file);
        return NULL;
    }
    fclose(meta_file);

    // Create delta structure
    DeltaInfo* delta = malloc(sizeof(DeltaInfo));
    if (delta == NULL) {
        printf("Failed to allocate delta structure\n");
        return NULL;
    }

    delta->original_size = metadata.original_size;
    delta->new_size = 0;  // Will be calculated from operations
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
    FILE* delta_file = fopen(full_storage_path, "rb");
    if (delta_file == NULL) {
        printf("Failed to open delta file: %s\n", strerror(errno));
        free(delta->operations);
        free(delta);
        return NULL;
    }

    for (uint32_t i = 0; i < metadata.operation_count; i++) {
        DeltaOperation* op = &delta->operations[i];

        // Read operation header
        if (fread(&op->type, sizeof(DeltaOperationType), 1, delta_file) != 1 ||
            fread(&op->offset, sizeof(uint32_t), 1, delta_file) != 1 ||
            fread(&op->length, sizeof(uint32_t), 1, delta_file) != 1) {
            printf("Failed to read operation %u\n", i);
            // Clean up partial operations
            for (uint32_t j = 0; j < i; j++) {
                if (delta->operations[j].data != NULL) {
                    free(delta->operations[j].data);
                }
            }
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
                for (uint32_t j = 0; j < i; j++) {
                    if (delta->operations[j].data != NULL) {
                        free(delta->operations[j].data);
                    }
                }
                free(delta->operations);
                free(delta);
                fclose(delta_file);
                return NULL;
            }

            if (fread(op->data, sizeof(uint8_t), op->length, delta_file) != op->length) {
                printf("Failed to read data for operation %u\n", i);
                free(op->data);
                // Clean up
                for (uint32_t j = 0; j < i; j++) {
                    if (delta->operations[j].data != NULL) {
                        free(delta->operations[j].data);
                    }
                }
                free(delta->operations);
                free(delta);
                fclose(delta_file);
                return NULL;
            }
        } else {
            op->data = NULL;  // No data for COPY operations
        }
    }

    fclose(delta_file);

    // Calculate the new_size from operations
    uint32_t calculated_new_size = 0;
    for (uint32_t i = 0; i < delta->operation_count; i++) {
        DeltaOperation* op = &delta->operations[i];
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
 * Get list of available versions for a file
 */
int get_file_versions(StorageConfig* config, const char* filename,
                     uint32_t* versions, uint32_t max_versions) {
    if (config == NULL || filename == NULL || versions == NULL) return -1;

    uint32_t version_count = 0;

    // Scan storage directory for metadata files
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "%s/%s_v*.meta", config->storage_dir, filename);

    // This is a simplified implementation - in a real system, you'd use glob() or readdir()
    // For now, we'll check for versions 1-100
    for (uint32_t v = 1; v <= 100 && version_count < max_versions; v++) {
        char metadata_filename[512];
        char full_metadata_path[1024];

        generate_metadata_filename(filename, v, metadata_filename, sizeof(metadata_filename));
        snprintf(full_metadata_path, sizeof(full_metadata_path), "%s/%s",
                 config->storage_dir, metadata_filename);

        if (access(full_metadata_path, F_OK) == 0) {
            versions[version_count++] = v;
        }
    }

    return version_count;
}

/**
 * Delete a specific version
 */
int delete_version(StorageConfig* config, const char* filename, uint32_t version) {
    if (config == NULL || filename == NULL) return -1;

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

    if (result == 0) {
        printf("Deleted version %u for '%s'\n", version, filename);
    }

    return result;
}

/**
 * Apply delta to reconstruct file
 */
int apply_delta(const DeltaInfo* delta, const uint8_t* original_data,
                uint8_t* output_buffer, uint32_t output_buffer_size) {
    if (delta == NULL || output_buffer == NULL) return -1;
    // original_data can be NULL for first version (where original_size = 0)

    uint32_t output_pos = 0;

    for (uint32_t i = 0; i < delta->operation_count; i++) {
        const DeltaOperation* op = &delta->operations[i];

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

    return output_pos;  // Return the size of reconstructed data
}

/**
 * Apply delta and allocate result buffer
 * This is a convenience function that allocates the output buffer and returns the result
 */
uint8_t* apply_delta_alloc(const uint8_t* original_data, uint32_t original_size, const DeltaInfo* delta) {
    if (delta == NULL) return NULL;

    // Allocate output buffer
    uint8_t* output_buffer = malloc(delta->new_size);
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
 * Reconstruct a file from its delta chain
 * This function reconstructs a specific version by applying all deltas from version 1 up to the target version
 */
uint8_t* reconstruct_file_from_deltas(StorageConfig* config, const char* filename, uint32_t target_version, uint32_t* final_size) {
    if (config == NULL || filename == NULL || final_size == NULL) return NULL;
    if (target_version == 0) return NULL;

    // Start with version 1 (which is a full file)
    uint8_t* current_data = NULL;
    uint32_t current_size = 0;

    // Load version 1 delta (which contains the full file)
    DeltaInfo* delta = load_delta(config, filename, 1);
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
        uint8_t* new_data = apply_delta_alloc(current_data, current_size, delta);
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
 * Track a new version of a file
 */
int track_file_version(StorageConfig* config, const char* filename,
                      const uint8_t* file_data, uint32_t file_size, const char* message) {
    if (config == NULL || filename == NULL || file_data == NULL) return -1;

    // Get current version number
    uint32_t versions[100];
    int version_count = get_file_versions(config, filename, versions, 100);

    uint32_t new_version = 1;
    if (version_count > 0) {
        // Find the highest version number
        uint32_t max_version = versions[0];
        for (int i = 1; i < version_count; i++) {
            if (versions[i] > max_version) {
                max_version = versions[i];
            }
        }
        new_version = max_version + 1;
    }

    // Load the previous version if it exists
    uint8_t* original_data = NULL;
    uint32_t original_size = 0;

    if (version_count > 0) {
        // Reconstruct the previous version from the delta chain
        original_data = reconstruct_file_from_deltas(config, filename, new_version - 1, &original_size);
        if (original_data == NULL) {
            printf("Failed to reconstruct previous version %u\n", new_version - 1);
            return -1;
        }
    }

    // Create delta from previous version (or empty if first version)
    DeltaInfo* delta;
    if (original_data != NULL) {
        delta = delta_create(original_data, original_size, file_data, file_size);
    } else {
        // First version - create a delta that represents the entire file
        delta = malloc(sizeof(DeltaInfo));
        if (delta == NULL) return -1;

        delta->original_size = 0;
        delta->new_size = file_size;
        delta->operation_count = 1;
        delta->delta_size = file_size;

        delta->operations = malloc(sizeof(DeltaOperation));
        if (delta->operations == NULL) {
            free(delta);
            return -1;
        }

        DeltaOperation* op = &delta->operations[0];
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
    if (original_data != NULL) {
        free(original_data);
    }

    return result == 0 ? new_version : -1;
}
