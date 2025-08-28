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
    rm -f test_file.txt empty_file.txt test_binary.bin large_test_file.bin file1.txt file2.txt "test file with spaces.txt" message_test.txt list1.txt list2.txt status_test.txt
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

# Diff command tests
# Test 26: Diff help
run_test_with_output "Diff help" "./fiver diff --help" 0 "Usage: fiver diff"

# Test 27: Diff missing file argument
run_test_with_output "Diff missing file" "./fiver diff" 1 "missing file argument"

# Test 28: Diff on untracked file (no versions)
run_test_with_output "Diff no versions" "./fiver diff untracked.txt" 1 "No versions found"

# Prepare tracked file for diff tests
echo "Hello, fiver!" > diff_test.txt
run_test_with_output "Track for diff" "./fiver track diff_test.txt" 0 "Tracked diff_test.txt"

# Test 29: Diff default output
run_test_with_output "Diff default" "./fiver diff diff_test.txt" 0 "Diff for diff_test.txt"

# Test 30: Diff brief output
run_test_with_output "Diff brief" "./fiver diff diff_test.txt --brief" 0 "ops, delta"

# Test 31: Diff JSON output
run_test_with_output "Diff json" "./fiver diff diff_test.txt --json" 0 "\"file\""

# Create additional versions
echo "Hello, fiver!!" > diff_test.txt
run_test_with_output "Track v2 for diff" "./fiver track diff_test.txt" 0 "Tracked diff_test.txt"
echo "Hello, fiver!!!" > diff_test.txt
run_test_with_output "Track v3 for diff" "./fiver track diff_test.txt" 0 "Tracked diff_test.txt"

# Test 32: Diff specific version
run_test_with_output "Diff version 2" "./fiver diff diff_test.txt --version 2" 0 "version 2"

# Test 33: Diff invalid version value
run_test_with_output "Diff invalid version" "./fiver diff diff_test.txt --version 0" 1 "Invalid version"

# Test 34: Diff unknown option
run_test_with_output "Diff unknown option" "./fiver diff diff_test.txt --bogus" 1 "Unknown option"

# History command tests
# Test 35: History help
run_test_with_output "History help" "./fiver history --help" 0 "Usage: fiver history"

# Test 36: History missing file
run_test_with_output "History missing file" "./fiver history" 1 "missing file argument"

# Test 37: History no versions
run_test_with_output "History no versions" "./fiver history untracked_hist.txt" 1 "No versions found"

# Prepare versions with messages
echo "h1" > hist.txt
run_test_with_output "Track hist v1" "./fiver track hist.txt --message 'first'" 0 "Tracked hist.txt"
echo "h2" > hist.txt
run_test_with_output "Track hist v2" "./fiver track hist.txt --message 'second'" 0 "Tracked hist.txt"
echo "h3" > hist.txt
run_test_with_output "Track hist v3" "./fiver track hist.txt --message 'third'" 0 "Tracked hist.txt"

# Test 38: History table default
run_test_with_output "History table" "./fiver history hist.txt" 0 "History for hist.txt"

# Test 39: History brief format
run_test_with_output "History brief" "./fiver history hist.txt --format brief" 0 "v1:"

# Test 40: History json format
run_test_with_output "History json" "./fiver history hist.txt --format json" 0 "\"versions\""

# Test 41: History limit
run_test_with_output "History limit 2" "./fiver history hist.txt --limit 2" 0 "Version"

# Test 42: History unknown option
run_test_with_output "History unknown option" "./fiver history hist.txt --bogus" 1 "Unknown option"

# List command tests
# Test 43: List help
run_test_with_output "List help" "./fiver list --help" 0 "Usage: fiver list"

# Test 44: List default (empty)
run_test_with_output "List empty" "./fiver list" 0 "Tracked files:"

# Create some files for list testing
echo "list1" > list1.txt
run_test_with_output "Track list1" "./fiver track list1.txt" 0 "Tracked list1.txt"
echo "list2" > list2.txt
run_test_with_output "Track list2" "./fiver track list2.txt" 0 "Tracked list2.txt"
echo "list2v2" > list2.txt
run_test_with_output "Track list2 v2" "./fiver track list2.txt" 0 "Tracked list2.txt"

# Test 45: List with files
run_test_with_output "List with files" "./fiver list" 0 "list1"

# Test 46: List with sizes
run_test_with_output "List with sizes" "./fiver list --show-sizes" 0 "TotalDelta"

# Test 47: List json format
run_test_with_output "List json" "./fiver list --format json" 0 "\"files\""

# Test 48: List unknown option
run_test_with_output "List unknown option" "./fiver list --bogus" 1 "Unknown option"

# Status command tests
# Test 49: Status help
run_test_with_output "Status help" "./fiver status --help" 0 "Usage: fiver status"

# Test 50: Status missing file
run_test_with_output "Status missing file" "./fiver status" 1 "missing file argument"

# Test 51: Status no versions
run_test_with_output "Status no versions" "./fiver status untracked_status.txt" 1 "No versions found"

# Create file for status testing
echo "status content" > status_test.txt
run_test_with_output "Track for status" "./fiver track status_test.txt" 0 "Tracked status_test.txt"

# Test 52: Status default
run_test_with_output "Status default" "./fiver status status_test.txt" 0 "Tracked: yes"

# Test 53: Status json
run_test_with_output "Status json" "./fiver status status_test.txt --json" 0 "\"tracked\": true"

# Test 54: Status unknown option
run_test_with_output "Status unknown option" "./fiver status status_test.txt --bogus" 1 "Unknown option"

# Test 26: Track with message flag
echo "test content" > message_test.txt
run_test_with_output "Track with message" "./fiver track message_test.txt --message 'Test message'" 0 "Tracked message_test.txt"

# Test 27: Track with short message flag
run_test_with_output "Track with short message flag" "./fiver track message_test.txt -m 'Short flag test'" 0 "Tracked message_test.txt"

# Test 28: Track without message (should work)
run_test_with_output "Track without message" "./fiver track message_test.txt" 0 "Tracked message_test.txt"

# Test 29: Message flag without value
run_test_with_output "Message flag without value" "./fiver track message_test.txt --message" 1 "requires a value"

# Test 30: Message too long
long_message=$(printf 'a%.0s' {1..300})  # Create a 300-character string
run_test_with_output "Message too long" "./fiver track message_test.txt --message '$long_message'" 1 "Message is too long"

echo ""
echo -e "${YELLOW}==========================================${NC}"
echo -e "${YELLOW}  STORAGE VERIFICATION TESTS${NC}"
echo -e "${YELLOW}==========================================${NC}"

# Test 40: Verify storage structure
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
