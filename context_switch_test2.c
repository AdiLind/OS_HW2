#include "uthreads.h"
#include <stdio.h>

static int thread_switch_count = 0;

void counting_thread(void) {
    int tid = uthread_get_tid();
    printf("Thread %d started\n", tid);
    
    for (int i = 0; i < 5; i++) {
        thread_switch_count++;
        printf("Thread %d: count=%d, switches=%d\n", 
               tid, i, thread_switch_count);
        
        // Heavy work to trigger timer
        volatile long sum = 0;
        for (long j = 0; j < 100000000L; j++) {
            sum += j;
        }
    }
    
    printf("Thread %d finished\n", tid);
    uthread_terminate(tid);
}

int main(void) {
    printf("=== Context Switch Validation ===\n");
    
    // Short quantum to force many switches
    if (uthread_init(20000) != 0) {  // 20ms
        return 1;
    }
    
    // Create 3 threads
    int tid1 = uthread_spawn(counting_thread);
    int tid2 = uthread_spawn(counting_thread);  
    int tid3 = uthread_spawn(counting_thread);
    
    printf("Created threads: %d, %d, %d\n", tid1, tid2, tid3);
    
    // Main thread work
    for (int i = 0; i < 10; i++) {
        printf("Main: iteration %d, total_quantums=%d\n", 
               i, uthread_get_total_quantums());
        
        // Heavy work
        volatile long sum = 0;
        for (long j = 0; j < 100000000L; j++) {
            sum += j;
        }
    }
    
    printf("Final switch count: %d\n", thread_switch_count);
    printf("Final quantums: %d\n", uthread_get_total_quantums());
    
    // Should see multiple threads running and many context switches
    if (thread_switch_count < 10) {
        printf("⚠️  Warning: Low switch count - context switching may not work properly\n");
    }
    
    if (uthread_get_total_quantums() < 5) {
        printf("⚠️  Warning: Low quantum count - timer may not work properly\n");
    }
    
    uthread_terminate(0);
    return 0;
}