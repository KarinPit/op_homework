/*
 * testuthreads.c
 *
 * Comprehensive test suite for the uthreads library (Part B, Exercise 2).
 *
 * Rules followed (from the PDF):
 *  - Library must not print to stdout; only stderr for errors.
 *  - No malloc/free; static arrays only.
 *  - MAX_THREAD_NUM = 100, STACK_SIZE = 4096.
 *  - Round-Robin scheduler with SIGVTALRM-based virtual timer.
 *  - Total quantum count starts at 1 after uthread_init.
 *  - uthread_sleep: current quantum not counted; sleep_until = total_q + num_quantums.
 *  - uthread_block / uthread_resume: dual-reason blocking.
 *  - Smallest available TID is always reused on spawn.
 *  - Terminating tid==0 terminates the entire process (exit 0).
 *  - Error output format: "thread library error: ..." or "system error: ...".
 *
 * Each test prints PASS/FAIL to stdout.
 * Do NOT submit this file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include "uthreads.h"

/* ===================================================================== */
/*                          Test Helpers                                 */
/* ===================================================================== */

static int test_number = 0;
static int passed = 0;
static int failed = 0;

#define ASSERT(cond, msg)                                               \
    do {                                                                \
        if (cond) {                                                     \
            printf("[PASS] Test %d: %s\n", test_number, msg);          \
            passed++;                                                   \
        } else {                                                        \
            printf("[FAIL] Test %d: %s\n", test_number, msg);          \
            failed++;                                                   \
        }                                                               \
        test_number++;                                                  \
    } while (0)

/* Quantum length used for most tests (100ms of virtual CPU time). */
#define QUANTUM_USECS 100000

/* ===================================================================== */
/*                     Shared inter-thread state                         */
/* ===================================================================== */

/* Used by multiple tests. */
static volatile int g_order[200];
static volatile int g_order_idx = 0;

static void reset_globals(void)
{
    g_order_idx = 0;
    for (int i = 0; i < 200; i++) g_order[i] = -1;
}

/* ===================================================================== */
/*           Helper: fork a child, mask SIGVTALRM first so the          */
/*           inherited timer cannot fire in the child before it is      */
/*           ready.  Returns child pid in parent; 0 in child.           */
/* ===================================================================== */

static pid_t safe_fork(void)
{
    sigset_t block_vtalrm, old_mask;
    sigemptyset(&block_vtalrm);
    sigaddset(&block_vtalrm, SIGVTALRM);
    sigprocmask(SIG_BLOCK, &block_vtalrm, &old_mask);

    pid_t pid = fork();

    /* Parent: restore previous mask so the timer keeps ticking. */
    if (pid != 0) {
        sigprocmask(SIG_SETMASK, &old_mask, NULL);
    }
    /* Child: keep SIGVTALRM blocked; child will set up its own timer
     * via uthread_init if needed, which will unblock or reset it. */
    return pid;
}

/* ===================================================================== */
/*           TEST GROUP 1 – uthread_init                                 */
/* ===================================================================== */

static void test_init(void)
{
    printf("\n--- GROUP 1: uthread_init ---\n");

    /*
     * NOTE: uthread_init() was already called successfully in main().
     * We verify the post-init state here; we do NOT call it again in this
     * process (double-init is undefined behaviour).
     *
     * 1.1 Valid quantum — verified by the fact that main() did not return
     *     early after calling uthread_init(QUANTUM_USECS).
     */
    ASSERT(1, "uthread_init with valid quantum did not fail (checked in main)");

    /* 1.2 After init, main thread (tid 0) is RUNNING */
    ASSERT(uthread_get_tid() == 0, "main thread tid is 0 after init");

    /* 1.3 Total quantums starts at 1 (checked immediately after init in main,
     *     before any scheduling; still >=1 here after later scheduling.) */
    ASSERT(uthread_get_total_quantums() >= 1,
           "total_quantums >= 1 after init");

    /* 1.4 Main thread quantum count >= 1 */
    ASSERT(uthread_get_quantums(0) >= 1,
           "main thread quantum count >= 1 after init");

    /* 1.5 Invalid quantum (0) must return -1.
     *     Test in a fresh child process to avoid double-init in this process. */
    pid_t pid = safe_fork();
    if (pid == 0) {
        /* child: no library state yet; init with 0 must return -1 */
        int ret = uthread_init(0);
        _exit(ret == -1 ? 42 : 43);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 42,
           "uthread_init(0) returns -1");

    /* 1.6 Invalid quantum (-1) must return -1. */
    pid = safe_fork();
    if (pid == 0) {
        int ret = uthread_init(-1);
        _exit(ret == -1 ? 42 : 43);
    }
    waitpid(pid, &status, 0);
    ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 42,
           "uthread_init(-1) returns -1");

    /* 1.7 Verify total_quantums == 1 right after a fresh init (child). */
    pid = safe_fork();
    if (pid == 0) {
        uthread_init(QUANTUM_USECS);
        int tq = uthread_get_total_quantums();
        _exit(tq == 1 ? 42 : 43);
    }
    waitpid(pid, &status, 0);
    ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 42,
           "total_quantums == 1 immediately after fresh init");

    /* 1.8 Main thread quantum count == 1 right after a fresh init (child). */
    pid = safe_fork();
    if (pid == 0) {
        uthread_init(QUANTUM_USECS);
        int q = uthread_get_quantums(0);
        _exit(q == 1 ? 42 : 43);
    }
    waitpid(pid, &status, 0);
    ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 42,
           "main thread quantum count == 1 immediately after fresh init");
}

/* ===================================================================== */
/*           TEST GROUP 2 – uthread_spawn                                */
/* ===================================================================== */

static void dummy_entry(void)
{
    uthread_terminate(uthread_get_tid());
}

static void test_spawn(void)
{
    printf("\n--- GROUP 2: uthread_spawn ---\n");

    /* 2.1 Spawn returns valid TID >= 1 */
    int tid1 = uthread_spawn(dummy_entry);
    ASSERT(tid1 == 1, "first spawn returns tid 1");

    /* 2.2 Second spawn returns tid 2 */
    int tid2 = uthread_spawn(dummy_entry);
    ASSERT(tid2 == 2, "second spawn returns tid 2");

    /* 2.3 NULL entry_point must return -1 */
    int r = uthread_spawn(NULL);
    ASSERT(r == -1, "spawn(NULL) returns -1");

    /* 2.4 New thread starts with quantum count 0 */
    ASSERT(uthread_get_quantums(tid1) == 0,
           "newly spawned thread has quantum count 0");

    /* 2.5 New thread is in READY state (not running, not blocked) —
     *     verified indirectly: get_quantums succeeds (thread exists). */
    ASSERT(uthread_get_quantums(tid2) == 0,
           "second spawned thread has quantum count 0");

    /* 2.6 Spawn up to MAX_THREAD_NUM - 1 additional threads (slots 3..99). */
    int last_tid = -1;
    for (int i = 3; i < MAX_THREAD_NUM; i++) {
        last_tid = uthread_spawn(dummy_entry);
    }
    ASSERT(last_tid == MAX_THREAD_NUM - 1,
           "spawning up to MAX_THREAD_NUM - 1 succeeds");

    /* 2.7 One more spawn when full must return -1 */
    int overflow = uthread_spawn(dummy_entry);
    ASSERT(overflow == -1,
           "spawning beyond MAX_THREAD_NUM returns -1");

    /* Terminate all extra threads to leave a clean state. */
    for (int i = 1; i < MAX_THREAD_NUM; i++) {
        uthread_terminate(i);
    }
}

/* ===================================================================== */
/*           TEST GROUP 3 – TID reuse after termination                  */
/* ===================================================================== */

static void test_tid_reuse(void)
{
    printf("\n--- GROUP 3: TID reuse ---\n");

    /* 3.1 Terminate tid 1 and re-spawn: should get tid 1 back (smallest free). */
    int t1 = uthread_spawn(dummy_entry);
    int t2 = uthread_spawn(dummy_entry);
    ASSERT(t1 == 1 && t2 == 2, "setup: tid 1 and 2 created");

    uthread_terminate(t1);   /* free slot 1 */
    int t_new = uthread_spawn(dummy_entry);
    ASSERT(t_new == 1, "after terminating tid 1, next spawn reuses tid 1");

    uthread_terminate(t_new);
    uthread_terminate(t2);

    /* 3.2 Terminate lowest of three, verify reuse again. */
    int a = uthread_spawn(dummy_entry);
    int b = uthread_spawn(dummy_entry);
    int c = uthread_spawn(dummy_entry);
    uthread_terminate(b);    /* free slot 2 (middle) */
    int d = uthread_spawn(dummy_entry);
    ASSERT(d == b, "smallest free TID (middle) is reused on spawn");

    uthread_terminate(a);
    uthread_terminate(c);
    uthread_terminate(d);
}

/* ===================================================================== */
/*           TEST GROUP 4 – uthread_terminate                            */
/* ===================================================================== */

static void test_terminate_error_cases(void)
{
    printf("\n--- GROUP 4: uthread_terminate errors ---\n");

    /* 4.1 Terminate non-existent tid returns -1 */
    int r = uthread_terminate(50);
    ASSERT(r == -1, "terminate non-existent tid returns -1");

    /* 4.2 Terminate negative tid returns -1 */
    r = uthread_terminate(-1);
    ASSERT(r == -1, "terminate tid -1 returns -1");

    /* 4.3 After terminate, get_quantums on that tid returns -1 */
    int t = uthread_spawn(dummy_entry);
    uthread_terminate(t);
    r = uthread_get_quantums(t);
    ASSERT(r == -1, "get_quantums on terminated tid returns -1");

    /* 4.4 Terminate tid 0 exits the process with code 0.
     *     We verify this in a fresh child process (uses safe_fork to avoid
     *     a stale SIGVTALRM firing before the child calls uthread_init). */
    pid_t pid = safe_fork();
    if (pid == 0) {
        uthread_init(QUANTUM_USECS);
        uthread_terminate(0);   /* must call exit(0) */
        _exit(99);              /* should not reach here */
    }
    int status = 0;
    waitpid(pid, &status, 0);
    ASSERT(WIFEXITED(status) && WEXITSTATUS(status) == 0,
           "terminate tid 0 exits process with code 0");
}

/* ===================================================================== */
/*           TEST GROUP 5 – uthread_block / uthread_resume               */
/* ===================================================================== */

static void test_block_resume_errors(void)
{
    printf("\n--- GROUP 5: uthread_block / uthread_resume errors ---\n");

    /* 5.1 Block main thread (tid 0) must return -1 */
    int r = uthread_block(0);
    ASSERT(r == -1, "block tid 0 returns -1");

    /* 5.2 Block non-existent tid returns -1 */
    r = uthread_block(99);
    ASSERT(r == -1, "block non-existent tid returns -1");

    /* 5.3 Resume non-existent tid returns -1 */
    r = uthread_resume(50);
    ASSERT(r == -1, "resume non-existent tid returns -1");

    /* 5.4 Block already-BLOCKED thread is a no-op (returns 0) */
    int t = uthread_spawn(dummy_entry);
    r = uthread_block(t);
    ASSERT(r == 0, "block READY thread returns 0");
    r = uthread_block(t);   /* second block on same thread */
    ASSERT(r == 0, "block already-BLOCKED thread is no-op (returns 0)");

    /* 5.5 Resume on RUNNING / READY thread has no effect (returns 0) */
    uthread_resume(t);      /* unblock so it can be scheduled */
    /* Now resume again — t is READY, should be no-op */
    r = uthread_resume(t);
    ASSERT(r == 0, "resume READY/RUNNING thread is no-op (returns 0)");

    uthread_terminate(t);
}

/* ===================================================================== */
/*           TEST GROUP 6 – uthread_sleep errors                         */
/* ===================================================================== */

/*
 * Two separate volatile flags per test:
 *   g_sleep_err_done  — set to 1 once the thread has finished (always).
 *   g_sleep_err_result — set to 1 if the tested call returned -1 (correct).
 * This decouples "thread completed" from "result was correct", preventing
 * an infinite loop when the library returns the wrong value.
 */
static volatile int g_sleep_err_done   = 0;
static volatile int g_sleep_err_result = 0;

static void sleep_zero_thread(void)
{
    int r = uthread_sleep(0);
    g_sleep_err_result = (r == -1) ? 1 : 0;
    g_sleep_err_done   = 1;   /* always set; loop in main exits regardless */
    uthread_terminate(uthread_get_tid());
}

static void sleep_negative_thread(void)
{
    int r = uthread_sleep(-3);
    g_sleep_err_result = (r == -1) ? 1 : 0;
    g_sleep_err_done   = 1;
    uthread_terminate(uthread_get_tid());
}

static void test_sleep_errors(void)
{
    printf("\n--- GROUP 6: uthread_sleep errors ---\n");

    /* 6.1 Main thread (tid 0) cannot call uthread_sleep. */
    int r = uthread_sleep(1);
    ASSERT(r == -1, "uthread_sleep from main thread returns -1");

    /* 6.2 uthread_sleep(0) is an error (num_quantums <= 0). */
    g_sleep_err_done   = 0;
    g_sleep_err_result = 0;
    uthread_spawn(sleep_zero_thread);
    /* Wait for the thread to finish (loop exits even if result is wrong). */
    while (!g_sleep_err_done) {
        kill(getpid(), SIGVTALRM);
    }
    ASSERT(g_sleep_err_result == 1, "uthread_sleep(0) returns -1");

    /* 6.3 uthread_sleep(-3) is an error. */
    g_sleep_err_done   = 0;
    g_sleep_err_result = 0;
    uthread_spawn(sleep_negative_thread);
    while (!g_sleep_err_done) {
        kill(getpid(), SIGVTALRM);
    }
    ASSERT(g_sleep_err_result == 1, "uthread_sleep(-3) returns -1");
}

/* ===================================================================== */
/*           TEST GROUP 7 – uthread_get_tid                              */
/* ===================================================================== */

static volatile int g_tid_seen = -1;

static void tid_check_thread(void)
{
    g_tid_seen = uthread_get_tid();
    uthread_terminate(uthread_get_tid());
}

static void test_get_tid(void)
{
    printf("\n--- GROUP 7: uthread_get_tid ---\n");
    g_tid_seen = -1;

    /* 7.1 Main thread reports tid 0 */
    ASSERT(uthread_get_tid() == 0, "main thread reports tid 0");

    /* 7.2 Spawned thread reports its own tid */
    int t = uthread_spawn(tid_check_thread);
    while (g_tid_seen == -1) {
        kill(getpid(), SIGVTALRM);
    }
    ASSERT(g_tid_seen == t, "spawned thread reports correct tid");
}

/* ===================================================================== */
/*           TEST GROUP 8 – uthread_get_total_quantums                   */
/* ===================================================================== */

static void test_total_quantums(void)
{
    printf("\n--- GROUP 8: uthread_get_total_quantums ---\n");

    /* 8.1 Starts at 1 right after init (already tested in group 1, repeat here). */
    int tq = uthread_get_total_quantums();
    ASSERT(tq >= 1, "total_quantums >= 1 at any point after init");

    /* 8.2 Sending SIGVTALRM increments total_quantums by 1. */
    int before = uthread_get_total_quantums();
    kill(getpid(), SIGVTALRM);
    int after = uthread_get_total_quantums();
    ASSERT(after == before + 1,
           "total_quantums incremented by 1 after one SIGVTALRM");

    /* 8.3 Three more SIGVTALRMs increment by 3. */
    before = uthread_get_total_quantums();
    kill(getpid(), SIGVTALRM);
    kill(getpid(), SIGVTALRM);
    kill(getpid(), SIGVTALRM);
    after = uthread_get_total_quantums();
    ASSERT(after == before + 3,
           "total_quantums incremented by 3 after three SIGVTALRMs");
}

/* ===================================================================== */
/*           TEST GROUP 9 – uthread_get_quantums                         */
/* ===================================================================== */

static void test_get_quantums_errors(void)
{
    printf("\n--- GROUP 9: uthread_get_quantums errors ---\n");

    /* 9.1 Non-existent tid returns -1 */
    int r = uthread_get_quantums(50);
    ASSERT(r == -1, "get_quantums for non-existent tid returns -1");

    /* 9.2 Negative tid returns -1 */
    r = uthread_get_quantums(-5);
    ASSERT(r == -1, "get_quantums for negative tid returns -1");

    /* 9.3 Main thread (tid 0) quantum count grows over time. */
    int q_before = uthread_get_quantums(0);
    kill(getpid(), SIGVTALRM);
    /* After a context switch back to main, its quantum should have grown. */
    int q_after = uthread_get_quantums(0);
    ASSERT(q_after > q_before,
           "main thread quantum count increases after scheduling");
}

/* ===================================================================== */
/*           TEST GROUP 10 – Round-Robin scheduling order                */
/* ===================================================================== */

/*
 * This is the reference test from tests/test.c.
 * Expected scheduling: {0, 1, 2, 0, 1, 0, 0, 2, 0, 0}
 *
 * Thread 1 (uthread_function): records tid, takes one extra quantum via
 * SIGVTALRM, records tid again, terminates.
 * Thread 2 (uthread_function2): records tid, sleeps for 4 quantums,
 * records tid again, terminates.
 * Main (tid 0): fills the scheduling array and drives the loop.
 */

/*
 * sched_arr / sched_idx are written by uthread entry functions invoked
 * across siglongjmp / signal boundaries — must be volatile.
 */
#define SCHED_LENGTH 10
static volatile int sched_arr[SCHED_LENGTH];
static volatile int sched_idx = 0;

static void rr_thread1(void)
{
    sched_arr[sched_idx++] = uthread_get_tid();   /* slot 1 */
    kill(getpid(), SIGVTALRM);                     /* force preempt */
    sched_arr[sched_idx++] = uthread_get_tid();   /* slot 4 */
    uthread_terminate(uthread_get_tid());
}

static void rr_thread2(void)
{
    sched_arr[sched_idx++] = uthread_get_tid();   /* slot 2 */
    uthread_sleep(4);
    sched_arr[sched_idx++] = uthread_get_tid();   /* slot 7 */
    uthread_terminate(uthread_get_tid());
}

static void test_round_robin(void)
{
    printf("\n--- GROUP 10: Round-Robin scheduling order ---\n");

    for (int i = 0; i < SCHED_LENGTH; i++) sched_arr[i] = -1;
    sched_idx = 0;

    uthread_spawn(rr_thread1);   /* tid 1 */
    uthread_spawn(rr_thread2);   /* tid 2 */

    while (1) {
        if (sched_idx >= SCHED_LENGTH) break;
        sched_arr[sched_idx++] = uthread_get_tid();
        kill(getpid(), SIGVTALRM);
    }

    int expected[SCHED_LENGTH] = {0, 1, 2, 0, 1, 0, 0, 2, 0, 0};
    int ok = 1;
    for (int i = 0; i < SCHED_LENGTH; i++) {
        if (sched_arr[i] != expected[i]) {
            ok = 0;
            break;
        }
    }
    ASSERT(ok, "Round-Robin scheduling matches expected order {0,1,2,0,1,0,0,2,0,0}");
}

/* ===================================================================== */
/*           TEST GROUP 11 – Sleep timing (current quantum not counted)  */
/* ===================================================================== */

static volatile int g_sleep_start_q = 0;
static volatile int g_sleep_wake_q  = 0;
static volatile int g_sleep_done    = 0;

static void sleep_timing_thread(void)
{
    g_sleep_start_q = uthread_get_total_quantums();
    uthread_sleep(3);
    g_sleep_wake_q = uthread_get_total_quantums();
    g_sleep_done   = 1;
    uthread_terminate(uthread_get_tid());
}

static void test_sleep_timing(void)
{
    printf("\n--- GROUP 11: Sleep timing ---\n");
    g_sleep_start_q = 0;
    g_sleep_wake_q  = 0;
    g_sleep_done    = 0;

    uthread_spawn(sleep_timing_thread);

    /* Drive scheduling until the thread wakes. */
    while (!g_sleep_done) {
        kill(getpid(), SIGVTALRM);
    }

    /*
     * If sleep started at quantum Q, thread must NOT run during quantums
     * Q+1, Q+2, Q+3.  It may become READY at quantum Q+4.
     * So wake quantum >= sleep_start + 4.
     */
    ASSERT(g_sleep_wake_q >= g_sleep_start_q + 4,
           "thread wakes no earlier than sleep_start + 4 quantums");

    /*
     * It should also wake no later than sleep_start + 4 + small overhead
     * (depending on how many other threads are READY, but here only main
     * and this thread exist, so overhead is <=1 extra quantum of main).
     */
    ASSERT(g_sleep_wake_q <= g_sleep_start_q + 5,
           "thread wakes within expected window (no starvation)");
}

/* ===================================================================== */
/*           TEST GROUP 12 – Dual-reason blocking                        */
/*           (sleep + explicit block requires both to clear)             */
/* ===================================================================== */

static volatile int g_dual_ran = 0;

static void dual_block_thread(void)
{
    uthread_sleep(2);
    /* If we reach here, both blocking reasons were cleared. */
    g_dual_ran = 1;
    uthread_terminate(uthread_get_tid());
}

static void test_dual_blocking(void)
{
    printf("\n--- GROUP 12: Dual-reason blocking ---\n");
    g_dual_ran = 0;

    int t = uthread_spawn(dual_block_thread);

    /* Let thread start and call uthread_sleep(2). */
    kill(getpid(), SIGVTALRM);

    /* While it is sleeping, also explicitly block it. */
    uthread_block(t);

    /* Run through more than 2 quantums — thread must NOT wake yet. */
    for (int i = 0; i < 4; i++) {
        kill(getpid(), SIGVTALRM);
    }
    ASSERT(g_dual_ran == 0,
           "thread still blocked after sleep expires (explicit block not cleared)");

    /* Now resume the explicit block. */
    uthread_resume(t);

    /* Run a few more quantums to let it execute. */
    for (int i = 0; i < 3; i++) {
        kill(getpid(), SIGVTALRM);
    }
    ASSERT(g_dual_ran == 1,
           "thread runs after both sleep and explicit block are cleared");
}

/* ===================================================================== */
/*           TEST GROUP 13 – resume does not early-wake sleeping thread  */
/* ===================================================================== */

static volatile int g_early_wake = 0;

static void early_wake_thread(void)
{
    uthread_sleep(5);
    g_early_wake = 1;
    uthread_terminate(uthread_get_tid());
}

static void test_resume_no_early_wake(void)
{
    printf("\n--- GROUP 13: resume does not wake sleeping thread early ---\n");
    g_early_wake = 0;

    int t = uthread_spawn(early_wake_thread);

    /* Let thread start and enter sleep. */
    kill(getpid(), SIGVTALRM);

    /* Immediately resume it (it was never explicitly blocked, only sleeping). */
    uthread_resume(t);

    /* Run 2 more quantums — should still be sleeping. */
    kill(getpid(), SIGVTALRM);
    kill(getpid(), SIGVTALRM);
    ASSERT(g_early_wake == 0,
           "uthread_resume on sleeping (non-blocked) thread does not wake it early");

    /* Let the sleep expire naturally. */
    for (int i = 0; i < 6; i++) {
        kill(getpid(), SIGVTALRM);
    }
    ASSERT(g_early_wake == 1,
           "thread wakes after sleep period expires naturally");
}

/* ===================================================================== */
/*           TEST GROUP 14 – block / resume of other thread              */
/*           Running thread continues running; blocked other stops.      */
/* ===================================================================== */

static volatile int g_other_ran = 0;

static void other_thread_entry(void)
{
    g_other_ran = 1;
    uthread_terminate(uthread_get_tid());
}

static void test_block_other(void)
{
    printf("\n--- GROUP 14: block/resume of other thread ---\n");
    g_other_ran = 0;

    int t = uthread_spawn(other_thread_entry);

    /* Block the spawned thread before it ever runs. */
    uthread_block(t);

    /* Run through several quantums; blocked thread must not execute. */
    for (int i = 0; i < 5; i++) {
        kill(getpid(), SIGVTALRM);
    }
    ASSERT(g_other_ran == 0, "blocked thread does not run");

    /* Resume the thread and let it run. */
    uthread_resume(t);
    for (int i = 0; i < 3; i++) {
        kill(getpid(), SIGVTALRM);
    }
    ASSERT(g_other_ran == 1, "thread runs after resume");
}

/* ===================================================================== */
/*           TEST GROUP 15 – terminate other thread (running continues)  */
/* ===================================================================== */

static volatile int g_victim_ran = 0;

static void victim_entry(void)
{
    g_victim_ran = 1;
    /* Infinite loop — we expect it to be terminated externally. */
    while (1) {
        kill(getpid(), SIGVTALRM);
    }
}

static void test_terminate_other(void)
{
    printf("\n--- GROUP 15: terminate other thread ---\n");
    g_victim_ran = 0;

    int t = uthread_spawn(victim_entry);

    /* Let it run once to set g_victim_ran. */
    kill(getpid(), SIGVTALRM);
    while (g_victim_ran == 0) {
        kill(getpid(), SIGVTALRM);
    }

    /* Terminate the other thread; main must continue. */
    int r = uthread_terminate(t);
    ASSERT(r == 0, "terminate of other thread returns 0");

    /* Verify it is gone. */
    r = uthread_get_quantums(t);
    ASSERT(r == -1, "get_quantums on terminated thread returns -1");

    /* Main is still running. */
    ASSERT(uthread_get_tid() == 0, "main thread is still running after terminating other");
}

/* ===================================================================== */
/*           TEST GROUP 16 – quantum counter for individual threads      */
/* ===================================================================== */

static volatile int g_q_thread_done = 0;
static volatile int g_q_val         = 0;

static void q_counter_thread(void)
{
    /* Yield twice via SIGVTALRM to accumulate quantums. */
    kill(getpid(), SIGVTALRM);
    kill(getpid(), SIGVTALRM);
    g_q_val = uthread_get_quantums(uthread_get_tid());
    g_q_thread_done = 1;
    uthread_terminate(uthread_get_tid());
}

static void test_thread_quantum_counter(void)
{
    printf("\n--- GROUP 16: per-thread quantum counter ---\n");
    g_q_thread_done = 0;
    g_q_val         = 0;

    int t = uthread_spawn(q_counter_thread);
    (void)t;

    while (!g_q_thread_done) {
        kill(getpid(), SIGVTALRM);
    }

    /* The thread ran: initial + 2 SIGVTALRM re-schedules = at least 3. */
    ASSERT(g_q_val >= 3, "spawned thread quantum count >= 3 after two yields");
}

/* ===================================================================== */
/*           TEST GROUP 17 – No READY duplicates                         */
/*           A thread must appear at most once in the READY queue.       */
/*           We test indirectly: resume a READY thread is a no-op.       */
/* ===================================================================== */

static volatile int g_dup_count = 0;

static void dedup_thread(void)
{
    /* Simply counts how many times it runs. */
    g_dup_count++;
    kill(getpid(), SIGVTALRM);  /* yield back */
    g_dup_count++;
    uthread_terminate(uthread_get_tid());
}

static void test_no_ready_duplicates(void)
{
    printf("\n--- GROUP 17: No READY duplicates ---\n");
    g_dup_count = 0;

    int t = uthread_spawn(dedup_thread);

    /* The thread is READY. Calling resume on a READY thread must be a no-op. */
    uthread_resume(t);
    uthread_resume(t);
    uthread_resume(t);

    /* Wait for it to finish. */
    while (g_dup_count < 2) {
        kill(getpid(), SIGVTALRM);
    }

    /* If it ran exactly 2 times, its quantum count should be 2. */
    /* (We cannot read it after termination; check count instead.) */
    ASSERT(g_dup_count == 2, "thread ran exactly twice (no duplicate READY entries)");
}

/* ===================================================================== */
/*           TEST GROUP 18 – Timer reset on each new quantum             */
/*           Verified by ensuring scheduler does not stall.              */
/* ===================================================================== */

static volatile int g_timer_ticks = 0;

static void timer_tick_thread(void)
{
    g_timer_ticks++;
    uthread_terminate(uthread_get_tid());
}

static void test_timer_reset(void)
{
    printf("\n--- GROUP 18: Timer resets on new quantum ---\n");
    g_timer_ticks = 0;

    /* Spawn 5 threads; each increments the counter once. */
    for (int i = 0; i < 5; i++) {
        uthread_spawn(timer_tick_thread);
    }

    /* Let them all run. */
    for (int i = 0; i < 10; i++) {
        kill(getpid(), SIGVTALRM);
    }

    ASSERT(g_timer_ticks == 5, "all 5 threads ran exactly once each");
}

/* ===================================================================== */
/*           TEST GROUP 19 – uthread_get_quantums for non-existent TID  */
/* ===================================================================== */

static void test_quantums_invalid(void)
{
    printf("\n--- GROUP 19: get_quantums invalid TID ---\n");

    ASSERT(uthread_get_quantums(MAX_THREAD_NUM) == -1,
           "get_quantums(MAX_THREAD_NUM) returns -1");

    ASSERT(uthread_get_quantums(MAX_THREAD_NUM + 1) == -1,
           "get_quantums(MAX_THREAD_NUM + 1) returns -1");

    ASSERT(uthread_get_quantums(-100) == -1,
           "get_quantums(-100) returns -1");
}

/* ===================================================================== */
/*           TEST GROUP 20 – Spawn after all others terminate            */
/*           Slot reuse scenario with multiple rounds.                   */
/* ===================================================================== */

static void short_thread(void)
{
    uthread_terminate(uthread_get_tid());
}

static void test_spawn_after_full_round(void)
{
    printf("\n--- GROUP 20: Spawn after all others terminate ---\n");

    /* Spawn and run to termination a series of threads, verifying TIDs. */
    for (int round = 0; round < 3; round++) {
        int t = uthread_spawn(short_thread);
        ASSERT(t == 1, "after all others terminate, next tid is 1");
        /* Let it run and terminate. */
        while (uthread_get_quantums(t) >= 0) {
            kill(getpid(), SIGVTALRM);
        }
    }
}

/* ===================================================================== */
/*           TEST GROUP 21 – Sleep then block interaction                */
/* ===================================================================== */

static volatile int g_sb_phase = 0;

static void sleep_block_thread(void)
{
    g_sb_phase = 1;             /* phase 1: started */
    uthread_sleep(2);
    g_sb_phase = 2;             /* phase 2: woke from sleep (not blocked) */
    uthread_terminate(uthread_get_tid());
}

static void test_sleep_block_interaction(void)
{
    printf("\n--- GROUP 21: Sleep then resume while sleep active ---\n");
    g_sb_phase = 0;

    int t = uthread_spawn(sleep_block_thread);

    /* Let it enter sleep. */
    while (g_sb_phase == 0) {
        kill(getpid(), SIGVTALRM);
    }

    /* Now block it while it is sleeping. */
    uthread_block(t);

    /* Let the sleep expire (2 quantums from when it started). */
    for (int i = 0; i < 4; i++) {
        kill(getpid(), SIGVTALRM);
    }

    /* Even though sleep expired, explicit block keeps it blocked. */
    ASSERT(g_sb_phase == 1,
           "thread blocked while sleeping stays blocked after sleep expires");

    /* Resume the explicit block. */
    uthread_resume(t);

    for (int i = 0; i < 3; i++) {
        kill(getpid(), SIGVTALRM);
    }

    ASSERT(g_sb_phase == 2, "thread runs after both sleep and explicit block cleared");
}

/* ===================================================================== */
/*           TEST GROUP 22 – Library error message format                */
/*           (Smoke-test: call invalid ops and verify return values.)    */
/* ===================================================================== */

static void test_error_return_values(void)
{
    printf("\n--- GROUP 22: Error return values ---\n");

    /* All of these must return -1 and print to stderr (not stdout). */
    ASSERT(uthread_block(0)              == -1, "block(0) == -1");
    ASSERT(uthread_block(-1)             == -1, "block(-1) == -1");
    ASSERT(uthread_resume(-1)            == -1, "resume(-1) == -1");
    ASSERT(uthread_resume(99)            == -1, "resume(99) non-existent == -1");
    ASSERT(uthread_terminate(-1)         == -1, "terminate(-1) == -1");
    ASSERT(uthread_terminate(99)         == -1, "terminate(99) non-existent == -1");
    ASSERT(uthread_get_quantums(-1)      == -1, "get_quantums(-1) == -1");
    ASSERT(uthread_get_quantums(99)      == -1, "get_quantums(99) non-existent == -1");
    ASSERT(uthread_sleep(0)              == -1, "sleep(0) from main == -1");
    ASSERT(uthread_sleep(-1)             == -1, "sleep(-1) from main == -1");
    ASSERT(uthread_spawn(NULL)           == -1, "spawn(NULL) == -1");
}

/* ===================================================================== */
/*           TEST GROUP 23 – Newly spawned thread quantum == 0           */
/* ===================================================================== */

static void test_new_thread_zero_quantums(void)
{
    printf("\n--- GROUP 23: New thread starts with 0 quantums ---\n");

    int t1 = uthread_spawn(dummy_entry);
    int t2 = uthread_spawn(dummy_entry);
    ASSERT(uthread_get_quantums(t1) == 0,
           "t1 has 0 quantums before first scheduling");
    ASSERT(uthread_get_quantums(t2) == 0,
           "t2 has 0 quantums before first scheduling");

    /* Let them run. */
    for (int i = 0; i < 4; i++) {
        kill(getpid(), SIGVTALRM);
    }
    /* After running at least once, quantum count > 0 (or already terminated). */
}

/* ===================================================================== */
/*           TEST GROUP 24 – READY queue ordering (FIFO for preempted)   */
/* ===================================================================== */

/*
 * Spawn two threads A (tid 1) and B (tid 2).
 * Ready queue initially: [1, 2].
 * Main yields (SIGVTALRM) -> thread 1 runs -> thread 1 yields -> thread 2 runs.
 * We record the order to verify FIFO.
 */

static int g_fifo_order[4];
static int g_fifo_idx = 0;

static void fifo_thread_a(void)
{
    g_fifo_order[g_fifo_idx++] = uthread_get_tid();
    uthread_terminate(uthread_get_tid());
}

static void fifo_thread_b(void)
{
    g_fifo_order[g_fifo_idx++] = uthread_get_tid();
    uthread_terminate(uthread_get_tid());
}

static void test_ready_queue_order(void)
{
    printf("\n--- GROUP 24: READY queue FIFO order ---\n");
    g_fifo_idx = 0;
    memset(g_fifo_order, -1, sizeof(g_fifo_order));

    int a = uthread_spawn(fifo_thread_a);   /* tid 1, added first */
    int b = uthread_spawn(fifo_thread_b);   /* tid 2, added second */
    (void)a; (void)b;

    /* Yield to thread 1. */
    kill(getpid(), SIGVTALRM);
    /* Yield to thread 2. */
    kill(getpid(), SIGVTALRM);
    /* Back to main. */
    kill(getpid(), SIGVTALRM);

    ASSERT(g_fifo_order[0] == 1 && g_fifo_order[1] == 2,
           "READY queue is FIFO: tid 1 runs before tid 2");
}

/* ===================================================================== */
/*           TEST GROUP 25 – Partial quantum: timer resets on switch     */
/* ===================================================================== */

static volatile int g_partial_done = 0;

static void partial_quantum_thread(void)
{
    /* Block ourselves immediately (does not consume the full quantum). */
    /* The scheduler should switch to next thread immediately. */
    g_partial_done = 1;
    uthread_block(uthread_get_tid());
    /* Should not reach here until resumed. */
    g_partial_done = 2;
    uthread_terminate(uthread_get_tid());
}

static void test_partial_quantum(void)
{
    printf("\n--- GROUP 25: Partial quantum – timer resets on voluntary block ---\n");
    g_partial_done = 0;

    int t = uthread_spawn(partial_quantum_thread);

    kill(getpid(), SIGVTALRM);  /* let thread start and block itself */
    while (g_partial_done == 0) {
        kill(getpid(), SIGVTALRM);
    }

    ASSERT(g_partial_done == 1,
           "thread blocked itself before partial quantum; scheduler continued");

    /* Resume and let it finish. */
    uthread_resume(t);
    for (int i = 0; i < 3; i++) {
        kill(getpid(), SIGVTALRM);
    }
    ASSERT(g_partial_done == 2, "thread resumed and ran to completion");
}

/* ===================================================================== */
/*           MAIN                                                         */
/* ===================================================================== */

int main(void)
{
    printf("========================================\n");
    printf("  uthreads Library Test Suite\n");
    printf("========================================\n");

    /*
     * uthread_init is called ONCE here for the whole suite.
     * We verify the very-first post-init state immediately below,
     * before any SIGVTALRM can fire, to catch the "total_q starts at 1"
     * and "main quantum starts at 1" requirements.
     * Groups that need a completely fresh library state use safe_fork().
     */
    if (uthread_init(QUANTUM_USECS) != 0) {
        fprintf(stderr, "FATAL: uthread_init failed in main.\n");
        return 1;
    }
    /* Snapshot immediately — before the first SIGVTALRM can fire. */
    {
        int tq_at_init = uthread_get_total_quantums();
        int mq_at_init = uthread_get_quantums(0);
        if (tq_at_init != 1) {
            printf("[FAIL] Pre-suite: total_quantums != 1 right after init (got %d)\n",
                   tq_at_init);
            failed++;
        } else {
            printf("[PASS] Pre-suite: total_quantums == 1 right after init\n");
            passed++;
        }
        test_number++;
        if (mq_at_init != 1) {
            printf("[FAIL] Pre-suite: main quantum count != 1 right after init (got %d)\n",
                   mq_at_init);
            failed++;
        } else {
            printf("[PASS] Pre-suite: main quantum count == 1 right after init\n");
            passed++;
        }
        test_number++;
    }

    test_init();
    test_spawn();
    test_tid_reuse();
    test_terminate_error_cases();
    test_block_resume_errors();
    test_sleep_errors();
    test_get_tid();
    test_total_quantums();
    test_get_quantums_errors();
    test_round_robin();
    test_sleep_timing();
    test_dual_blocking();
    test_resume_no_early_wake();
    test_block_other();
    test_terminate_other();
    test_thread_quantum_counter();
    test_no_ready_duplicates();
    test_timer_reset();
    test_quantums_invalid();
    test_spawn_after_full_round();
    test_sleep_block_interaction();
    test_error_return_values();
    test_new_thread_zero_quantums();
    test_ready_queue_order();
    test_partial_quantum();

    printf("\n========================================\n");
    printf("  Results: %d passed, %d failed\n", passed, failed);
    printf("========================================\n");

    return (failed == 0) ? 0 : 1;
}
