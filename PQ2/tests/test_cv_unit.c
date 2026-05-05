#include "cond_var.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int failed = 0;
    condition_variable cv;
    condition_variable_init(&cv);

    // Test 1: signal when nobody is waiting should not crash
    condition_variable_signal(&cv);
    if (atomic_load(&cv.waiters) != 0) {
        printf("FAIL! waiters should be 0 after signal with no waiters, got %d\n", atomic_load(&cv.waiters));
        failed = 1;
    } else {
        printf("PASS: signal with no waiters does nothing\n");
    }

    // Test 2: broadcast when nobody is waiting should not crash
    condition_variable_broadcast(&cv);
    if (atomic_load(&cv.waiters) != 0) {
        printf("FAIL! waiters should be 0 after broadcast with no waiters, got %d\n", atomic_load(&cv.waiters));
        failed = 1;
    } else {
        printf("PASS! broadcast with no waiters does nothing\n");
    }

    return failed;
}
