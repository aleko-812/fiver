#!/bin/bash

# Quick Memory Leak Test for Fiver
# Simple test to quickly check for memory leaks

set -e

echo "🔍 Quick Memory Leak Test"
echo "========================="

# Clean up
rm -f test_memory.txt
rm -rf .fiver

# Build with debug symbols
make clean
make CFLAGS="-g -O0 -Wall -Wextra -Iinclude"

# Create test file
echo "This is a test file for memory leak detection." > test_memory.txt

# Run valgrind on basic operations
echo "Running valgrind on basic operations..."

valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
    --verbose --log-file=quick_memory.log \
    ./fiver track test_memory.txt -m "Test version" && \
    ./fiver history test_memory.txt && \
    ./fiver diff test_memory.txt && \
    ./fiver list

# Check results
if grep -q "ERROR SUMMARY: 0 errors" quick_memory.log; then
    echo "✅ No memory leaks detected!"
else
    echo "❌ Memory leaks detected!"
    echo "Error summary:"
    grep -A 10 "ERROR SUMMARY" quick_memory.log
fi

# Show memory usage summary
echo -e "\nMemory usage summary:"
grep -E "(HEAP SUMMARY|LEAK SUMMARY)" quick_memory.log

# Clean up
rm -f test_memory.txt quick_memory.log
rm -rf .fiver

echo "Quick memory test complete!"
