#include "tl_semaphore.h"
#include <sched.h>

void ticketlock_init(ticket_lock *lock)
{
    atomic_init(&lock->ticket, 0);
    atomic_init(&lock->cur_ticket, 0);
}

void ticketlock_acquire(ticket_lock *lock)
{
    int my_ticket = atomic_fetch_add(&lock->ticket, 1);
    while (atomic_load(&lock->cur_ticket) != my_ticket)
        sched_yield();
}

void ticketlock_release(ticket_lock *lock)
{
    atomic_fetch_add(&lock->cur_ticket, 1);
}

void semaphore_init(semaphore *sem, int initial_value)
{
    atomic_init(&sem->value, initial_value);
    ticketlock_init(&sem->lock);
}

void semaphore_wait(semaphore *sem)
{
    while (1)
    {
        ticketlock_acquire(&sem->lock);
        if (atomic_load(&sem->value) > 0)
        {
            atomic_fetch_add(&sem->value, -1);
            ticketlock_release(&sem->lock);
            return;
        }
        ticketlock_release(&sem->lock);
        sched_yield();
    }
}

void semaphore_signal(semaphore *sem)
{
    ticketlock_acquire(&sem->lock);
    atomic_fetch_add(&sem->value, 1);
    ticketlock_release(&sem->lock);
}
