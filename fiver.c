#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <time.h>
#include "delta_structures.h"
#include <errno.h>

// Version information
#define FIVER_VERSION "1.0.0"
#define FIVER_DESCRIPTION "A fast file versioning system using delta compression"

// Command structure
typedef struct {
    const char *name;
    const char *description;
    int (*handler)(int argc, char *argv[]);
} Command;

// Forward declarations for command handlers
int cmd_track(int argc, char *argv[]);
int cmd_diff(int argc, char *argv[]);
int cmd_restore(int argc, char *argv[]);
int cmd_history(int argc, char *argv[]);
int cmd_list(int argc, char *argv[]);
int cmd_status(int argc, char *argv[]);

// Global command table
static const Command commands[] = {
    {"track", "Track a new version of a file", cmd_track},
    {"diff", "Show differences between versions", cmd_diff},
    {"restore", "Restore a file to a specific version", cmd_restore},
    {"history", "Show version history of a file", cmd_history},
    {"list", "List all tracked files", cmd_list},
    {"status", "Show current status of a file", cmd_status},
    {NULL, NULL, NULL}  // End marker
};

// Utility functions
void print_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "fiver: error: ");
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void print_success(const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("✓ ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

void print_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    printf("ℹ ");
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

// Help and version functions
void print_version(void) {
    printf("fiver %s\n", FIVER_VERSION);
    printf("%s\n", FIVER_DESCRIPTION);
}

void print_usage(const char *program_name) {
    printf("Usage: %s <command> [options] [arguments]\n\n", program_name);
    printf("Commands:\n");

    for (const Command *cmd = commands; cmd->name != NULL; cmd++) {
        printf("  %-12s %s\n", cmd->name, cmd->description);
    }

    printf("\nGlobal options:\n");
    printf("  -h, --help     Show this help message\n");
    printf("  -v, --version  Show version information\n");
    printf("  --verbose      Enable verbose output\n");
    printf("  --quiet        Suppress non-error output\n");

    printf("\nExamples:\n");
    printf("  %s track document.pdf\n", program_name);
    printf("  %s diff document.pdf --version 2\n", program_name);
    printf("  %s restore document.pdf --version 1\n", program_name);
    printf("  %s history document.pdf\n", program_name);
    printf("  %s list\n", program_name);
    printf("  %s status document.pdf\n", program_name);

    printf("\nFor more information about a command, run:\n");
    printf("  %s <command> --help\n", program_name);
}

void print_command_help(const char *command_name) {
    printf("Usage: fiver %s [options] [arguments]\n\n", command_name);

    // Find the command
    const Command *cmd = NULL;
    for (const Command *c = commands; c->name != NULL; c++) {
        if (strcmp(c->name, command_name) == 0) {
            cmd = c;
            break;
        }
    }

    if (cmd == NULL) {
        print_error("Unknown command: %s", command_name);
        return;
    }

    printf("Description: %s\n\n", cmd->description);

    // Command-specific help
    if (strcmp(command_name, "track") == 0) {
        printf("Arguments:\n");
        printf("  <file>        Path to the file to track\n\n");
        printf("Options:\n");
        printf("  --message, -m <msg>  Add a custom message for this version (max 255 characters)\n");
        printf("Examples:\n");
        printf("  fiver track document.pdf\n");
        printf("  fiver track document.pdf --message \"Added new chapter\"\n");
    }
    else if (strcmp(command_name, "diff") == 0) {
        printf("Arguments:\n");
        printf("  <file>        Path to the tracked file\n\n");
        printf("Options:\n");
        printf("  --version, -v <N>    Compare with version N (default: latest)\n");
        printf("  --json               Output in JSON format\n");
        printf("  --brief              Show only summary\n\n");
        printf("Examples:\n");
        printf("  fiver diff document.pdf\n");
        printf("  fiver diff document.pdf --version 2\n");
        printf("  fiver diff document.pdf --json\n");
    }
    else if (strcmp(command_name, "restore") == 0) {
        printf("Arguments:\n");
        printf("  <file>        Path to the tracked file\n\n");
        printf("Options:\n");
        printf("  --version <N>    Restore to specific version (default: latest)\n");
        printf("  --output, -o <path>  Output file path (default: original path)\n");
        printf("  --force          Overwrite existing file\n");
        printf("  --json           Output in JSON format\n\n");
        printf("Examples:\n");
        printf("  fiver restore document.pdf\n");
        printf("  fiver restore document.pdf --version 2\n");
        printf("  fiver restore document.pdf --version 1 --force\n");
        printf("  fiver restore document.pdf --version 2 --output old_version.pdf\n");
    }
    else if (strcmp(command_name, "history") == 0) {
        printf("Arguments:\n");
        printf("  <file>        Path to the tracked file\n\n");
        printf("Options:\n");
        printf("  --format <fmt>       Output format (table, json, brief)\n");
        printf("  --limit <N>          Show only last N versions\n\n");
        printf("Examples:\n");
        printf("  fiver history document.pdf\n");
        printf("  fiver history document.pdf --format json\n");
        printf("  fiver history document.pdf --limit 5\n");
    }
    else if (strcmp(command_name, "list") == 0) {
        printf("Options:\n");
        printf("  --show-sizes         Show file sizes\n");
        printf("  --format <fmt>       Output format (table, json)\n");
        printf("  --sort <field>       Sort by (name, size, modified)\n\n");
        printf("Examples:\n");
        printf("  fiver list\n");
        printf("  fiver list --show-sizes\n");
        printf("  fiver list --format json\n");
    }
    else if (strcmp(command_name, "status") == 0) {
        printf("Arguments:\n");
        printf("  <file>        Path to the tracked file\n\n");
        printf("Options:\n");
        printf("  --json               Output in JSON format\n\n");
        printf("Examples:\n");
        printf("  fiver status document.pdf\n");
        printf("  fiver status document.pdf --json\n");
    }
}

// Global flags
static int verbose_flag = 0;
static int quiet_flag = 0;
static char *message_flag = NULL;

// Main function
int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    // Check for global options first
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        print_usage(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        print_version();
        return 0;
    }

    // Check for command help
    if (argc >= 3 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        print_command_help(argv[1]);
        return 0;
    }

    // Find the command
    const char *command_name = argv[1];
    const Command *cmd = NULL;

    for (const Command *c = commands; c->name != NULL; c++) {
        if (strcmp(c->name, command_name) == 0) {
            cmd = c;
            break;
        }
    }

    if (cmd == NULL) {
        print_error("Unknown command: %s", command_name);
        printf("Run '%s --help' for usage information.\n", argv[0]);
        return 1;
    }

    // Set up command arguments (skip program name and command name)
    int cmd_argc = argc - 2;
    char **cmd_argv = argv + 2;

    // Process global flags in command arguments
    for (int i = 0; i < cmd_argc; i++) {
        if (strcmp(cmd_argv[i], "--verbose") == 0) {
            verbose_flag = 1;
            // Remove from arguments
            for (int j = i; j < cmd_argc - 1; j++) {
                cmd_argv[j] = cmd_argv[j + 1];
            }
            cmd_argc--;
            i--; // Recheck this position
        }
        else if (strcmp(cmd_argv[i], "--quiet") == 0) {
            quiet_flag = 1;
            // Remove from arguments
            for (int j = i; j < cmd_argc - 1; j++) {
                cmd_argv[j] = cmd_argv[j + 1];
            }
            cmd_argc--;
            i--; // Recheck this position
        }
        else if (strcmp(cmd_argv[i], "--message") == 0 || strcmp(cmd_argv[i], "-m") == 0) {
            if (i + 1 >= cmd_argc) {
                print_error("--message requires a value");
                return 1;
            }
            message_flag = cmd_argv[i + 1];
            if (strlen(message_flag) > 255) {
                print_error("Message is too long (max 255 characters)");
                return 1;
            }

            // Remove both --message and its value from arguments
            for (int j = i; j < cmd_argc - 2; j++) {
                cmd_argv[j] = cmd_argv[j + 2];
            }
            cmd_argc -= 2;
            i--; // Recheck this position
        }
    }

    // Call the command handler
    int result = cmd->handler(cmd_argc, cmd_argv);

    if (result != 0 && !quiet_flag) {
        print_error("Command '%s' failed with exit code %d", command_name, result);
    }

    return result;
}

// Placeholder command implementations (we'll implement these next)
int cmd_track(int argc, char *argv[]) {
    if (argc < 1) {
        print_error("track: missing file argument");
        printf("Usage: fiver track <file> [options]\n");
        return 1;
    }

    const char *filename = argv[0];
    if (verbose_flag) {
        print_info("Tracking file: %s", filename);
    }

    // Check if file exists
    if (access(filename, F_OK) != 0) {
        print_error("File does not exist: %s", filename);
        return 1;
    }

    // Check if file is readable
    if (access(filename, R_OK) != 0) {
        print_error("File is not readable: %s", filename);
        return 1;
    }

    // Check if it's a regular file
    struct stat st;
    if (stat(filename, &st) != 0) {
        print_error("Cannot access file: %s", filename);
        return 1;
    }

    if (!S_ISREG(st.st_mode)) {
        print_error("Not a regular file: %s", filename);
        return 1;
    }

    // Initialize storage
    StorageConfig* config = storage_init("./fiver_storage");
    if (config == NULL) {
        print_error("Failed to initialize storage");
        return 1;
    }

    if (verbose_flag) {
        print_info("Storage initialized: %s", config->storage_dir);
    }

    // Read the file data
    FILE* file = fopen(filename, "rb");
    if (file == NULL) {
        print_error("Cannot open file: %s", filename);
        storage_free(config);
        return 1;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size < 0) {
        print_error("Cannot determine file size: %s", filename);
        fclose(file);
        storage_free(config);
        return 1;
    }

    if (file_size == 0) {
        print_error("Cannot track empty file: %s", filename);
        fclose(file);
        storage_free(config);
        return 1;
    }

    // Allocate buffer and read file
    uint8_t* file_data = malloc(file_size);
    if (file_data == NULL) {
        print_error("Out of memory");
        fclose(file);
        storage_free(config);
        return 1;
    }

    size_t bytes_read = fread(file_data, 1, file_size, file);
    fclose(file);

    if (bytes_read != (size_t)file_size) {
        print_error("Failed to read file: %s", filename);
        free(file_data);
        storage_free(config);
        return 1;
    }

    if (verbose_flag) {
        print_info("Read %zu bytes from %s", bytes_read, filename);
    }

    // Track the file version
    int result = track_file_version(config, filename, file_data, file_size, message_flag);

    // Clean up file data
    free(file_data);

    if (result < 0) {
        print_error("Failed to track file: %s", filename);
        storage_free(config);
        return 1;
    }

    // Clean up storage config
    storage_free(config);

    print_success("Tracked %s (%zu bytes)", filename, bytes_read);
    return 0;
}

int cmd_diff(int argc, char *argv[]) {
    if (argc < 1) {
        print_error("diff: missing file argument");
        printf("Usage: fiver diff <file> [options]\n");
        return 1;
    }

    const char *filename = argv[0];

    // Parse options: --version/-v, --json, --brief
    uint32_t target_version = 0; // 0 => latest
    int json_flag_local = 0;
    int brief_flag_local = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            if (i + 1 >= argc) {
                print_error("--version requires a value");
                return 1;
            }
            long v = strtol(argv[i + 1], NULL, 10);
            if (v <= 0) {
                print_error("Invalid version: %s", argv[i + 1]);
                return 1;
            }
            target_version = (uint32_t)v;
            i++;
        } else if (strcmp(argv[i], "--json") == 0) {
            json_flag_local = 1;
        } else if (strcmp(argv[i], "--brief") == 0) {
            brief_flag_local = 1;
        } else {
            print_error("Unknown option: %s", argv[i]);
            return 1;
        }
    }

    if (verbose_flag) {
        if (target_version > 0) {
            print_info("Showing diff for %s (version %u)", filename, target_version);
        } else {
            print_info("Showing diff for %s (latest)", filename);
        }
    }

    // Initialize storage
    StorageConfig* config = storage_init("./fiver_storage");
    if (config == NULL) {
        print_error("Failed to initialize storage");
        return 1;
    }

    // Resolve latest version if needed
    if (target_version == 0) {
        uint32_t versions[256];
        int count = get_file_versions(config, filename, versions, 256);
        if (count <= 0) {
            print_error("No versions found for: %s", filename);
            storage_free(config);
            return 1;
        }
        uint32_t max_v = versions[0];
        for (int i = 1; i < count; i++) {
            if (versions[i] > max_v) max_v = versions[i];
        }
        target_version = max_v;
    }

    // Load delta
    DeltaInfo* delta = load_delta(config, filename, target_version);
    if (delta == NULL) {
        print_error("Failed to load delta for %s (version %u)", filename, target_version);
        storage_free(config);
        return 1;
    }

    // Output
    if (json_flag_local) {
        // Minimal JSON summary
        printf("{\n");
        printf("  \"file\": \"%s\",\n", filename);
        printf("  \"version\": %u,\n", target_version);
        printf("  \"original_size\": %u,\n", delta->original_size);
        printf("  \"delta_size\": %u,\n", delta->delta_size);
        printf("  \"operation_count\": %u\n", delta->operation_count);
        printf("}\n");
    } else if (brief_flag_local) {
        printf("%s v%u: %u ops, delta %u bytes (orig %u)\n",
               filename, target_version, delta->operation_count, delta->delta_size, delta->original_size);
    } else {
        printf("Diff for %s (version %u):\n", filename, target_version);
        print_delta_info(delta);
    }

    delta_free(delta);
    storage_free(config);
    return 0;
}

int cmd_restore(int argc, char *argv[]) {
    if (argc < 1) {
        print_error("restore: missing file argument");
        printf("Usage: fiver restore <file> [--version <N>] [options]\n");
        printf("Options:\n");
        printf("  --version <N>    Restore to specific version (default: latest)\n");
        printf("  --force          Overwrite existing file\n");
        printf("  --json           Output in JSON format\n");
        printf("Examples:\n");
        printf("  fiver restore document.pdf\n");
        printf("  fiver restore document.pdf --version 2\n");
        printf("  fiver restore document.pdf --version 1 --force\n");
        return 1;
    }

    const char *filename = argv[0];
    uint32_t target_version = 0; // 0 means latest
    int force_flag = 0;
    int json_flag = 0;
    const char *output_path = NULL; // NULL means use original filename

    // Parse options
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0) {
            if (i + 1 >= argc) {
                print_error("--version requires a value");
                return 1;
            }
            long v = strtol(argv[i + 1], NULL, 10);
            if (v <= 0) {
                print_error("Invalid version: %s (must be > 0)", argv[i + 1]);
                return 1;
            }
            target_version = (uint32_t)v;
            i++; // Skip the value
        } else if (strcmp(argv[i], "--force") == 0) {
            force_flag = 1;
        } else if (strcmp(argv[i], "--json") == 0) {
            json_flag = 1;
        } else if (strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                print_error("--output requires a value");
                return 1;
            }
            output_path = argv[i + 1];
            i++; // Skip the value
        } else {
            print_error("Unknown option: %s", argv[i]);
            return 1;
        }
    }

    if (verbose_flag) {
        print_info("Restoring file: %s", filename);
        if (target_version > 0) {
            print_info("Target version: %u", target_version);
        } else {
            print_info("Target version: latest");
        }
    }

    // Initialize storage
    StorageConfig* config = storage_init("./fiver_storage");
    if (config == NULL) {
        print_error("Failed to initialize storage");
        return 1;
    }

    // Get available versions
    uint32_t versions[512];
    int count = get_file_versions(config, filename, versions, 512);
    if (count <= 0) {
        print_error("No versions found for: %s", filename);
        storage_free(config);
        return 1;
    }

    // Find the target version
    if (target_version == 0) {
        // Find latest version
        target_version = versions[0];
        for (int i = 1; i < count; i++) {
            if (versions[i] > target_version) {
                target_version = versions[i];
            }
        }
    } else {
        // Validate that the target version exists
        int version_exists = 0;
        for (int i = 0; i < count; i++) {
            if (versions[i] == target_version) {
                version_exists = 1;
                break;
            }
        }
        if (!version_exists) {
            print_error("Version %u not found for: %s", target_version, filename);
            storage_free(config);
            return 1;
        }
    }

    // Determine the actual output path
    const char *actual_output_path = output_path ? output_path : filename;

    // Check if target file already exists
    if (!force_flag && access(actual_output_path, F_OK) == 0) {
        print_error("File %s already exists. Use --force to overwrite.", actual_output_path);
        storage_free(config);
        return 1;
    }

    // Reconstruct the file
    uint32_t file_size;
    uint8_t* file_data = reconstruct_file_from_deltas(config, filename, target_version, &file_size);
    if (file_data == NULL) {
        print_error("Failed to reconstruct version %u of: %s", target_version, filename);
        storage_free(config);
        return 1;
    }

    // Write the file
    FILE* output_file = fopen(actual_output_path, "wb");
    if (output_file == NULL) {
        print_error("Failed to create file: %s (%s)", actual_output_path, strerror(errno));
        free(file_data);
        storage_free(config);
        return 1;
    }

    size_t written = fwrite(file_data, 1, file_size, output_file);
    fclose(output_file);

    if (written != file_size) {
        print_error("Failed to write file: %s (wrote %zu of %u bytes)", actual_output_path, written, file_size);
        free(file_data);
        storage_free(config);
        return 1;
    }

    // Output result
    if (json_flag) {
        printf("{\n");
        printf("  \"file\": \"%s\",\n", filename);
        printf("  \"output_file\": \"%s\",\n", actual_output_path);
        printf("  \"restored_version\": %u,\n", target_version);
        printf("  \"file_size\": %u,\n", file_size);
        printf("  \"success\": true\n");
        printf("}\n");
    } else {
        print_success("Restored %s to version %u (%u bytes) -> %s", filename, target_version, file_size, actual_output_path);
    }

    // Cleanup
    free(file_data);
    storage_free(config);
    return 0;
}

int cmd_history(int argc, char *argv[]) {
    if (argc < 1) {
        print_error("history: missing file argument");
        printf("Usage: fiver history <file> [options]\n");
        return 1;
    }

    const char *filename = argv[0];

    // Options: --format <fmt>, --limit <N>
    const char *format = "table"; // table | json | brief
    int limit = 0; // 0 => no limit
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--format") == 0) {
            if (i + 1 >= argc) { print_error("--format requires a value"); return 1; }
            format = argv[i + 1];
            i++;
        } else if (strcmp(argv[i], "--limit") == 0) {
            if (i + 1 >= argc) { print_error("--limit requires a value"); return 1; }
            long v = strtol(argv[i + 1], NULL, 10);
            if (v < 0) { print_error("Invalid limit: %s", argv[i + 1]); return 1; }
            limit = (int)v;
            i++;
        } else {
            print_error("Unknown option: %s", argv[i]);
            return 1;
        }
    }

    if (verbose_flag) {
        print_info("Showing history for file: %s", filename);
    }

    // Initialize storage
    StorageConfig* config = storage_init("./fiver_storage");
    if (config == NULL) {
        print_error("Failed to initialize storage");
        return 1;
    }

    // Get versions
    uint32_t versions[512];
    int count = get_file_versions(config, filename, versions, 512);
    if (count <= 0) {
        print_error("No versions found for: %s", filename);
        storage_free(config);
        return 1;
    }

    // Sort ascending for consistency
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (versions[j] < versions[i]) {
                uint32_t tmp = versions[i]; versions[i] = versions[j]; versions[j] = tmp;
            }
        }
    }

    int start_index = 0;
    if (limit > 0 && limit < count) {
        start_index = count - limit; // show last <limit> entries
    }

    if (strcmp(format, "json") == 0) {
        printf("{\n  \"file\": \"%s\",\n  \"versions\": [\n", filename);
        int first = 1;
        for (int idx = start_index; idx < count; idx++) {
            uint32_t v = versions[idx];
            // Read metadata
            char metadata_filename[512];
            snprintf(metadata_filename, sizeof(metadata_filename), "%s/%s_v%u.meta", config->storage_dir, filename, v);
            FILE* meta_file = fopen(metadata_filename, "rb");
            FileMetadata meta; memset(&meta, 0, sizeof(meta));
            if (meta_file) { fread(&meta, sizeof(FileMetadata), 1, meta_file); fclose(meta_file); }
            if (!first) printf(",\n"); first = 0;
            printf("    { \"version\": %u, \"operations\": %u, \"delta_size\": %u, \"timestamp\": %ld, \"message\": \"%s\" }",
                   v, meta.operation_count, meta.delta_size, (long)meta.timestamp, meta.message);
        }
        printf("\n  ]\n}\n");
    } else if (strcmp(format, "brief") == 0) {
        for (int idx = start_index; idx < count; idx++) {
            uint32_t v = versions[idx];
            char metadata_filename[512];
            snprintf(metadata_filename, sizeof(metadata_filename), "%s/%s_v%u.meta", config->storage_dir, filename, v);
            FILE* meta_file = fopen(metadata_filename, "rb");
            FileMetadata meta; memset(&meta, 0, sizeof(meta));
            if (meta_file) { fread(&meta, sizeof(FileMetadata), 1, meta_file); fclose(meta_file); }
            printf("v%u: %u ops, delta %u bytes%s%s\n", v, meta.operation_count, meta.delta_size,
                   meta.message[0] ? ", msg: " : "",
                   meta.message[0] ? meta.message : "");
        }
    } else { // table
        printf("History for %s:\n", filename);
        printf("Version  Timestamp            Ops  Delta  Message\n");
        printf("-------  -------------------  ----  -----  -------\n");
        char timebuf[64];
        for (int idx = start_index; idx < count; idx++) {
            uint32_t v = versions[idx];
            char metadata_filename[512];
            snprintf(metadata_filename, sizeof(metadata_filename), "%s/%s_v%u.meta", config->storage_dir, filename, v);
            FILE* meta_file = fopen(metadata_filename, "rb");
            FileMetadata meta; memset(&meta, 0, sizeof(meta));
            if (meta_file) { fread(&meta, sizeof(FileMetadata), 1, meta_file); fclose(meta_file); }
            struct tm *tm_info = localtime(&meta.timestamp);
            if (tm_info) {
                strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);
            } else {
                strcpy(timebuf, "-");
            }
            printf("%-7u  %-19s  %-4u  %-5u  %s\n", v, timebuf, meta.operation_count, meta.delta_size, meta.message);
        }
    }

    storage_free(config);
    return 0;
}

int cmd_list(int argc, char *argv[]) {
    if (verbose_flag) {
        print_info("Listing tracked files");
    }

    // Options: --show-sizes, --format <table|json>
    int show_sizes = 0;
    const char *format = "table";
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--show-sizes") == 0) {
            show_sizes = 1;
        } else if (strcmp(argv[i], "--format") == 0) {
            if (i + 1 >= argc) { print_error("--format requires a value"); return 1; }
            format = argv[i + 1];
            i++;
        } else {
            print_error("Unknown option: %s", argv[i]);
            return 1;
        }
    }

    StorageConfig* config = storage_init("./fiver_storage");
    if (!config) {
        print_error("Failed to initialize storage");
        return 1;
    }

    // Aggregate by base filename
    typedef struct {
        char name[256];
        uint32_t latest_version;
        uint32_t version_count;
        uint64_t total_delta;
    } FileSummary;

    FileSummary summaries[1024];
    int summary_count = 0;

    DIR *dir = opendir(config->storage_dir);
    if (!dir) {
        print_error("Cannot open storage dir: %s", config->storage_dir);
        storage_free(config);
        return 1;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        const char *fname = ent->d_name;
        size_t len = strlen(fname);
        if (len < 6) continue; // too short
        // We care only about .meta files of pattern <base>_v<ver>.meta
        if (len >= 5 && strcmp(fname + len - 5, ".meta") == 0) {
            // Find "_v" from the end
            const char *suffix = fname + len - 5; // points at ".meta"
            const char *p = suffix;
            // scan backwards to find 'v'
            int idx_v = -1;
            for (int i = (int)(len - 6); i >= 0; i--) {
                if (fname[i] == 'v' && i > 0 && fname[i - 1] == '_') { idx_v = i; break; }
            }
            if (idx_v < 0) continue;
            // base name is upto idx_v-1 (exclude "_v")
            size_t base_len = (size_t)(idx_v - 1);
            if (base_len >= sizeof(summaries[0].name)) base_len = sizeof(summaries[0].name) - 1;
            char base[256];
            memcpy(base, fname, base_len);
            base[base_len] = '\0';

            // parse version number after 'v' until '.'
            uint32_t ver = 0;
            for (size_t i = (size_t)idx_v + 1; i < len && fname[i] != '.'; i++) {
                if (!isdigit((unsigned char)fname[i])) { ver = 0; break; }
                ver = ver * 10 + (uint32_t)(fname[i] - '0');
            }
            if (ver == 0) continue;

            // read metadata to get delta_size
            char metadata_path[1024];
            snprintf(metadata_path, sizeof(metadata_path), "%s/%s", config->storage_dir, fname);
            FILE *mf = fopen(metadata_path, "rb");
            FileMetadata meta; memset(&meta, 0, sizeof(meta));
            if (mf) {
                fread(&meta, sizeof(FileMetadata), 1, mf);
                fclose(mf);
            }

            // find or add summary
            int pos = -1;
            for (int i = 0; i < summary_count; i++) {
                if (strcmp(summaries[i].name, base) == 0) { pos = i; break; }
            }
            if (pos < 0 && summary_count < (int)(sizeof(summaries)/sizeof(summaries[0]))) {
                pos = summary_count++;
                strncpy(summaries[pos].name, base, sizeof(summaries[pos].name) - 1);
                summaries[pos].name[sizeof(summaries[pos].name) - 1] = '\0';
                summaries[pos].latest_version = 0;
                summaries[pos].version_count = 0;
                summaries[pos].total_delta = 0;
            }
            if (pos >= 0) {
                summaries[pos].version_count += 1;
                if (ver > summaries[pos].latest_version) summaries[pos].latest_version = ver;
                summaries[pos].total_delta += meta.delta_size;
            }
        }
    }
    closedir(dir);

    // Output
    if (strcmp(format, "json") == 0) {
        printf("{\n  \"files\": [\n");
        for (int i = 0; i < summary_count; i++) {
            if (i > 0) printf(",\n");
            if (show_sizes) {
                printf("    { \"name\": \"%s\", \"versions\": %u, \"latest\": %u, \"total_delta\": %llu }",
                       summaries[i].name, summaries[i].version_count, summaries[i].latest_version,
                       (unsigned long long)summaries[i].total_delta);
            } else {
                printf("    { \"name\": \"%s\", \"versions\": %u, \"latest\": %u }",
                       summaries[i].name, summaries[i].version_count, summaries[i].latest_version);
            }
        }
        printf("\n  ]\n}\n");
    } else { // table (default)
        if (show_sizes) {
            printf("Tracked files:\n");
            printf("Name                              Versions  Latest  TotalDelta\n");
            printf("--------------------------------  --------  ------  ----------\n");
            for (int i = 0; i < summary_count; i++) {
                printf("%-32s  %-8u  %-6u  %-10llu\n", summaries[i].name,
                       summaries[i].version_count, summaries[i].latest_version,
                       (unsigned long long)summaries[i].total_delta);
            }
        } else {
            printf("Tracked files:\n");
            printf("Name                              Versions  Latest\n");
            printf("--------------------------------  --------  ------\n");
            for (int i = 0; i < summary_count; i++) {
                printf("%-32s  %-8u  %-6u\n", summaries[i].name,
                       summaries[i].version_count, summaries[i].latest_version);
            }
        }
    }

    storage_free(config);
    return 0;
}

int cmd_status(int argc, char *argv[]) {
    if (argc < 1) {
        print_error("status: missing file argument");
        printf("Usage: fiver status <file> [options]\n");
        return 1;
    }

    const char *filename = argv[0];

    // Options: --json
    int json_flag_local = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            json_flag_local = 1;
        } else {
            print_error("Unknown option: %s", argv[i]);
            return 1;
        }
    }

    if (verbose_flag) {
        print_info("Showing status for file: %s", filename);
    }

    // Initialize storage
    StorageConfig* config = storage_init("./fiver_storage");
    if (config == NULL) {
        print_error("Failed to initialize storage");
        return 1;
    }

    // Get versions
    uint32_t versions[512];
    int count = get_file_versions(config, filename, versions, 512);
    if (count <= 0) {
        print_error("No versions found for: %s", filename);
        storage_free(config);
        return 1;
    }

    // Find latest version
    uint32_t latest_version = versions[0];
    for (int i = 1; i < count; i++) {
        if (versions[i] > latest_version) {
            latest_version = versions[i];
        }
    }

    // Load latest metadata
    char metadata_filename[512];
    snprintf(metadata_filename, sizeof(metadata_filename), "%s/%s_v%u.meta",
             config->storage_dir, filename, latest_version);

    FILE* meta_file = fopen(metadata_filename, "rb");
    if (meta_file == NULL) {
        print_error("Cannot read metadata for version %u", latest_version);
        storage_free(config);
        return 1;
    }

    FileMetadata meta;
    memset(&meta, 0, sizeof(meta));
    if (fread(&meta, sizeof(FileMetadata), 1, meta_file) != 1) {
        print_error("Failed to read metadata");
        fclose(meta_file);
        storage_free(config);
        return 1;
    }
    fclose(meta_file);

    // Check if current file exists and compare
    struct stat current_st;
    int current_exists = (stat(filename, &current_st) == 0);

    // For now, we'll skip hash comparison since calculate_hash is not implemented
    // TODO: Implement proper hash comparison when calculate_hash is available

    // Output
    if (json_flag_local) {
        printf("{\n");
        printf("  \"file\": \"%s\",\n", filename);
        printf("  \"tracked\": true,\n");
        printf("  \"version_count\": %d,\n", count);
        printf("  \"latest_version\": %u,\n", latest_version);
        printf("  \"latest_timestamp\": %ld,\n", (long)meta.timestamp);
        printf("  \"latest_operations\": %u,\n", meta.operation_count);
        printf("  \"latest_delta_size\": %u,\n", meta.delta_size);
        printf("  \"latest_message\": \"%s\",\n", meta.message);
        printf("  \"current_file_exists\": %s,\n", current_exists ? "true" : "false");
        if (current_exists) {
            printf("  \"current_file_size\": %ld,\n", (long)current_st.st_size);
            printf("  \"current_file_modified\": %ld,\n", (long)current_st.st_mtime);
            printf("  \"is_up_to_date\": \"unknown\"\n");
        } else {
            printf("  \"is_up_to_date\": false\n");
        }
        printf("}\n");
    } else {
        printf("Status for %s:\n", filename);
        printf("  Tracked: yes\n");
        printf("  Versions: %d\n", count);
        printf("  Latest version: %u\n", latest_version);

        char timebuf[64];
        struct tm *tm_info = localtime(&meta.timestamp);
        if (tm_info) {
            strftime(timebuf, sizeof(timebuf), "%Y-%m-%d %H:%M:%S", tm_info);
        } else {
            strcpy(timebuf, "unknown");
        }
        printf("  Latest timestamp: %s\n", timebuf);
        printf("  Latest operations: %u\n", meta.operation_count);
        printf("  Latest delta size: %u bytes\n", meta.delta_size);
        if (meta.message[0]) {
            printf("  Latest message: %s\n", meta.message);
        }

        printf("  Current file: %s\n", current_exists ? "exists" : "missing");
        if (current_exists) {
            printf("  Current size: %ld bytes\n", (long)current_st.st_size);
            printf("  Current modified: %s\n", ctime(&current_st.st_mtime));
            printf("  Up to date: unknown (hash comparison not implemented)\n");
        }
    }

    storage_free(config);
    return 0;
}
