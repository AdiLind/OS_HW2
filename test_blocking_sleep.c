/* how to compile 
gcc -O0 -g -Wall -Wextra test_blocking_sleep.c uthreads.o -o test_blocking_sleep
gcc -std=c17 -Wall -Wextra -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L     test_blocking_sleep.c uthreads.c -o test_blocking_sleep

*/

#include "uthreads.h"
#include <stdio.h>
#include <assert.h>

// Test results
static int test1_completed = 0;
static int test2_completed = 0;
static int test3_completed = 0;
static int test4_completed = 0;

// Test 1: Simple block and resume
void thread1_block_resume(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Started - will be blocked by main\n", tid);
    
    // Do some work
    for (int i = 0; i < 3; i++) {
        printf("[Thread %d] Working... iteration %d\n", tid, i);
        for (volatile long j = 0; j < 50000000; j++);
    }
    
    printf("[Thread %d] Finished work, should have been blocked and resumed\n", tid);
    test1_completed = 1;
    uthread_terminate(tid);
}

// Test 2: Thread that blocks itself (should fail)
void thread2_self_block(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Trying to block myself...\n", tid);
    
    // This should switch to another thread
    int result = uthread_block(tid);
    
    // We should never reach here if blocking worked
    printf("[Thread %d] ERROR: Returned from self-block with result %d\n", tid, result);
    test2_completed = 1;
    uthread_terminate(tid);
}

// Test 3: Sleep test
void thread3_sleep(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Started - will sleep for 3 quantums\n", tid);
    
    int before_sleep = uthread_get_total_quantums();
    printf("[Thread %d] Going to sleep at quantum %d\n", tid, before_sleep);
    
    uthread_sleep(3);
    
    int after_sleep = uthread_get_total_quantums();
    printf("[Thread %d] Woke up at quantum %d (slept for %d quantums)\n", 
           tid, after_sleep, after_sleep - before_sleep);
    
    test3_completed = 1;
    uthread_terminate(tid);
}

// Test 4: Block + Sleep combination
void thread4_block_and_sleep(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Started - will sleep then be blocked\n", tid);
    
    // First sleep
    printf("[Thread %d] Sleeping for 2 quantums...\n", tid);
    uthread_sleep(2);
    printf("[Thread %d] Woke up from sleep\n", tid);
    
    // Signal that we're ready to be blocked
    test4_completed = 1;
    
    // Do work until blocked
    for (int i = 0; i < 10; i++) {
        printf("[Thread %d] Working after sleep... iteration %d\n", tid, i);
        for (volatile long j = 0; j < 50000000; j++);
    }
    
    printf("[Thread %d] Finished\n", tid);
    uthread_terminate(tid);
}

// Test error cases
void test_error_cases(void) {
    printf("\n=== Testing Error Cases ===\n");
    
    // Initialize library
    assert(uthread_init(50000) == 0);
    
    // Test 1: Block main thread (should fail)
    printf("Test: Blocking main thread... ");
    assert(uthread_block(0) == -1);
    printf("✓ Correctly rejected\n");
    
    // Test 2: Resume invalid thread
    printf("Test: Resume invalid thread... ");
    assert(uthread_resume(999) == -1);
    printf("✓ Correctly rejected\n");
    
    // Test 3: Block invalid thread
    printf("Test: Block invalid thread... ");
    assert(uthread_block(-1) == -1);
    printf("✓ Correctly rejected\n");
    
    // Test 4: Main thread sleep (should fail)
    printf("Test: Main thread sleep... ");
    assert(uthread_sleep(5) == -1);
    printf("✓ Correctly rejected\n");
    
    // Test 5: Invalid sleep duration
    int tid = uthread_spawn(thread1_block_resume);
    uthread_block(tid); // Block it so it doesn't interfere
    
    printf("\nError tests completed successfully!\n");
}

// Main test function
void test_blocking_and_sleep(void) {
    printf("\n=== Testing Blocking and Sleep Operations ===\n");
    
    // Initialize with 100ms quantum
    assert(uthread_init(100000) == 0);
    printf("Library initialized with 100ms quantum\n\n");
    
    // Test 1: Block and Resume
    printf("--- Test 1: Block and Resume ---\n");
    int tid1 = uthread_spawn(thread1_block_resume);
    printf("Main: Spawned thread %d\n", tid1);
    
    // Let it run a bit
    for (volatile long i = 0; i < 100000000; i++);
    
    // Block the thread
    printf("Main: Blocking thread %d\n", tid1);
    assert(uthread_block(tid1) == 0);
    
    // Do some work while thread is blocked
    printf("Main: Thread blocked, doing work...\n");
    for (int i = 0; i < 3; i++) {
        printf("Main: Working... quantum %d\n", uthread_get_total_quantums());
        for (volatile long j = 0; j < 100000000; j++);
    }
    
    // Resume the thread
    printf("Main: Resuming thread %d\n", tid1);
    assert(uthread_resume(tid1) == 0);
    
    // Wait for thread to complete
    while (!test1_completed) {
        for (volatile long i = 0; i < 100000000; i++);
    }
    
    // Test 2: Self-blocking thread
    printf("\n--- Test 2: Self-Blocking Thread ---\n");
    int tid2 = uthread_spawn(thread2_self_block);
    
    // Wait a bit
    while (!test2_completed) {
        for (volatile long i = 0; i < 100000000; i++);
    }
    
    // Test 3: Sleep
    printf("\n--- Test 3: Sleep Test ---\n");
    int tid3 = uthread_spawn(thread3_sleep);
    
    // Do work while thread sleeps
    while (!test3_completed) {
        printf("Main: Working while thread sleeps... quantum %d\n", 
               uthread_get_total_quantums());
        for (volatile long i = 0; i < 200000000; i++);
    }
    
    // Test 4: Block + Sleep
    printf("\n--- Test 4: Block + Sleep Combination ---\n");
    int tid4 = uthread_spawn(thread4_block_and_sleep);
    
    // Wait for thread to signal it's ready to be blocked
    while (!test4_completed) {
        for (volatile long i = 0; i < 100000000; i++);
    }
    
    // Now block it
    printf("Main: Blocking thread %d after it slept\n", tid4);
    assert(uthread_block(tid4) == 0);
    
    // Wait a bit
    for (int i = 0; i < 2; i++) {
        printf("Main: Thread is blocked... quantum %d\n", uthread_get_total_quantums());
        for (volatile long j = 0; j < 200000000; j++);
    }
    
    // Resume it
    printf("Main: Resuming thread %d\n", tid4);
    assert(uthread_resume(tid4) == 0);
    
    // Let it finish
    for (volatile long i = 0; i < 500000000; i++);
    
    printf("\nAll blocking and sleep tests completed!\n");
    printf("Final statistics:\n");
    printf("- Total quantums: %d\n", uthread_get_total_quantums());
    printf("- Main thread quantums: %d\n", uthread_get_quantums(0));
    
    uthread_terminate(0);
}

int main(void) {
    printf("=== Comprehensive Block/Resume/Sleep Tests ===\n");
    
    // First test error cases
    test_error_cases();
    
    // Then test normal operations
    test_blocking_and_sleep();
    
    return 0;
}