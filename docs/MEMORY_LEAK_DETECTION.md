# Memory Leak Detection Guide

This document describes how to detect and fix memory leaks in the fiver project using various tools and techniques.

## 🔍 Available Tools

### 1. **Valgrind** (Primary Tool)
The most comprehensive memory leak detection tool for C programs.

#### Installation
```bash
# Ubuntu/Debian
sudo apt install valgrind

# CentOS/RHEL
sudo yum install valgrind

# macOS
brew install valgrind
```

#### Usage
```bash
# Basic memory leak check
valgrind --leak-check=full --show-leak-kinds=all ./fiver track file.txt

# With detailed output
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
    --verbose ./fiver track file.txt

# Suppress known false positives
valgrind --leak-check=full --show-leak-kinds=all \
    --suppressions=valgrind.supp ./fiver track file.txt
```

### 2. **AddressSanitizer** (Runtime Detection)
Built into GCC/Clang, detects memory errors at runtime.

#### Usage
```bash
# Build with AddressSanitizer
make clean
make CFLAGS="-g -O1 -fsanitize=address -fno-omit-frame-pointer"

# Run the program
./fiver track file.txt
```

### 3. **MemorySanitizer** (Uninitialized Memory)
Detects use of uninitialized memory (Clang only).

#### Usage
```bash
# Build with MemorySanitizer
make clean
make CFLAGS="-g -O1 -fsanitize=memory -fno-omit-frame-pointer"

# Run the program
./fiver track file.txt
```

### 4. **UndefinedBehaviorSanitizer** (UB Detection)
Detects undefined behavior that can lead to memory issues.

#### Usage
```bash
# Build with UBSan
make clean
make CFLAGS="-g -O1 -fsanitize=undefined -fno-omit-frame-pointer"

# Run the program
./fiver track file.txt
```

### 5. **Static Analysis Tools**

#### cppcheck
```bash
# Install
sudo apt install cppcheck

# Run analysis
cppcheck --enable=all --inconclusive --std=c99 src/ include/
```

#### clang static analyzer
```bash
# Install
sudo apt install clang-tools

# Run analysis
scan-build -o scan-build-report make clean all
```

## 🛠️ Makefile Targets

The project includes several Makefile targets for memory leak detection:

### Quick Memory Test
```bash
make memory-quick
```
- Runs a basic valgrind test on core operations
- Fast execution (~30 seconds)
- Good for development workflow

### Comprehensive Memory Test
```bash
make memory-test
```
- Runs valgrind on all fiver operations
- Tests with different file types and sizes
- Generates detailed reports
- Takes longer (~5-10 minutes)

### Static Analysis
```bash
make static-analysis
```
- Runs multiple static analysis tools
- Includes AddressSanitizer, MemorySanitizer, UBSan
- Generates comprehensive reports

## 📊 Understanding Valgrind Output

### Memory Leak Types

#### 1. **Definitely Lost**
```
==12345== 8 bytes in 1 blocks are definitely lost in loss record 1 of 1
==12345==    at 0x4C2DB8F: malloc (in /usr/lib/valgrind/vgpreload_memcheck-amd64-linux.so)
==12345==    by 0x4005A1: main (example.c:10)
```
**Action**: Fix immediately - this is a real memory leak.

#### 2. **Indirectly Lost**
```
==12345== 16 bytes in 1 blocks are indirectly lost in loss record 2 of 2
==12345==    at 0x4C2DB8F: malloc (in /usr/lib/valgrind/vgpreload_memcheck-amd64-linux.so)
==12345==    by 0x4005B1: create_structure (example.c:15)
```
**Action**: Usually fixed by fixing the "definitely lost" leak.

#### 3. **Possibly Lost**
```
==12345== 24 bytes in 1 blocks are possibly lost in loss record 3 of 3
==12345==    at 0x4C2DB8F: malloc (in /usr/lib/valgrind/vgpreload_memcheck-amd64-linux.so)
==12345==    by 0x4005C1: some_function (example.c:20)
```
**Action**: Investigate - might be a leak or false positive.

#### 4. **Still Reachable**
```
==12345== 32 bytes in 1 blocks are still reachable in loss record 4 of 4
==12345==    at 0x4C2DB8F: malloc (in /usr/lib/valgrind/vgpreload_memcheck-amd64-linux.so)
==12345==    by 0x4005D1: library_function (library.c:10)
```
**Action**: Usually safe to ignore - library allocations.

### Common Issues and Fixes

#### 1. **Uninitialized Bytes**
```
==12345== Syscall param write(buf) points to uninitialised byte(s)
==12345==    at 0x4989574: write (write.c:26)
==12345==    by 0x4900974: _IO_file_write@@GLIBC_2.2.5 (fileops.c:1181)
```
**Fix**: Initialize structures with `memset()` or `calloc()`.

#### 2. **Use After Free**
```
==12345== Invalid read of size 4
==12345==    at 0x4005A1: main (example.c:10)
==12345==    by 0x4005B1: some_function (example.c:15)
==12345==  Address 0x51f0040 is 0 bytes inside a block of size 4 free'd
==12345==    at 0x4C2DB8F: free (in /usr/lib/valgrind/vgpreload_memcheck-amd64-linux.so)
==12345==    by 0x4005C1: cleanup (example.c:20)
```
**Fix**: Don't use memory after freeing it.

#### 3. **Buffer Overflow**
```
==12345== Invalid write of size 1
==12345==    at 0x4005A1: strcpy (example.c:10)
==12345==    by 0x4005B1: main (example.c:15)
==12345==  Address 0x51f0040 is 0 bytes after a block of size 4 alloc'd
==12345==    at 0x4C2DB8F: malloc (in /usr/lib/valgrind/vgpreload_memcheck-amd64-linux.so)
==12345==    by 0x4005C1: allocate_buffer (example.c:20)
```
**Fix**: Check buffer bounds before writing.

## 🔧 Best Practices

### 1. **Always Initialize Structures**
```c
// Bad
FileMetadata metadata;
metadata.version = 1;

// Good
FileMetadata metadata;
memset(&metadata, 0, sizeof(FileMetadata));
metadata.version = 1;
```

### 2. **Check Return Values**
```c
// Bad
malloc(size);

// Good
void *ptr = malloc(size);
if (ptr == NULL) {
    // Handle error
    return -1;
}
```

### 3. **Free All Allocated Memory**
```c
// Bad
void function() {
    char *str = malloc(100);
    // Function ends without freeing str
}

// Good
void function() {
    char *str = malloc(100);
    if (str == NULL) return;

    // Use str...

    free(str);
}
```

### 4. **Use Valgrind Suppressions**
Create `valgrind.supp` for known false positives:
```
{
   ignore_libc_leaks
   Memcheck:Leak
   ...
   obj:*/libc-*
}
```

## 📈 Performance Impact

| Tool | Performance Impact | Accuracy | Use Case |
|------|-------------------|----------|----------|
| Valgrind | 10-50x slower | High | Development, CI |
| AddressSanitizer | 2-5x slower | High | Development, Testing |
| MemorySanitizer | 2-5x slower | High | Development, Testing |
| UBSan | 1.5-2x slower | Medium | Development, Testing |
| Static Analysis | No runtime impact | Medium | CI, Pre-commit |

## 🚀 CI/CD Integration

### GitHub Actions Example
```yaml
name: Memory Leak Detection
on: [push, pull_request]

jobs:
  memory-test:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v2
    - name: Install dependencies
      run: |
        sudo apt update
        sudo apt install valgrind cppcheck
    - name: Build with debug symbols
      run: make clean && make CFLAGS="-g -O0 -Wall -Wextra -Iinclude"
    - name: Run memory leak tests
      run: make memory-test
    - name: Run static analysis
      run: make static-analysis
```

## 📝 Troubleshooting

### Valgrind Not Working
- Ensure program was built with debug symbols (`-g`)
- Check if valgrind supports your architecture
- Try running with `--tool=memcheck` explicitly

### False Positives
- Use suppressions file for known issues
- Check if the "leak" is actually reachable
- Verify the code is correct before suppressing

### Performance Issues
- Use `--tool=helgrind` for thread issues
- Use `--tool=drd` for data race detection
- Consider using AddressSanitizer for faster detection

## 📚 Additional Resources

- [Valgrind User Manual](https://valgrind.org/docs/manual/manual.html)
- [AddressSanitizer Documentation](https://github.com/google/sanitizers/wiki/AddressSanitizer)
- [MemorySanitizer Documentation](https://github.com/google/sanitizers/wiki/MemorySanitizer)
- [cppcheck Manual](http://cppcheck.sourceforge.net/manual.html)

---

**Remember**: Memory leak detection is an ongoing process. Run these tools regularly during development and before releases to maintain code quality.
