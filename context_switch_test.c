/* how to compile:
gcc -std=c17 -Wall -Wextra -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L     context_switch_test.c uthreads.
c -o test_context_switch

*/

#include "uthreads.h"
#include <stdio.h>

// Very simple thread that just prints
void simple_thread(void) {
    int tid = uthread_get_tid();
    printf(">>> Thread %d is running! <<<\n", tid);
    
    // Do some work
    for (int i = 0; i < 3; i++) {
        printf("Thread %d: iteration %d\n", tid, i);
        for (volatile long j = 0; j < 50000000; j++);
    }
    
    printf("Thread %d terminating\n", tid);
    uthread_terminate(tid);
}

int main(void) {
    printf("=== Context Switch Debug Test ===\n");
    
    // Initialize with very short quantum (10ms)
    if (uthread_init(10000) != 0) {
        printf("Failed to initialize\n");
        return 1;
    }
    
    printf("Main thread started (tid=%d)\n", uthread_get_tid());
    printf("Initial quantums: %d\n", uthread_get_total_quantums());
    
    // Spawn just one thread
    int tid = uthread_spawn(simple_thread);
    printf("Spawned thread %d\n", tid);
    
    // Main thread work - with status updates
    for (int i = 0; i < 10; i++) {
        printf("Main: iteration %d, quantums=%d\n", 
               i, uthread_get_total_quantums());
        
        // Check who's running
        int current = uthread_get_tid();
        if (current != 0) {
            printf("!!! Context switch happened! Now running thread %d !!!\n", current);
        }
        
        // Busy work
        for (volatile long j = 0; j < 50000000; j++);
    }
    
    printf("Main thread done\n");
    uthread_terminate(0);
    
    return 0;
}