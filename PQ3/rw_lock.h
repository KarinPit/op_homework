#ifndef RW_LOCK_H
#define RW_LOCK_H

#include "cond_var.h" 

typedef struct {
    ticket_lock lk;                   
    condition_variable read_cv;      
    condition_variable write_cv;    
    
    int active_readers;         
    int active_writers;              
    int waiting_writers;             
    int waiting_readers;             
} rw_lock;

void rwlock_init(rw_lock* rw);
void rwlock_acquire_read(rw_lock* rw);
void rwlock_release_read(rw_lock* rw);
void rwlock_acquire_write(rw_lock* rw);
void rwlock_release_write(rw_lock* rw);

#endif
//
