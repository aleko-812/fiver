#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdarg.h>
#include <sys/stat.h>
#include "delta_structures.h"

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
        printf("  --version, -v <N>    Version to restore (required)\n");
        printf("  --output, -o <path>  Output path (default: original path)\n");
        printf("  --force, -f          Overwrite existing file\n\n");
        printf("Examples:\n");
        printf("  fiver restore document.pdf --version 2\n");
        printf("  fiver restore document.pdf --version 1 --output old_version.pdf\n");
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
    if (verbose_flag) {
        print_info("Showing diff for file: %s", filename);
    }

    // TODO: Implement actual diff logic
    print_info("Diff for %s (placeholder implementation)", filename);
    return 0;
}

int cmd_restore(int argc, char *argv[]) {
    if (argc < 1) {
        print_error("restore: missing file argument");
        printf("Usage: fiver restore <file> --version <N> [options]\n");
        return 1;
    }

    const char *filename = argv[0];
    if (verbose_flag) {
        print_info("Restoring file: %s", filename);
    }

    // TODO: Implement actual restore logic
    print_success("Restored %s (placeholder implementation)", filename);
    return 0;
}

int cmd_history(int argc, char *argv[]) {
    if (argc < 1) {
        print_error("history: missing file argument");
        printf("Usage: fiver history <file> [options]\n");
        return 1;
    }

    const char *filename = argv[0];
    if (verbose_flag) {
        print_info("Showing history for file: %s", filename);
    }

    // TODO: Implement actual history logic
    print_info("History for %s (placeholder implementation)", filename);
    return 0;
}

int cmd_list(int argc, char *argv[]) {
    if (verbose_flag) {
        print_info("Listing tracked files");
    }

    // TODO: Implement actual list logic
    print_info("List of tracked files (placeholder implementation)");
    return 0;
}

int cmd_status(int argc, char *argv[]) {
    if (argc < 1) {
        print_error("status: missing file argument");
        printf("Usage: fiver status <file> [options]\n");
        return 1;
    }

    const char *filename = argv[0];
    if (verbose_flag) {
        print_info("Showing status for file: %s", filename);
    }

    // TODO: Implement actual status logic
    print_info("Status for %s (placeholder implementation)", filename);
    return 0;
}
