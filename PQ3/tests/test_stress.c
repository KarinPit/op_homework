#include <stdio.h>
#include <pthread.h>
#include "rw_lock.h"

#define NUM_READERS 10
#define NUM_WRITERS 4
#define ITERATIONS 10000

rw_lock rw;
int shared_resource = 0;

void* reader_thread(void* arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        rwlock_acquire_read(&rw);
        // רק קוראים, לא משנים כלום
        rwlock_release_read(&rw);
    }
    return NULL;
}

void* writer_thread(void* arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        rwlock_acquire_write(&rw);
        shared_resource++; // מעלים את המונה
        rwlock_release_write(&rw);
    }
    return NULL;
}

int main(void) {
    pthread_t readers[NUM_READERS];
    pthread_t writers[NUM_WRITERS];
    
    rwlock_init(&rw);
    
    // יצירת כל התהליכונים
    for (int i = 0; i < NUM_READERS; i++) {
        pthread_create(&readers[i], NULL, reader_thread, NULL);
    }
    for (int i = 0; i < NUM_WRITERS; i++) {
        pthread_create(&writers[i], NULL, writer_thread, NULL);
    }
    
    // המתנה שכולם יסיימו את עשרות אלפי הפעולות שלהם
    for (int i = 0; i < NUM_READERS; i++) {
        pthread_join(readers[i], NULL);
    }
    for (int i = 0; i < NUM_WRITERS; i++) {
        pthread_join(writers[i], NULL);
    }
    
    int expected = NUM_WRITERS * ITERATIONS;
    if (shared_resource == expected) {
        printf("Stress test passed successfully. Final value: %d\n", shared_resource);
        return 0;
    } else {
        printf("Stress test FAILED! Expected %d, got %d\n", expected, shared_resource);
        return 1;
    }
}

