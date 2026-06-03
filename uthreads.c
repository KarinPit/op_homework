#include "uthreads.h"

/* ===================================================================== */
/* Global Variables                            */
/* ===================================================================== */

thread_t threads[MAX_THREAD_NUM];

//char stacks[MAX_THREAD_NUM][STACK_SIZE];
// מכריח את הקומפיילר ליישר את המחסנית ל-16 בתים, חובה במעבדי ARM64 (Mac M1/M2/M3)
__attribute__((aligned(16))) char stacks[MAX_THREAD_NUM][STACK_SIZE];
int global_quantum_usecs = 0;
int current_running_tid = -1;
int total_quantum_count = 0;

int ready_queue[MAX_THREAD_NUM];
bool is_explicitly_blocked[MAX_THREAD_NUM] = {false};
int ready_head = 0;
int ready_tail = 0;
int ready_count = 0;

/* ===================================================================== */
/* Helper Functions                            */
/* ===================================================================== */

void print_system_error(const char* msg) {
    fprintf(stderr, "system error: %s\n", msg);
    exit(1);
}

int print_library_error(const char* msg) {
    fprintf(stderr, "thread library error: %s\n", msg);
    return -1;
}

void remove_from_ready_queue(int tid) {
    int temp_queue[MAX_THREAD_NUM];
    int new_count = 0;
    
    for (int i = 0; i < ready_count; i++) {
        if (ready_queue[i] != tid) {
            temp_queue[new_count++] = ready_queue[i];
        }
    }
    
    for (int i = 0; i < new_count; i++) {
        ready_queue[i] = temp_queue[i];
    }
    ready_count = new_count;
}
/* ===================================================================== */
/* Internal Queue & Signal Helpers                                       */
/* ===================================================================== */

void block_signals(void) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGVTALRM);
    if (sigprocmask(SIG_BLOCK, &set, NULL) < 0) {
        print_system_error("sigprocmask block failed.");
    }
}

void unblock_signals(void) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGVTALRM);
    if (sigprocmask(SIG_UNBLOCK, &set, NULL) < 0) {
        print_system_error("sigprocmask unblock failed.");
    }
}

void enqueue(int tid) {
    if (ready_count < MAX_THREAD_NUM) {
        ready_queue[ready_count++] = tid;
    }
}

int dequeue(void) {
    if (ready_count == 0) {
        return -1;
    }
    int tid = ready_queue[0];
    for (int i = 1; i < ready_count; i++) {
        ready_queue[i - 1] = ready_queue[i];
    }
    ready_count--;
    return tid;
}
/* ===================================================================== */
/* Scheduling & Context Switching                                        */
/* ===================================================================== */

void schedule_next(void) {
    int current = current_running_tid;
    int next_quantum = total_quantum_count + 1;

    // 1. קודם כל עוברים על כל החוטים ומכניסים לתור את מי שסיים לישון
    for (int i = 1; i < MAX_THREAD_NUM; i++) {
        if (threads[i].state == THREAD_BLOCKED && threads[i].sleep_until > 0) {
            if (threads[i].sleep_until <= next_quantum) {
                threads[i].sleep_until = 0; 
                
                if (!is_explicitly_blocked[i]) {
                    threads[i].state = THREAD_READY;
                    enqueue(i);
                }
            }
        }
    }

    // 2. רק עכשיו נכניס לסוף התור את החוט שרץ כרגע (אם הוא לא חסום/מת)
    if (threads[current].state == THREAD_RUNNING) {
        threads[current].state = THREAD_READY;
        enqueue(current);
    }

    // 3. שולפים את החוט הבא
    int next = dequeue();
    if (next == -1) {
        next = current;
    }

    current_running_tid = next;
    threads[next].state = THREAD_RUNNING;
    threads[next].quantums++;
    total_quantum_count++;

    struct itimerval timer;
    timer.it_value.tv_sec = global_quantum_usecs / 1000000;
    timer.it_value.tv_usec = global_quantum_usecs % 1000000;
    timer.it_interval.tv_sec = global_quantum_usecs / 1000000;
    timer.it_interval.tv_usec = global_quantum_usecs % 1000000;
    
    if (setitimer(ITIMER_VIRTUAL, &timer, NULL) < 0) {
        print_system_error("setitimer reset failed in schedule_next.");
    }

   if (current != next) {
        int ret_val = sigsetjmp(threads[current].env, 1);
        
        if (ret_val == 0) {
            unblock_signals();
            siglongjmp(threads[next].env, 1);
        }
        unblock_signals();
    } else {
        unblock_signals();
    }
}


void timer_handler(int signum) {
    (void)signum; 
    schedule_next();
}
/* ===================================================================== */
/* External Interface                          */
/* ===================================================================== */

int uthread_init(int quantum_usecs) {
    global_quantum_usecs = quantum_usecs;
    if (quantum_usecs <= 0) {
        return print_library_error("quantum_usecs must be strictly positive.");
    }

    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        threads[i].state = THREAD_UNUSED;
    }

    threads[0].tid = 0;
    threads[0].state = THREAD_RUNNING;
    threads[0].quantums = 1;
    threads[0].sleep_until = 0;
    
    current_running_tid = 0;
    total_quantum_count = 1;

    struct sigaction sa;
    sa.sa_handler = &timer_handler;
    if (sigemptyset(&sa.sa_mask) < 0) {
        print_system_error("sigemptyset failed.");
    }
    sa.sa_flags = 0;
    
    if (sigaction(SIGVTALRM, &sa, NULL) < 0) {
        print_system_error("sigaction failed.");
    }

    struct itimerval timer;
    timer.it_value.tv_sec = quantum_usecs / 1000000;
    timer.it_value.tv_usec = quantum_usecs % 1000000;
    timer.it_interval.tv_sec = quantum_usecs / 1000000;
    timer.it_interval.tv_usec = quantum_usecs % 1000000;

    if (setitimer(ITIMER_VIRTUAL, &timer, NULL) < 0) {
        print_system_error("setitimer failed.");
    }

    return 0;
}

int uthread_get_tid() {
    return current_running_tid;
}

int uthread_get_total_quantums() {
    return total_quantum_count;
}

int uthread_get_quantums(int tid) {
    if (tid < 0 || tid >= MAX_THREAD_NUM || threads[tid].state == THREAD_UNUSED) {
        return print_library_error("Thread does not exist.");
    }
    return threads[tid].quantums;
}
int uthread_spawn(thread_entry_point entry_point) {
    if (entry_point == NULL) {
        return print_library_error("entry_point cannot be NULL.");
    }

    block_signals();

    int new_tid = -1;
    for (int i = 1; i < MAX_THREAD_NUM; i++) {
        if (threads[i].state == THREAD_UNUSED) {
            new_tid = i;
            break;
        }
    }

    if (new_tid == -1) {
        unblock_signals();
        return print_library_error("exceeded maximum number of threads.");
    }

    is_explicitly_blocked[new_tid] = false; 

    threads[new_tid].tid = new_tid;
    threads[new_tid].state = THREAD_READY;
    threads[new_tid].quantums = 0;
    threads[new_tid].sleep_until = 0;
    threads[new_tid].entry = entry_point;

    setup_thread(new_tid, stacks[new_tid], entry_point);

    enqueue(new_tid);

    unblock_signals();

    return new_tid;
}
void setup_thread(int tid, char *stack, thread_entry_point entry_point) {
    setup_jmpbuff(&threads[tid].env, stack, STACK_SIZE, entry_point);
}

int uthread_terminate(int tid) {
    block_signals();

    if (tid < 0 || tid >= MAX_THREAD_NUM || threads[tid].state == THREAD_UNUSED) {
        unblock_signals();
        return print_library_error("Cannot terminate a non-existent thread.");
    }

    if (tid == 0) {
        exit(0);
    }

    threads[tid].state = THREAD_UNUSED;

    remove_from_ready_queue(tid);

    if (current_running_tid == tid) {
        schedule_next();
    } else {
        unblock_signals();
    }

    return 0;
}
/* ===================================================================== */
/* Block & Resume                         */
/* ===================================================================== */
int uthread_block(int tid) {
    block_signals();

    if (tid < 0 || tid >= MAX_THREAD_NUM || threads[tid].state == THREAD_UNUSED) {
        unblock_signals();
        return print_library_error("Cannot block a non-existent thread.");
    }
    if (tid == 0) {
        unblock_signals();
        return print_library_error("Cannot block the main thread.");
    }

    is_explicitly_blocked[tid] = true;

    if (threads[tid].state == THREAD_READY) {
        remove_from_ready_queue(tid);
        threads[tid].state = THREAD_BLOCKED;
        
    } else if (tid == current_running_tid) {
        threads[tid].state = THREAD_BLOCKED;
        schedule_next(); 
      
        return 0;
        
    } else {
        threads[tid].state = THREAD_BLOCKED;
    }

    unblock_signals();
    return 0;
}
int uthread_resume(int tid) {
    block_signals();

    if (tid < 0 || tid >= MAX_THREAD_NUM || threads[tid].state == THREAD_UNUSED) {
        unblock_signals();
        return print_library_error("Cannot resume a non-existent thread.");
    }

    is_explicitly_blocked[tid] = false;

    
    if (threads[tid].state == THREAD_BLOCKED && threads[tid].sleep_until <= total_quantum_count) {
        threads[tid].state = THREAD_READY;
        enqueue(tid);
    }

    unblock_signals();
    return 0;
}
int uthread_sleep(int num_quantums) {
    block_signals();

    if (current_running_tid == 0) {
        unblock_signals();
        return print_library_error("The main thread cannot go to sleep.");
    }
    if (num_quantums <= 0) {
        unblock_signals();
        return print_library_error("Sleep duration must be strictly positive.");
    }

    threads[current_running_tid].sleep_until = total_quantum_count + num_quantums + 1;
    
    threads[current_running_tid].state = THREAD_BLOCKED;

    schedule_next();


    return 0;
}
