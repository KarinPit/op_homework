#include "cond_var.h"
#include <sched.h>

void condition_variable_init(condition_variable *cv) {
    atomic_init(&cv->waiters, 0);
    ticketlock_init(&cv->lock);
    semaphore_init(&cv->sem, 0);
}

void condition_variable_wait(condition_variable *cv, ticket_lock *ext_lock) {
    atomic_fetch_add(&cv->waiters, 1);
    ticketlock_release(ext_lock);

    semaphore_wait(&cv->sem);
    ticketlock_acquire(ext_lock);
}

void condition_variable_signal(condition_variable *cv) {
    ticketlock_acquire(&cv->lock);
    if (atomic_load(&cv->waiters) > 0) {
        atomic_fetch_add(&cv->waiters, -1);
        semaphore_signal(&cv->sem);
    }
    ticketlock_release(&cv->lock);
}

void condition_variable_broadcast(condition_variable *cv) {
    ticketlock_acquire(&cv->lock);
    while (atomic_load(&cv->waiters) > 0) {
        atomic_fetch_add(&cv->waiters, -1);
        semaphore_signal(&cv->sem);
    }
    ticketlock_release(&cv->lock);
}
//
