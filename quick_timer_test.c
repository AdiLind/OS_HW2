#include "uthreads.h"
#include <stdio.h>

void quick_thread(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Started!\n", tid);
    
    for (int i = 0; i < 3; i++) {
        printf("[Thread %d] Iteration %d\n", tid, i);
        // Light work - should trigger timer quickly
        for (volatile long j = 0; j < 10000000; j++);
    }
    
    printf("[Thread %d] Done!\n", tid);
    uthread_terminate(tid);
}

int main(void) {
    printf("=== Quick Timer Test with Fixed Timer ===\n");
    
    // Very short quantum - 5ms
    if (uthread_init(5000) != 0) {
        return 1;
    }
    
    printf("Initial: quantums=%d\n", uthread_get_total_quantums());
    
    // Spawn 2 threads
    int tid1 = uthread_spawn(quick_thread);
    int tid2 = uthread_spawn(quick_thread);
    printf("Spawned threads %d and %d\n", tid1, tid2);
    
    // Main work - short iterations
    for (int i = 0; i < 15; i++) {
        printf("[Main] Iteration %d, quantums=%d\n", 
               i, uthread_get_total_quantums());
        
        // Light work
        for (volatile long j = 0; j < 10000000; j++);
    }
    
    printf("Final quantums: %d\n", uthread_get_total_quantums());
    printf("Expected: Should see MANY more quantums!\n");
    
    uthread_terminate(0);
    return 0;
}