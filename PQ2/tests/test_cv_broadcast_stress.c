#include "cond_var.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>

#define NUM_THREADS 16
#define ITERATIONS  100

static condition_variable cv;
static ticket_lock ext_lock;
static atomic_int released_count;
static atomic_int phase;

static void *thread_func(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        while (atomic_load(&phase) != i)
            sched_yield();

        ticketlock_acquire(&ext_lock);
        condition_variable_wait(&cv, &ext_lock);
        atomic_fetch_add(&released_count, 1);
        ticketlock_release(&ext_lock);
    }
    return NULL;
}

int main(void) {
    int failed = 0;
    condition_variable_init(&cv);
    ticketlock_init(&ext_lock);
    atomic_init(&released_count, 0);
    atomic_init(&phase, -1);

    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, thread_func, NULL);

    for (int i = 0; i < ITERATIONS; i++) {
        atomic_store(&released_count, 0);
        atomic_store(&phase, i);

        while (atomic_load(&cv.waiters) < NUM_THREADS)
            sched_yield();

        condition_variable_broadcast(&cv);

        while (atomic_load(&released_count) < NUM_THREADS)
            sched_yield();

        if (atomic_load(&released_count) != NUM_THREADS) {
            printf("FAIL! iteration %d released %d/%d threads\n", i + 1, atomic_load(&released_count), NUM_THREADS);
            failed = 1;
        }
    }

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    if (!failed)
        printf("PASS! all %d iterations released all %d threads\n", ITERATIONS, NUM_THREADS);

    return failed;
}
