#include "tl_semaphore.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>

#define NUM_THREADS 16
#define OPS_PER_THREAD 10000
#define INITIAL_VALUE 10

static semaphore sem;

static void *thread_func(void *arg) {
    (void)arg;
    for (int i = 0; i < OPS_PER_THREAD; i++) {
        semaphore_wait(&sem);
        semaphore_signal(&sem);
    }
    return NULL;
}

int main(void) {
    pthread_t threads[NUM_THREADS];
    semaphore_init(&sem, INITIAL_VALUE);

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, thread_func, NULL);

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    int final_value = atomic_load(&sem.value);
    if (final_value != INITIAL_VALUE) {
        printf("FAIL! final value should be %d but got %d\n", INITIAL_VALUE, final_value);
        return 1;
    }

    printf("PASS! final value is %d after %d threads x %d ops\n", final_value, NUM_THREADS, OPS_PER_THREAD);
    return 0;
}
