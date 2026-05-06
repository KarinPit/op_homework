#include <stdio.h>
#include "rw_lock.h"

int main(void) {
            rw_lock rw;
    
    rwlock_init(&rw);
    
    rwlock_acquire_read(&rw);
    rwlock_release_read(&rw);
    
    rwlock_acquire_write(&rw);
    rwlock_release_write(&rw);
    
    printf("Unit test passed successfully.\n");
    return 0; 
}
//
