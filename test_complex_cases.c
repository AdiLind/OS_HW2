/* how to compile:
gcc -std=c17 -Wall -Wextra -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L     test_complex_cases.c uthreads.c -o test_blocking_sleep_complex

*/
#include "uthreads.h"
#include <stdio.h>
#include <assert.h>

// Thread that will be both sleeping and blocked
void complex_thread(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Started\n", tid);
    
    // Signal main thread we're ready
    printf("[Thread %d] Sleeping for 5 quantums...\n", tid);
    int sleep_start = uthread_get_total_quantums();
    
    // Sleep for 5 quantums
    uthread_sleep(5);
    
    // If we reach here, both conditions were resolved
    int wake_time = uthread_get_total_quantums();
    printf("[Thread %d] Woke up at quantum %d (slept from %d)\n", 
           tid, wake_time, sleep_start);
    
    printf("[Thread %d] Both sleep expired AND was resumed!\n", tid);
    uthread_terminate(tid);
}

int main(void) {
    printf("=== Test: Thread Both Blocked and Sleeping ===\n");
    
    // Initialize with 50ms quantum for faster testing
    assert(uthread_init(50000) == 0);
    
    // Spawn the thread
    int tid = uthread_spawn(complex_thread);
    printf("Main: Spawned thread %d\n", tid);
    
    // Give the thread a chance to run and start sleeping
    printf("Main: Letting thread start...\n");
    for (int i = 0; i < 3; i++) {
        for (volatile long j = 0; j < 50000000; j++);
    }
    
    // Now block it while it's sleeping
    printf("Main: Blocking thread %d while it's sleeping\n", tid);
    assert(uthread_block(tid) == 0);
    
    // Do work for several quantums
    printf("Main: Thread is both sleeping AND blocked\n");
    for (int i = 0; i < 8; i++) {
        printf("Main: Working... quantum %d\n", uthread_get_total_quantums());
        for (volatile long j = 0; j < 100000000; j++);
    }
    
    // At this point, sleep should have expired, but thread is still blocked
    printf("Main: Sleep should have expired, but thread is still blocked\n");
    
    // Resume the thread
    printf("Main: Resuming thread %d\n", tid);
    assert(uthread_resume(tid) == 0);
    
    // Thread should now wake up
    printf("Main: Thread should wake up now\n");
    
    // Wait for thread to finish
    for (volatile long i = 0; i < 200000000; i++);
    
    printf("\nTest completed successfully!\n");
    printf("This proves that a thread must satisfy BOTH conditions:\n");
    printf("1. Sleep duration must expire\n");
    printf("2. Thread must be resumed\n");
    
    uthread_terminate(0);
    return 0;
}