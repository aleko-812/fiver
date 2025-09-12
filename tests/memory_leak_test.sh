#!/bin/bash

# Memory Leak Detection Test Suite for Fiver
# This script tests various fiver operations for memory leaks using Valgrind

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test files
TEST_FILE="memory_test.txt"
TEST_BINARY="memory_test.bin"
LARGE_FILE="memory_large.txt"

# Cleanup function
cleanup() {
    echo -e "${YELLOW}Cleaning up test files...${NC}"
    rm -f "$TEST_FILE" "$TEST_BINARY" "$LARGE_FILE"
    rm -rf .fiver
}

# Set up cleanup on exit
trap cleanup EXIT

echo -e "${GREEN}🔍 Memory Leak Detection Test Suite${NC}"
echo "================================================"

# Build fiver with debug symbols
echo -e "${YELLOW}Building fiver with debug symbols...${NC}"
make clean
make CFLAGS="-g -O0 -Wall -Wextra -Iinclude"

# Test 1: Basic track operation
echo -e "\n${YELLOW}Test 1: Basic track operation${NC}"
echo "Creating test file..."
echo "This is a test file for memory leak detection." > "$TEST_FILE"

echo "Running valgrind on track command..."
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
    --verbose --log-file=valgrind_track.log \
    ./fiver track "$TEST_FILE" -m "Initial version"

if grep -q "ERROR SUMMARY: 0 errors" valgrind_track.log; then
    echo -e "${GREEN}✓ Track operation: No memory leaks detected${NC}"
else
    echo -e "${RED}✗ Track operation: Memory leaks detected${NC}"
    grep -A 5 -B 5 "ERROR SUMMARY" valgrind_track.log
fi

# Test 2: Multiple versions
echo -e "\n${YELLOW}Test 2: Multiple versions${NC}"
echo "Adding more content..." >> "$TEST_FILE"
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
    --verbose --log-file=valgrind_track2.log \
    ./fiver track "$TEST_FILE" -m "Second version"

if grep -q "ERROR SUMMARY: 0 errors" valgrind_track2.log; then
    echo -e "${GREEN}✓ Multiple versions: No memory leaks detected${NC}"
else
    echo -e "${RED}✗ Multiple versions: Memory leaks detected${NC}"
    grep -A 5 -B 5 "ERROR SUMMARY" valgrind_track2.log
fi

# Test 3: History command
echo -e "\n${YELLOW}Test 3: History command${NC}"
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
    --verbose --log-file=valgrind_history.log \
    ./fiver history "$TEST_FILE"

if grep -q "ERROR SUMMARY: 0 errors" valgrind_history.log; then
    echo -e "${GREEN}✓ History command: No memory leaks detected${NC}"
else
    echo -e "${RED}✗ History command: Memory leaks detected${NC}"
    grep -A 5 -B 5 "ERROR SUMMARY" valgrind_history.log
fi

# Test 4: Diff command
echo -e "\n${YELLOW}Test 4: Diff command${NC}"
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
    --verbose --log-file=valgrind_diff.log \
    ./fiver diff "$TEST_FILE"

if grep -q "ERROR SUMMARY: 0 errors" valgrind_diff.log; then
    echo -e "${GREEN}✓ Diff command: No memory leaks detected${NC}"
else
    echo -e "${RED}✗ Diff command: Memory leaks detected${NC}"
    grep -A 5 -B 5 "ERROR SUMMARY" valgrind_diff.log
fi

# Test 5: Restore command
echo -e "\n${YELLOW}Test 5: Restore command${NC}"
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
    --verbose --log-file=valgrind_restore.log \
    ./fiver restore "$TEST_FILE" --version 1 --output restored.txt --force

if grep -q "ERROR SUMMARY: 0 errors" valgrind_restore.log; then
    echo -e "${GREEN}✓ Restore command: No memory leaks detected${NC}"
else
    echo -e "${RED}✗ Restore command: Memory leaks detected${NC}"
    grep -A 5 -B 5 "ERROR SUMMARY" valgrind_restore.log
fi

# Test 6: List command
echo -e "\n${YELLOW}Test 6: List command${NC}"
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
    --verbose --log-file=valgrind_list.log \
    ./fiver list

if grep -q "ERROR SUMMARY: 0 errors" valgrind_list.log; then
    echo -e "${GREEN}✓ List command: No memory leaks detected${NC}"
else
    echo -e "${RED}✗ List command: Memory leaks detected${NC}"
    grep -A 5 -B 5 "ERROR SUMMARY" valgrind_list.log
fi

# Test 7: Large file test
echo -e "\n${YELLOW}Test 7: Large file test${NC}"
echo "Creating large test file..."
dd if=/dev/urandom of="$LARGE_FILE" bs=1M count=10 2>/dev/null

valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
    --verbose --log-file=valgrind_large.log \
    ./fiver track "$LARGE_FILE" -m "Large file test"

if grep -q "ERROR SUMMARY: 0 errors" valgrind_large.log; then
    echo -e "${GREEN}✓ Large file: No memory leaks detected${NC}"
else
    echo -e "${RED}✗ Large file: Memory leaks detected${NC}"
    grep -A 5 -B 5 "ERROR SUMMARY" valgrind_large.log
fi

# Test 8: Binary file test
echo -e "\n${YELLOW}Test 8: Binary file test${NC}"
echo "Creating binary test file..."
dd if=/dev/urandom of="$TEST_BINARY" bs=1K count=100 2>/dev/null

valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes \
    --verbose --log-file=valgrind_binary.log \
    ./fiver track "$TEST_BINARY" -m "Binary file test"

if grep -q "ERROR SUMMARY: 0 errors" valgrind_binary.log; then
    echo -e "${GREEN}✓ Binary file: No memory leaks detected${NC}"
else
    echo -e "${RED}✗ Binary file: Memory leaks detected${NC}"
    grep -A 5 -B 5 "ERROR SUMMARY" valgrind_binary.log
fi

# Summary
echo -e "\n${GREEN}📊 Memory Leak Test Summary${NC}"
echo "================================"

# Count successful tests
SUCCESS_COUNT=0
TOTAL_TESTS=8

for log in valgrind_*.log; do
    if grep -q "ERROR SUMMARY: 0 errors" "$log"; then
        ((SUCCESS_COUNT++))
    fi
done

echo "Successful tests: $SUCCESS_COUNT/$TOTAL_TESTS"

if [ $SUCCESS_COUNT -eq $TOTAL_TESTS ]; then
    echo -e "${GREEN}🎉 All tests passed! No memory leaks detected.${NC}"
else
    echo -e "${RED}⚠️  Some tests failed. Check the valgrind logs for details.${NC}"
    echo "Valgrind log files:"
    ls -la valgrind_*.log
fi

# Clean up valgrind logs
echo -e "\n${YELLOW}Cleaning up valgrind logs...${NC}"
rm -f valgrind_*.log restored.txt

echo -e "${GREEN}Memory leak detection complete!${NC}"
