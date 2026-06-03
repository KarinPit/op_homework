#include "cond_var.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>

#define NUM_THREADS 8

static condition_variable cv;
static ticket_lock ext_lock;
static atomic_int released_count;

static void *thread_func(void *arg) {
    (void)arg;
    ticketlock_acquire(&ext_lock);
    condition_variable_wait(&cv, &ext_lock);
    atomic_fetch_add(&released_count, 1);
    ticketlock_release(&ext_lock);
    return NULL;
}

int main(void) {
    int failed = 0;
    condition_variable_init(&cv);
    ticketlock_init(&ext_lock);
    atomic_init(&released_count, 0);

    pthread_t threads[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, thread_func, NULL);

    // give threads time to start waiting
    struct timespec ts = {0, 10000000}; // 10ms
    nanosleep(&ts, NULL);

    for (int i = 0; i < NUM_THREADS; i++) {
        condition_variable_signal(&cv);
        nanosleep(&ts, NULL);
        int count = atomic_load(&released_count);
        if (count != i + 1) {
            printf("FAIL! after signal %d, released count should be %d but got %d\n", i + 1, i + 1, count);
            failed = 1;
        } else {
            printf("PASS! after signal %d, exactly %d thread(s) released\n", i + 1, i + 1);
        }
    }

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    return failed;
}
