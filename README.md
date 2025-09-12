# Fiver - Fast File Versioning System

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

FiVer - stands for File Versioning.

A lightweight, efficient file versioning system that tracks changes to text-based files using delta compression. Optimized for source code, configuration files, and data files where content changes map directly to byte changes.

## 🚀 Features

- **Smart Delta Compression**: Three-tier algorithm that automatically chooses the best compression strategy:
  - **Simple approach**: For small end-of-file changes (95%+ identical)
  - **Chunk-based approach**: For small changes anywhere in the file (<1% of file)
  - **Rolling hash algorithm**: For complex changes with rsync-like pattern matching
- **Ultra-Efficient Storage**: Small changes produce tiny deltas (6-11 bytes for 100MB files)
- **Fast Reconstruction**: Quickly restore any previous version by applying delta chains
- **CLI Interface**: Simple command-line interface for all operations
- **Multiple Formats**: Support for human-readable, JSON, and brief output formats
- **Text File Optimization**: Designed for files where content changes map directly to byte changes
- **Message Support**: Add descriptive messages to track changes with `--message` flag
- **Flexible Output**: Restore files to different locations with the `--output` flag
- **Comprehensive Testing**: 119 automated tests covering all functionality
- **Memory Leak Detection**: Built-in valgrind integration and static analysis tools
- **Performance Optimized**: SIMD-accelerated byte comparisons, optimized hash tables, and early termination strategies

## 🎯 Use Cases & Recommendations

### ✅ **Excellent Use Cases**

Fiver works best with files where **content changes map directly to byte changes**:

#### **1. Source Code & Development**
- **Source code files** (`.c`, `.py`, `.js`, `.rs`, `.go`, `.java`, etc.)
- **Configuration files** (`.conf`, `.ini`, `.yaml`, `.json`, `.toml`, `.xml`)
- **Documentation** (`.md`, `.txt`, `.rst`, `.adoc`)
- **Build files** (`Makefile`, `CMakeLists.txt`, `Dockerfile`)
- **Scripts** (`.sh`, `.bat`, `.ps1`)

#### **2. Data Files**
- **CSV/TSV files** (`.csv`, `.tsv`) - row-based changes
- **Log files** (`.log`) - append-only or line-based changes
- **Database dumps** (`.sql`) - text-based SQL exports
- **Backup files** (`.bak`, `.old`) - text-based backups

#### **3. Uncompressed Binary Files**
- **Database files** (SQLite `.db` - some formats)
- **Image files** (`.bmp`, `.ppm` - uncompressed formats)
- **Audio files** (`.wav`, `.flac` - uncompressed)
- **Archive files** (`.tar` - uncompressed)

### ❌ **Poor Use Cases**

Fiver is **not recommended** for files with complex internal structure or compression:

#### **1. Office Documents**
- **Microsoft Office** (`.docx`, `.xlsx`, `.pptx`) - ZIP archives with XML
- **OpenDocument** (`.odt`, `.ods`, `.odp`) - ZIP archives with XML
- **PDF files** (`.pdf`) - complex binary format with metadata

#### **2. Compressed Files**
- **Images** (`.jpg`, `.png`, `.gif`, `.webp`) - compressed formats
- **Audio** (`.mp3`, `.aac`, `.ogg`) - compressed formats
- **Video** (`.mp4`, `.mkv`, `.avi`) - compressed formats
- **Archives** (`.zip`, `.rar`, `.7z`, `.tar.gz`) - compressed formats

#### **3. Files with Metadata Changes**
- **Files with timestamps** that change on every edit
- **Files with different encodings** (UTF-8 vs UTF-16)
- **Files with line ending changes** (LF vs CRLF)

### 🎯 **Best Practices**

1. **Use fiver for**: Source code, configs, documentation, data files
2. **Test first**: Try fiver on a small sample to verify good compression
3. **Monitor delta sizes**: Large deltas indicate the file type isn't suitable
4. **Check memory safety**: Run `make memory-quick` during development
5. **Validate before release**: Run `make memory-all` before deploying

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

### Memory Leak Detection
```bash
# Quick memory leak test (30 seconds)
make memory-quick

# Comprehensive memory leak test (5-10 minutes)
make memory-test

# Static analysis for memory issues
make static-analysis

# Run all memory leak detection tests
make memory-all
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

# Track with short message flag
./fiver track myfile.txt -m "Quick message"

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

### Command-Specific Options

#### Track Command
- `--message, -m`: Add a descriptive message to the version
- `--verbose, -v`: Show detailed tracking information

#### Diff Command
- `--version N`: Show differences for specific version
- `--json`: Output in JSON format
- `--brief`: Show brief summary only

#### History Command
- `--format {table|json|brief}`: Output format (default: table)
- `--limit N`: Limit to last N versions

#### List Command
- `--show-sizes`: Include file size information
- `--format {table|json}`: Output format (default: table)

#### Status Command
- `--json`: Output in JSON format

#### Restore Command
- `--version N`: Restore to specific version (default: latest)
- `--output, -o FILE`: Restore to different file location
- `--force`: Overwrite existing files
- `--json`: Output in JSON format

## 🏗️ Architecture

### Core Components

1. **Delta Algorithm** (`src/delta_algorithm.c`)
   - Three-tier compression strategy:
     - Simple approach for small end-of-file changes
     - Chunk-based approach for small changes anywhere
     - Rolling hash algorithm for complex changes
   - SIMD-accelerated byte comparisons (8-byte and 4-byte chunks)
   - Early termination strategies and cost-benefit analysis
   - Adaptive thresholds based on file size

2. **Storage System** (`src/storage_system.c`)
   - Manages file version storage in `.fiver/` directory
   - Handles delta serialization/deserialization
   - Provides file reconstruction from delta chains
   - Metadata management with timestamps and user messages

3. **Rolling Hash** (`src/rolling_hash.c`)
   - Adler-32 inspired rolling hash implementation
   - Bit-shifting for better hash distribution
   - Efficient sliding window operations

4. **Hash Table** (`src/hash_table.c`)
   - Separate chaining hash table for pattern matching
   - O(1) insertion with head-of-chain placement
   - Handles hash collisions efficiently

5. **CLI Interface** (`src/fiver.c`)
   - Complete command-line interface with 6 commands
   - Comprehensive argument parsing with getopt
   - User-friendly output formatting (table, JSON, brief)
   - Robust error handling and validation

### Storage Format

The system stores files in `.fiver/` directory:

- `filename_v1.delta`: Delta file for version 1 (contains full file)
- `filename_v1.meta`: Metadata for version 1
- `filename_v2.delta`: Delta from v1 to v2
- `filename_v2.meta`: Metadata for version 2
- ... and so on

### Delta Compression Algorithms

Fiver uses a sophisticated three-tier approach to automatically choose the best compression strategy:

#### 1. Simple Approach (95%+ identical files)
- **Use case**: Small additions to the end of files
- **Method**: Find common prefix, then INSERT remaining data
- **Result**: 2 operations (COPY + INSERT)
- **Example**: Adding 11 bytes to 100MB file → 11-byte delta

#### 2. Chunk-based Approach (<1% change)
- **Use case**: Small changes anywhere in the file
- **Method**: Find common prefix and suffix, INSERT middle changes
- **Result**: 3 operations (COPY + INSERT + COPY)
- **Example**: Changing 6 bytes in 100MB file → 6-byte delta

#### 3. Rolling Hash Algorithm (complex changes)
- **Use case**: Major modifications, deletions, insertions
- **Method**: Rsync-like algorithm with rolling hash and pattern matching
- **Features**:
  - Adler-32 inspired rolling hash with bit-shifting
  - SIMD-accelerated 8-byte and 4-byte chunk comparisons
  - Early termination strategies
  - Cost-benefit analysis for match selection
  - Adaptive thresholds based on file size

## 🧪 Testing

The project includes a comprehensive test suite with **119 automated tests** covering:

- All CLI commands and options (track, diff, history, list, status, restore)
- Edge cases and error conditions
- File tracking and versioning with messages
- Delta compression accuracy and size verification
- File reconstruction from delta chains
- Storage system integrity
- Small delta generation for end-of-file and middle-of-file changes
- Binary and text file support
- JSON output format validation
- Error handling and validation

### Test Commands

```bash
# Run basic functionality tests
make test

# Quick memory leak detection (30 seconds)
make memory-quick

# Comprehensive memory leak detection (5-10 minutes)
make memory-test

# Static analysis for potential memory issues
make static-analysis

# Run all memory leak detection tests
make memory-all
```

All tests pass successfully, ensuring robust functionality and memory safety across all features.

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

### Memory Management

- All structures are properly initialized to avoid uninitialized bytes
- Memory leaks are detected using valgrind and static analysis
- Comprehensive memory leak detection tools are available
- See [Memory Leak Detection Guide](docs/MEMORY_LEAK_DETECTION.md) for details

## 📊 Performance

- **Storage Efficiency**:
  - Small changes: 6-11 bytes for 100MB files (99.99%+ compression)
  - Complex changes: Typically 10-50% of original file size for deltas
- **Algorithm Performance**:
  - Rolling hash: O(n) time complexity with optimized hash distribution
  - SIMD-accelerated byte comparisons for faster pattern matching
  - Early termination strategies to avoid unnecessary processing
- **Reconstruction Speed**: Linear time complexity for delta chain application
- **Memory Usage**: Minimal memory footprint during operations
- **Scalability**: Supports thousands of versions per file
- **Hash Table**: O(1) insertion with separate chaining collision resolution

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Ensure all tests pass
6. Open a pull request to `main`

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

The MIT License is a permissive license that allows for:
- Commercial use
- Modification
- Distribution
- Private use

The only requirement is that the license and copyright notice be included in all copies or substantial portions of the software.

## ⚠️ Limitations & Known Issues

### **File Type Limitations**
- **Office documents** (`.docx`, `.xlsx`, `.pptx`) produce large deltas due to internal ZIP/XML structure
- **Compressed files** (`.jpg`, `.png`, `.mp3`, `.zip`) are not suitable for delta compression
- **PDF files** have complex internal structure that affects delta efficiency
- **Files with metadata changes** (timestamps, encoding) may produce larger deltas

### **Technical Limitations**
- Large files (>100MB) may require more memory during delta creation
- Very long filenames (>255 characters) may cause issues
- Concurrent access to the same file is not supported
- Text editors (like `vi`) may make extensive changes that affect delta size

### **Performance Considerations**
- Delta creation time increases with file size
- Very large files (>500MB) may take several minutes to process
- Memory usage scales with file size during delta creation

## 🔮 Ideas for Future Enhancements

- [ ] Compression of delta files (gzip/bzip2)
- [ ] Parallel delta processing for large files
- [ ] Web interface for file management
- [ ] Remote storage support (S3, FTP, etc.)
- [ ] Incremental backup functionality
- [ ] File deduplication across multiple files
- [ ] Branching and merging capabilities
- [ ] Integration with version control systems
- [ ] Memory profiling and optimization tools
- [ ] Automated memory leak detection in CI/CD

## 📚 Quick Reference

### Memory Leak Detection
```bash
make memory-quick      # Quick valgrind test (30s)
make memory-test       # Comprehensive valgrind test (5-10min)
make static-analysis   # Static analysis tools
make memory-all        # Run all memory tests
```

### Development Workflow
```bash
make clean && make     # Clean build
make test             # Run functionality tests
make memory-quick     # Check for memory leaks
make format           # Format code
```

### Documentation
- [Memory Leak Detection Guide](docs/MEMORY_LEAK_DETECTION.md) - Comprehensive memory testing guide
- [LICENSE](LICENSE) - MIT License details

## 📞 Support

For issues, questions, or contributions, please open an issue on the project repository.
