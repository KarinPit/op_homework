#include "tl_semaphore.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    int failed = 0;
    semaphore sem;

    // Test 1: basic wait then signal
    semaphore_init(&sem, 1);
    semaphore_wait(&sem);
    if (atomic_load(&sem.value) != 0)
    {
        printf("FAIL: after wait, value should be 0 but got %d\n", atomic_load(&sem.value));
        failed = 1;
    }
    else
    {
        printf("PASS: value is 0 after wait\n");
    }
    semaphore_signal(&sem);
    if (atomic_load(&sem.value) != 1)
    {
        printf("FAIL: after signal, value should be 1 but got %d\n", atomic_load(&sem.value));
        failed = 1;
    }
    else
    {
        printf("PASS: value is 1 after signal\n");
    }

    // Test 2: signal without wait
    semaphore_init(&sem, 0);
    semaphore_signal(&sem);
    if (atomic_load(&sem.value) != 1)
    {
        printf("FAIL: after signal on 0, value should be 1 but got %d\n", atomic_load(&sem.value));
        failed = 1;
    }
    else
    {
        printf("PASS: signal on value=0 works correctly\n");
    }

    return failed;
}
