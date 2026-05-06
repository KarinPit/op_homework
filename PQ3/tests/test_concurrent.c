#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "rw_lock.h"

rw_lock rw;
int shared_resource = 0;

void* reader_thread(void* arg) {
    rwlock_acquire_read(&rw);
    int temp = shared_resource; 
    rwlock_release_read(&rw);
    return NULL;
}

void* writer_thread(void* arg) {
    rwlock_acquire_write(&rw);
    shared_resource++; 
    rwlock_release_write(&rw);
    return NULL;
}

int main() {
    pthread_t readers[8];
    pthread_t writers[2];
    
    rwlock_init(&rw);
    
    for (int i = 0; i < 8; i++) {
        pthread_create(&readers[i], NULL, reader_thread, NULL);
    }
    for (int i = 0; i < 2; i++) {
        pthread_create(&writers[i], NULL, writer_thread, NULL);
    }
    
    for (int i = 0; i < 8; i++) {
        pthread_join(readers[i], NULL);
    }
    for (int i = 0; i < 2; i++) {
        pthread_join(writers[i], NULL);
    }
    
    if (shared_resource == 2) {
        printf("Concurrency test passed successfully.\n");
        return 0;
    } else {
        printf("Concurrency test FAILED! Expected 2, got %d\n", shared_resource);
        return 1;
    }
}
//
