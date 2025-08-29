# Fiver - Fast File Versioning System

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

A lightweight, efficient file versioning system that tracks changes to files using delta compression, similar to Git LFS but simpler and more focused.

## 🚀 Features

- **Delta Compression**: Uses rsync-like algorithms to store only the differences between file versions
- **Efficient Storage**: Only keeps the latest version in plain text, with deltas for previous versions
- **Fast Reconstruction**: Quickly restore any previous version by applying delta chains
- **CLI Interface**: Simple command-line interface for all operations
- **Multiple Formats**: Support for human-readable, JSON, and brief output formats
- **Binary File Support**: Works with any file type (text, binary, documents, etc.)
- **Message Support**: Add descriptive messages to track changes
- **Flexible Output**: Restore files to different locations with the `--output` flag

## 📋 Requirements

- GCC compiler with C99 support
- Standard C library
- POSIX-compliant system (Linux, macOS, BSD)

## 🛠️ Building

### Quick Build
```bash
make
```

### Clean Build
```bash
make clean && make
```

### Run Tests
```bash
make test
```

### Install (optional)
```bash
make install
```

## 📖 Usage

### Basic Commands

#### Track a File
```bash
# Track a file (creates version 1)
./fiver track myfile.txt

# Track with a message
./fiver track myfile.txt --message "Initial version"

# Track with verbose output
./fiver track --verbose myfile.txt
```

#### View File History
```bash
# Show history in table format (default)
./fiver history myfile.txt

# Show history in JSON format
./fiver history myfile.txt --format json

# Show brief history
./fiver history myfile.txt --format brief

# Limit to last 5 versions
./fiver history myfile.txt --limit 5
```

#### View Differences
```bash
# Show differences between versions
./fiver diff myfile.txt

# Show differences for specific version
./fiver diff myfile.txt --version 3

# Show differences in JSON format
./fiver diff myfile.txt --json

# Show brief differences
./fiver diff myfile.txt --brief
```

#### Restore a File
```bash
# Restore to latest version
./fiver restore myfile.txt --force

# Restore to specific version
./fiver restore myfile.txt --version 2 --force

# Restore to different location
./fiver restore myfile.txt --version 1 --output backup.txt --force

# Restore with JSON output
./fiver restore myfile.txt --version 3 --json --force
```

#### List Tracked Files
```bash
# List all tracked files
./fiver list

# List with size information
./fiver list --show-sizes

# List in JSON format
./fiver list --format json
```

#### Check File Status
```bash
# Check status of a file
./fiver status myfile.txt

# Check status in JSON format
./fiver status myfile.txt --json
```

### Global Options

- `--verbose, -v`: Enable verbose output
- `--quiet, -q`: Suppress output (except errors)
- `--version`: Show version information
- `--help, -h`: Show help information

## 🏗️ Architecture

### Core Components

1. **Delta Algorithm** (`src/delta_algorithm.c`)
   - Implements rsync-like delta compression
   - Uses rolling hash for efficient pattern matching
   - Creates delta operations (INSERT/COPY)

2. **Storage System** (`src/storage_system.c`)
   - Manages file version storage
   - Handles delta serialization/deserialization
   - Provides file reconstruction from delta chains

3. **Rolling Hash** (`src/rolling_hash.c`)
   - Adler-32 inspired rolling hash implementation
   - Efficient for sliding window operations

4. **Hash Table** (`src/hash_table.c`)
   - Separate chaining hash table for pattern matching
   - Handles hash collisions efficiently

5. **CLI Interface** (`src/fiver.c`)
   - Command-line argument parsing
   - User-friendly output formatting
   - Error handling and validation

### File Structure

```
fiver/
├── src/                    # Source files
│   ├── fiver.c            # Main CLI interface
│   ├── storage_system.c   # File versioning and storage
│   ├── delta_algorithm.c  # Delta compression algorithm
│   ├── rolling_hash.c     # Rolling hash implementation
│   └── hash_table.c       # Hash table for pattern matching
├── include/               # Header files
│   └── delta_structures.h # Core data structures and prototypes
├── tests/                 # Test files
│   ├── test_track_command.sh  # Comprehensive test suite
│   └── *.c               # Unit test files
├── .fiver/               # Storage directory (created automatically)
├── Makefile              # Build configuration
└── README.md             # This file
```

### Storage Format

The system stores files in `.fiver/` directory:

- `filename_v1.delta`: Delta file for version 1 (contains full file)
- `filename_v1.meta`: Metadata for version 1
- `filename_v2.delta`: Delta from v1 to v2
- `filename_v2.meta`: Metadata for version 2
- ... and so on

#### Metadata Structure
```c
typedef struct {
    uint32_t operation_count;  // Number of delta operations
    uint32_t delta_size;       // Size of delta file in bytes
    time_t timestamp;          // Creation timestamp
    char message[256];         // User-provided message
} FileMetadata;
```

#### Delta Operations
```c
typedef enum {
    DELTA_INSERT,  // Insert new data
    DELTA_COPY     // Copy from original file
} DeltaOperationType;

typedef struct {
    DeltaOperationType type;
    uint32_t offset;      // Offset in original file (for COPY)
    uint32_t length;      // Length of data
    uint8_t* data;        // Data to insert (for INSERT)
} DeltaOperation;
```

## 🧪 Testing

The project includes a comprehensive test suite with 114 tests covering:

- All CLI commands and options
- Edge cases and error conditions
- File tracking and versioning
- Delta compression accuracy
- File reconstruction
- Storage system integrity

Run tests with:
```bash
make test
```

## 🔧 Development

### Building from Source

1. Clone the repository
2. Run `make` to build
3. Run `make test` to verify functionality

### Adding New Features

1. Add source files to `src/`
2. Add headers to `include/`
3. Update `Makefile` with new source files
4. Add tests to `tests/`
5. Update this README

### Code Style

- Follow C99 standard
- Use descriptive variable names
- Add comments for complex logic
- Handle memory allocation errors
- Validate all inputs

## 📊 Performance

- **Storage Efficiency**: Typically 10-50% of original file size for deltas
- **Reconstruction Speed**: Linear time complexity for delta chain application
- **Memory Usage**: Minimal memory footprint during operations
- **Scalability**: Supports thousands of versions per file

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Ensure all tests pass
6. Submit a pull request

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

The MIT License is a permissive license that allows for:
- Commercial use
- Modification
- Distribution
- Private use

The only requirement is that the license and copyright notice be included in all copies or substantial portions of the software.

## 🐛 Known Issues

- Large files (>100MB) may require more memory during delta creation
- Very long filenames (>255 characters) may cause issues
- Concurrent access to the same file is not supported

## 🔮 Ideas for Future Enhancements

- [ ] Compression of delta files
- [ ] Parallel delta processing
- [ ] Web interface
- [ ] Remote storage support

## 📞 Support

For issues, questions, or contributions, please open an issue on the project repository.

---

**Fiver** - Making file versioning simple and efficient! 🚀
