#include "rw_lock.h"

#include "rw_lock.h"

void rwlock_init(rw_lock* rw) {
    ticketlock_init(&rw->lk);
    condition_variable_init(&rw->read_cv); 
    condition_variable_init(&rw->write_cv);
    rw->active_readers = 0;
    rw->active_writers = 0;
    rw->waiting_writers = 0;
    rw->waiting_readers = 0;
}

void rwlock_acquire_read(rw_lock* rw) {
    ticketlock_acquire(&rw->lk);
    rw->waiting_readers++;
    
    while (rw->active_writers > 0 || rw->waiting_writers > 0) {
        condition_variable_wait(&rw->read_cv, &rw->lk); 
    }
    
    rw->waiting_readers--;
    rw->active_readers++;
    ticketlock_release(&rw->lk);
}

void rwlock_release_read(rw_lock* rw) {
    ticketlock_acquire(&rw->lk);
    rw->active_readers--;
    
    if (rw->active_readers == 0 && rw->waiting_writers > 0) {
        condition_variable_signal(&rw->write_cv);
    }
    ticketlock_release(&rw->lk);
}

void rwlock_acquire_write(rw_lock* rw) {
    ticketlock_acquire(&rw->lk);
    rw->waiting_writers++; 
    
    while (rw->active_readers > 0 || rw->active_writers > 0) {
        condition_variable_wait(&rw->write_cv, &rw->lk);
    }
    
    rw->waiting_writers--;
    rw->active_writers = 1;
    ticketlock_release(&rw->lk);
}

void rwlock_release_write(rw_lock* rw) {
    ticketlock_acquire(&rw->lk);
    rw->active_writers = 0;
    
    if (rw->waiting_writers > 0) {
        condition_variable_signal(&rw->write_cv);
    } 

    else {
   
        
        for (int i = 0; i < rw->waiting_readers; i++) {
            condition_variable_signal(&rw->read_cv);
        }
    }
    ticketlock_release(&rw->lk);
}
//
