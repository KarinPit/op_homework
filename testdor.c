#include <stdio.h>
#include <stdlib.h>
#include "uthreads.h"

void my_thread_func(void) {
    printf("[Thread 1] Woke up! I'm running perfectly.\n"); 
    fflush(stdout); // דוחף את ההדפסה למסך בכוח
    
    uthread_sleep(1); 
    
    printf("[Thread 1] Woke up again! Terminating myself.\n"); 
    fflush(stdout);
    
    uthread_terminate(uthread_get_tid());
}

void my_thread_func2(void) {
    printf("[Thread 2] Hello there! I'm alive.\n"); 
    fflush(stdout);
    
    uthread_sleep(1);
    
    printf("[Thread 2] Done! Terminating program.\n"); 
    fflush(stdout);
    
    // נסיים את התוכנית כולה מכאן כדי לראות שהכל עבד
    exit(0); 
}

int main() {
    printf("--- Starting uthreads test ---\n"); 
    fflush(stdout);
    
    if (uthread_init(100000) == -1) {
        printf("Init failed!\n");
        return 1;
    }
    
    uthread_spawn(my_thread_func);
    uthread_spawn(my_thread_func2);
    
    printf("[Main] Threads spawned. Waiting for timer...\n"); 
    fflush(stdout);
    
    // לולאה אינסופית - המעבד יעבוד עד שהטיימר יזרוק אותנו לחוטים
    while (1) { }
    
    return 0;
}