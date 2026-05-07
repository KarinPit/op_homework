#!/bin/bash

mkdir -p output

CFLAGS="-std=c17 -Wall -Wextra -Werror -pedantic -g -pthread -I."
SRC_FILES="rw_lock.c cond_var.c tl_semaphore.c"

echo "--- Starting Compilation ---"

echo "Compiling test_unit..."
if ! gcc $CFLAGS tests/test_unit.c $SRC_FILES -o output/test_unit; then
    echo "Error: Compilation of test_unit failed!"
    exit 1
fi

if [ -f "tests/test_concurrent.c" ]; then
    echo "Compiling test_concurrent..."
    if ! gcc $CFLAGS tests/test_concurrent.c $SRC_FILES -o output/test_concurrent; then
        echo "Error: Compilation of test_concurrent failed!"
        exit 1
    fi
fi

if [ -f "tests/test_stress.c" ]; then
    echo "Compiling test_stress..."
    if ! gcc $CFLAGS tests/test_stress.c $SRC_FILES -o output/test_stress; then
        echo "Error: Compilation of test_stress failed!"
        exit 1
    fi
fi

echo "--- Compilation Successful ---"
echo ""
echo "--- Running Tests ---"

echo ">>> Running test_unit..."
if ! ./output/test_unit; then
    echo "Error: test_unit failed!"
    exit 1
fi

if [ -f "tests/test_concurrent.c" ]; then
    echo ">>> Running test_concurrent..."
    if ! ./output/test_concurrent; then
        echo "Error: test_concurrent failed!"
        exit 1
    fi
fi

if [ -f "tests/test_stress.c" ]; then
    echo ">>> Running test_stress..."
    if ! ./output/test_stress; then
        echo "Error: test_stress failed!"
        exit 1
    fi
fi

echo ""
echo "======================================"
echo " SUCCESS: All tests passed perfectly! "
echo "======================================"
exit 0

