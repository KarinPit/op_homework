#ifndef COND_VAR_H
#define COND_VAR_H

#include "tl_semaphore.h"

typedef struct {
    atomic_int waiters;
    ticket_lock lock;
    semaphore sem;
} condition_variable;

void condition_variable_init(condition_variable *cv);
void condition_variable_wait(condition_variable *cv, ticket_lock *ext_lock);
void condition_variable_signal(condition_variable *cv);
void condition_variable_broadcast(condition_variable *cv);

#endif
//
