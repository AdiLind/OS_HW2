#include "uthreads.h"
#include <stdio.h>

void worker_thread(void) {
    int tid = uthread_get_tid();
    
    for (int i = 0; i < 5; i++) {
        printf("[Thread %d] Working... iteration %d\n", tid, i);
        
        // Heavy CPU work to trigger timer
        volatile long sum = 0;
        for (long j = 0; j < 100000000L; j++) {
            sum += j;
        }
    }
    
    printf("[Thread %d] Finished!\n", tid);
    uthread_terminate(tid);
}

int main(void) {
    printf("=== Timer and Context Switch Test ===\n");
    
    // Use 50ms quantum
    if (uthread_init(50000) != 0) {
        printf("Init failed!\n");
        return 1;
    }
    
    printf("Main thread initialized (tid=%d)\n", uthread_get_tid());
    
    // Create 2 worker threads
    int tid1 = uthread_spawn(worker_thread);
    int tid2 = uthread_spawn(worker_thread);
    
    if (tid1 == -1 || tid2 == -1) {
        printf("Failed to spawn threads!\n");
        return 1;
    }
    
    printf("Spawned threads %d and %d\n", tid1, tid2);
    printf("Starting main thread work...\n");
    
    // Main thread work
    for (int i = 0; i < 10; i++) {
        printf("[Main] Working... iteration %d (quantums=%d)\n", 
               i, uthread_get_total_quantums());
        
        // Heavy CPU work
        volatile long sum = 0;
        for (long j = 0; j < 100000000L; j++) {
            sum += j;
        }
    }
    
    printf("Main thread done!\n");
    uthread_terminate(0);
    
    return 0;
}