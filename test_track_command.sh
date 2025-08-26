#!/bin/bash

# Test script for fiver track command
# Note: We don't use set -e because we want to continue testing even if some tests fail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

# Function to print test results
print_result() {
    local test_name="$1"
    local exit_code="$2"
    local expected_exit="$3"

    if [ "$exit_code" -eq "$expected_exit" ]; then
        echo -e "${GREEN}✓ PASS${NC}: $test_name"
        ((TESTS_PASSED++))
    else
        echo -e "${RED}✗ FAIL${NC}: $test_name (expected exit $expected_exit, got $exit_code)"
        ((TESTS_FAILED++))
    fi
}

# Function to run a test
run_test() {
    local test_name="$1"
    local command="$2"
    local expected_exit="$3"

    echo -e "${BLUE}Running:${NC} $test_name"
    echo "Command: $command"

    eval "$command" > /dev/null 2>&1
    local exit_code=$?

    print_result "$test_name" "$exit_code" "$expected_exit"
    echo ""
}

# Function to check if output contains expected text
run_test_with_output() {
    local test_name="$1"
    local command="$2"
    local expected_exit="$3"
    local expected_output="$4"

    echo -e "${BLUE}Running:${NC} $test_name"
    echo "Command: $command"

    local output
    output=$(eval "$command" 2>&1)
    local exit_code=$?

    if [ "$exit_code" -eq "$expected_exit" ] && echo "$output" | grep -q "$expected_output"; then
        echo -e "${GREEN}✓ PASS${NC}: $test_name"
        ((TESTS_PASSED++))
    else
        echo -e "${RED}✗ FAIL${NC}: $test_name (expected exit $expected_exit, got $exit_code)"
        echo "Expected output to contain: '$expected_output'"
        echo "Actual output:"
        echo "$output"
        ((TESTS_FAILED++))
    fi
    echo ""
}

# Function to check if file exists
check_file_exists() {
    local file="$1"
    if [ -f "$file" ]; then
        echo -e "${GREEN}✓ PASS${NC}: File exists: $file"
        ((TESTS_PASSED++))
    else
        echo -e "${RED}✗ FAIL${NC}: File missing: $file"
        ((TESTS_FAILED++))
    fi
}

# Function to cleanup test files
cleanup() {
    echo -e "${YELLOW}Cleaning up test files...${NC}"
    rm -f test_file.txt empty_file.txt test_binary.bin large_test_file.bin file1.txt file2.txt "test file with spaces.txt"
    rm -rf fiver_storage
    echo "Cleanup complete"
    echo ""
}

# Function to print summary
print_summary() {
    echo "=========================================="
    echo -e "${BLUE}TEST SUMMARY${NC}"
    echo "=========================================="
    echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
    echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
    local total=$((TESTS_PASSED + TESTS_FAILED))
    echo "Total tests: $total"

    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "${GREEN}All tests passed! 🎉${NC}"
        exit 0
    else
        echo -e "${RED}Some tests failed! 😞${NC}"
        exit 1
    fi
}

# Main test execution
echo -e "${BLUE}==========================================${NC}"
echo -e "${BLUE}  FIVER TRACK COMMAND AUTOMATED TESTS${NC}"
echo -e "${BLUE}==========================================${NC}"
echo ""

# Clean up any existing test files
cleanup

# Test 1: Build the fiver executable
echo -e "${YELLOW}Building fiver executable...${NC}"
if make > /dev/null 2>&1; then
    echo -e "${GREEN}✓ Build successful${NC}"
    ((TESTS_PASSED++))
else
    echo -e "${RED}✗ Build failed${NC}"
    ((TESTS_FAILED++))
    print_summary
fi
echo ""

# Test 2: Help command
run_test_with_output "Help command" "./fiver --help" 0 "Usage:"

# Test 3: Track command help
run_test_with_output "Track help" "./fiver track --help" 0 "Usage: fiver track"

# Test 4: Missing file argument
run_test_with_output "Missing file argument" "./fiver track" 1 "missing file argument"

# Test 5: Non-existent file
run_test_with_output "Non-existent file" "./fiver track nonexistent_file.txt" 1 "File does not exist"

# Test 6: Non-regular file (device file)
run_test_with_output "Non-regular file" "./fiver track /dev/null" 1 "Not a regular file"

# Test 7: Empty file (our implementation allows empty files)
echo "" > empty_file.txt
run_test_with_output "Empty file" "./fiver track empty_file.txt" 0 "Tracked empty_file.txt"

# Test 8: Successful tracking of text file
echo "Hello, fiver!" > test_file.txt
run_test_with_output "Track text file" "./fiver track test_file.txt" 0 "Tracked test_file.txt"

# Test 9: Check storage directory was created
if [ -d "fiver_storage" ]; then
    echo -e "${GREEN}✓ PASS${NC}: Storage directory created"
    ((TESTS_PASSED++))
else
    echo -e "${RED}✗ FAIL${NC}: Storage directory not created"
    ((TESTS_FAILED++))
fi

# Test 10: Check storage files were created
check_file_exists "fiver_storage/test_file.txt_v1.delta"
check_file_exists "fiver_storage/test_file.txt_v1.meta"

# Test 11: Track same file again (version 2)
run_test_with_output "Track same file (version 2)" "./fiver track test_file.txt" 0 "Tracked test_file.txt"

# Test 12: Check version 2 files were created
check_file_exists "fiver_storage/test_file.txt_v2.delta"
check_file_exists "fiver_storage/test_file.txt_v2.meta"

# Test 13: Track with verbose mode
run_test_with_output "Track with verbose mode" "./fiver track --verbose test_file.txt" 0 "Read.*bytes from test_file.txt"

# Test 14: Check version 3 files were created
check_file_exists "fiver_storage/test_file.txt_v3.delta"
check_file_exists "fiver_storage/test_file.txt_v3.meta"

# Test 15: Track binary file
dd if=/dev/urandom of=test_binary.bin bs=1024 count=1 > /dev/null 2>&1
run_test_with_output "Track binary file" "./fiver track test_binary.bin" 0 "Tracked test_binary.bin"

# Test 16: Check binary file storage
check_file_exists "fiver_storage/test_binary.bin_v1.delta"
check_file_exists "fiver_storage/test_binary.bin_v1.meta"

# Test 17: Track file with spaces in name
echo "test content" > "test file with spaces.txt"
run_test_with_output "Track file with spaces" "./fiver track 'test file with spaces.txt'" 0 "Tracked test file with spaces.txt"

# Test 18: Check file with spaces storage
check_file_exists "fiver_storage/test file with spaces.txt_v1.delta"
check_file_exists "fiver_storage/test file with spaces.txt_v1.meta"

# Test 19: Track large file (1MB)
dd if=/dev/urandom of=large_test_file.bin bs=1M count=1 > /dev/null 2>&1
run_test_with_output "Track large file" "./fiver track large_test_file.bin" 0 "Tracked large_test_file.bin"

# Test 20: Check large file storage
check_file_exists "fiver_storage/large_test_file.bin_v1.delta"
check_file_exists "fiver_storage/large_test_file.bin_v1.meta"

# Test 21: Multiple files tracking
echo "file1 content" > file1.txt
echo "file2 content" > file2.txt
run_test_with_output "Track multiple files" "./fiver track file1.txt && ./fiver track file2.txt" 0 "Tracked"

# Test 22: Check multiple files storage
check_file_exists "fiver_storage/file1.txt_v1.delta"
check_file_exists "fiver_storage/file2.txt_v1.delta"

# Test 23: Invalid command
run_test_with_output "Invalid command" "./fiver invalid_command" 1 "Unknown command"

# Test 24: Invalid option
run_test_with_output "Invalid option" "./fiver --invalid-option" 1 "Unknown command"

# Test 25: Version flag
run_test_with_output "Version flag" "./fiver --version" 0 "fiver 1.0.0"

echo ""
echo -e "${YELLOW}==========================================${NC}"
echo -e "${YELLOW}  STORAGE VERIFICATION TESTS${NC}"
echo -e "${YELLOW}==========================================${NC}"

# Test 26: Verify storage structure
echo -e "${BLUE}Checking storage directory structure...${NC}"
if [ -d "fiver_storage" ]; then
    echo "Storage directory contents:"
    ls -la fiver_storage/
    echo ""

    # Count files
    delta_files=$(find fiver_storage -name "*.delta" | wc -l)
    meta_files=$(find fiver_storage -name "*.meta" | wc -l)

    echo "Found $delta_files delta files and $meta_files meta files"

    if [ "$delta_files" -gt 0 ] && [ "$meta_files" -gt 0 ]; then
        echo -e "${GREEN}✓ PASS${NC}: Storage structure looks correct"
        ((TESTS_PASSED++))
    else
        echo -e "${RED}✗ FAIL${NC}: Storage structure incomplete"
        ((TESTS_FAILED++))
    fi
else
    echo -e "${RED}✗ FAIL${NC}: Storage directory not found"
    ((TESTS_FAILED++))
fi

# Cleanup
cleanup

# Print final summary
print_summary
