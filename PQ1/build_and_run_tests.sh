#!/bin/bash

CC=gcc; command -v gcc-13 >/dev/null && CC=gcc-13
CFLAGS="-std=c17 -Wall -Wextra -Werror -pedantic -g"

mkdir -p output

PASS=0
FAIL=0

compile_and_run() {
    local test_src=$1
    local test_name=$2
    local extra_flags=$3

    $CC $CFLAGS $extra_flags -I. tests/${test_src} tl_semaphore.c -o output/${test_name}
    if [ $? -ne 0 ]; then
        echo "COMPILE FAILED: ${test_src}"
        FAIL=$((FAIL + 1))
        return
    fi

    echo "Running ${test_name}..."
    ./output/${test_name}
    if [ $? -eq 0 ]; then
        echo "PASS: ${test_name}"
        PASS=$((PASS + 1))
    else
        echo "FAIL: ${test_name}"
        FAIL=$((FAIL + 1))
    fi
}

compile_and_run "test_unit.c"       "test_unit"       ""
compile_and_run "test_concurrent.c" "test_concurrent" "-pthread"
compile_and_run "test_stress.c"     "test_stress"     "-pthread"

echo ""
echo "Results: $PASS passed, $FAIL failed"

[ $FAIL -eq 0 ] && exit 0 || exit 1
