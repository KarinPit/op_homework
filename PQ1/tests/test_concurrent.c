#include "tl_semaphore.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>

#define NUM_THREADS 8

static semaphore sem;
static atomic_int finished_count;

static void *thread_func(void *arg) {
    (void)arg;
    semaphore_wait(&sem);
    atomic_fetch_add(&finished_count, 1);
    return NULL;
}

int main(void) {
    pthread_t threads[NUM_THREADS];
    atomic_init(&finished_count, 0);
    semaphore_init(&sem, 0);

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, thread_func, NULL);

    for (int i = 0; i < NUM_THREADS; i++) {
        semaphore_signal(&sem);
        printf("signal %d sent\n", i + 1);
    }

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    if (atomic_load(&finished_count) != NUM_THREADS) {
        printf("FAIL: only %d/%d threads finished\n", atomic_load(&finished_count), NUM_THREADS);
        return 1;
    }

    printf("PASS: all %d threads finished\n", NUM_THREADS);
    return 0;
}
