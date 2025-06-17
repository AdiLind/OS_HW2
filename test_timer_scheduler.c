/*gcc -std=c17 -Wall -Wextra -O0 -g \
  -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L \
  test_timer_scheduler.c uthreads.c -o test_timer_scheduler */

#include "uthreads.h"
#include <stdio.h>

// Simple thread function that counts
void counting_thread(void) {
    int tid = uthread_get_tid();
    for (int i = 0; i < 5; i++) {
        printf("Thread %d: iteration %d, total quantums: %d\n", 
               tid, i, uthread_get_total_quantums());
        
        // Busy loop to consume CPU time
        for (volatile int j = 0; j < 10000000; j++);
    }
    
    printf("Thread %d terminating\n", tid);
    uthread_terminate(tid);
}

int main(void) {
    printf("=== Testing Timer and Scheduler ===\n");
    
    // Initialize with 50ms quantum
    if (uthread_init(50000) == -1) {
        printf("Failed to initialize\n");
        return 1;
    }
    
    printf("Main thread (tid=%d) started\n", uthread_get_tid());
    
    // Spawn two threads
    int tid1 = uthread_spawn(counting_thread);
    int tid2 = uthread_spawn(counting_thread);
    
    printf("Spawned threads: %d and %d\n", tid1, tid2);
    
    // Main thread also does some work
    for (int i = 0; i < 15; i++) {
        printf("Main thread: iteration %d, total quantums: %d\n", 
               i, uthread_get_total_quantums());
        
        // Busy loop
        for (volatile int j = 0; j < 10000000; j++);
    }
    
    printf("Main thread terminating\n");
    uthread_terminate(0);
    
    return 0;
}