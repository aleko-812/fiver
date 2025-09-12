#!/bin/bash

# Static Analysis for Memory Leak Detection
# Uses various static analysis tools to detect potential memory issues

set -e

echo "🔍 Static Analysis for Memory Leak Detection"
echo "============================================="

# Check if tools are available
check_tool() {
    if ! command -v "$1" &> /dev/null; then
        echo "❌ $1 not found. Install with: $2"
        return 1
    fi
    echo "✅ $1 found"
    return 0
}

# Check for static analysis tools
echo "Checking for static analysis tools..."

# cppcheck
if check_tool "cppcheck" "sudo apt install cppcheck"; then
    echo -e "\n🔍 Running cppcheck..."
    cppcheck --enable=all --inconclusive --std=c99 \
        --suppress=missingIncludeSystem \
        --suppress=unusedFunction \
        --suppress=unmatchedSuppression \
        src/ include/ 2>&1 | tee cppcheck_report.txt

    if grep -q "error:" cppcheck_report.txt; then
        echo "❌ cppcheck found potential issues"
    else
        echo "✅ cppcheck: No major issues found"
    fi
fi

# scan-build (clang static analyzer)
if check_tool "scan-build" "sudo apt install clang-tools"; then
    echo -e "\n🔍 Running clang static analyzer..."
    scan-build -o scan-build-report make clean all 2>&1 | tee scan_build_report.txt

    if [ -d "scan-build-report" ]; then
        echo "📊 Scan-build report generated in scan-build-report/"
        echo "Open scan-build-report/index.html in a browser to view detailed results"
    fi
fi

# AddressSanitizer build
echo -e "\n🔍 Building with AddressSanitizer..."
make clean
make CFLAGS="-g -O1 -fsanitize=address -fno-omit-frame-pointer -Wall -Wextra -std=c99 -Iinclude"

echo "Testing AddressSanitizer build..."
echo "This is a test file for AddressSanitizer." > asan_test.txt

# Run a simple test with AddressSanitizer
./fiver track asan_test.txt -m "ASan test" 2>&1 | tee asan_output.txt

if grep -q "ERROR: AddressSanitizer" asan_output.txt; then
    echo "❌ AddressSanitizer detected memory issues"
    grep -A 5 -B 5 "ERROR: AddressSanitizer" asan_output.txt
else
    echo "✅ AddressSanitizer: No memory issues detected"
fi

# MemorySanitizer build (if available)
if gcc -fsanitize=memory -x c -c /dev/null -o /dev/null 2>/dev/null; then
    echo -e "\n🔍 Building with MemorySanitizer..."
    make clean
    make CFLAGS="-g -O1 -fsanitize=memory -fno-omit-frame-pointer -Wall -Wextra -std=c99 -Iinclude"

    echo "Testing MemorySanitizer build..."
    ./fiver track asan_test.txt -m "MSan test" 2>&1 | tee msan_output.txt

    if grep -q "ERROR: MemorySanitizer" msan_output.txt; then
        echo "❌ MemorySanitizer detected memory issues"
        grep -A 5 -B 5 "ERROR: MemorySanitizer" msan_output.txt
    else
        echo "✅ MemorySanitizer: No memory issues detected"
    fi
else
    echo "⚠️  MemorySanitizer not available (requires clang)"
fi

# UndefinedBehaviorSanitizer build
echo -e "\n🔍 Building with UndefinedBehaviorSanitizer..."
make clean
make CFLAGS="-g -O1 -fsanitize=undefined -fno-omit-frame-pointer -Wall -Wextra -std=c99 -Iinclude"

echo "Testing UndefinedBehaviorSanitizer build..."
./fiver track asan_test.txt -m "UBSan test" 2>&1 | tee ubsan_output.txt

if grep -q "runtime error" ubsan_output.txt; then
    echo "❌ UndefinedBehaviorSanitizer detected issues"
    grep -A 5 -B 5 "runtime error" ubsan_output.txt
else
    echo "✅ UndefinedBehaviorSanitizer: No issues detected"
fi

# Clean up
rm -f asan_test.txt asan_output.txt msan_output.txt ubsan_output.txt
rm -rf .fiver

# Summary
echo -e "\n📊 Static Analysis Summary"
echo "=========================="

if [ -f "cppcheck_report.txt" ]; then
    echo "cppcheck report: cppcheck_report.txt"
fi

if [ -d "scan-build-report" ]; then
    echo "clang static analyzer report: scan-build-report/"
fi

echo -e "\n✅ Static analysis complete!"
echo "Review the generated reports for detailed findings."
