#include "uthreads.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

// Global counters for testing
static int thread1_iterations = 0;
static int thread2_iterations = 0;
static int thread3_iterations = 0;
static int threads_completed = 0;

// Thread 1: Simple counting thread
void thread1_func(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Started\n", tid);
    
    for (int i = 0; i < 10; i++) {
        thread1_iterations++;
        printf("[Thread %d] Iteration %d, Total quantums: %d, My quantums: %d\n", 
               tid, i, uthread_get_total_quantums(), uthread_get_quantums(tid));
        
        // Busy work to consume CPU time
        for (volatile long j = 0; j < 5000000; j++);
    }
    
    threads_completed++;
    printf("[Thread %d] Completed\n", tid);
    uthread_terminate(tid);
}

// Thread 2: Thread that tests sleep
void thread2_func(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Started - will sleep\n", tid);
    
    for (int i = 0; i < 5; i++) {
        thread2_iterations++;
        printf("[Thread %d] Iteration %d before sleep\n", tid, i);
        
        // Note: uthread_sleep not implemented yet, so commenting out
        // uthread_sleep(2);
        
        for (volatile long j = 0; j < 5000000; j++);
    }
    
    threads_completed++;
    printf("[Thread %d] Completed\n", tid);
    uthread_terminate(tid);
}

// Thread 3: Thread that terminates early
void thread3_func(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Started - will terminate early\n", tid);
    
    for (int i = 0; i < 3; i++) {
        thread3_iterations++;
        printf("[Thread %d] Quick iteration %d\n", tid, i);
        for (volatile long j = 0; j < 2000000; j++);
    }
    
    threads_completed++;
    printf("[Thread %d] Early termination\n", tid);
    uthread_terminate(tid);
}

// Test basic functionality
void test_basic_functionality() {
    printf("\n=== Test 1: Basic Functionality ===\n");
    
    // Test initialization with valid quantum
    assert(uthread_init(100000) == 0);  // 100ms quantum
    printf("✓ Initialization successful\n");
    
    // Test main thread ID
    assert(uthread_get_tid() == 0);
    printf("✓ Main thread has ID 0\n");
    
    // Test initial quantum count
    assert(uthread_get_total_quantums() == 1);
    printf("✓ Initial quantum count is 1\n");
    
    // Test spawning threads
    int tid1 = uthread_spawn(thread1_func);
    assert(tid1 > 0);
    printf("✓ Spawned thread with ID %d\n", tid1);
    
    int tid2 = uthread_spawn(thread2_func);
    assert(tid2 > 0);
    printf("✓ Spawned thread with ID %d\n", tid2);
    
    // Main thread work
    printf("\n[Main] Starting main thread work\n");
    for (int i = 0; i < 20; i++) {
        printf("[Main] Iteration %d, Total quantums: %d\n", 
               i, uthread_get_total_quantums());
        
        for (volatile long j = 0; j < 10000000; j++);
        
        // Check if threads are making progress
        if (i == 10) {
            printf("\n[Main] Progress check at iteration 10:\n");
            printf("  - Thread 1 iterations: %d\n", thread1_iterations);
            printf("  - Thread 2 iterations: %d\n", thread2_iterations);
            printf("  - Total quantums: %d\n", uthread_get_total_quantums());
            
            if (uthread_get_total_quantums() == 1) {
                printf("⚠️  WARNING: Timer doesn't seem to be working!\n");
            }
        }
    }
    
    // Wait for threads to complete
    while (threads_completed < 2) {
        for (volatile long j = 0; j < 10000000; j++);
    }
    
    printf("\n[Main] All threads completed\n");
    printf("Final statistics:\n");
    printf("  - Total quantums: %d\n", uthread_get_total_quantums());
    printf("  - Main thread quantums: %d\n", uthread_get_quantums(0));
    
    // Terminate main thread
    uthread_terminate(0);
}

// Test error cases
void test_error_cases() {
    printf("\n=== Test 2: Error Cases ===\n");
    
    // Test initialization with invalid quantum
    assert(uthread_init(-1) == -1);
    printf("✓ Rejected negative quantum\n");
    
    assert(uthread_init(0) == -1);
    printf("✓ Rejected zero quantum\n");
    
    // Initialize properly for remaining tests
    assert(uthread_init(50000) == 0);
    
    // Test NULL entry point
    assert(uthread_spawn(NULL) == -1);
    printf("✓ Rejected NULL entry point\n");
    
    // Test invalid thread ID
    assert(uthread_terminate(999) == -1);
    printf("✓ Rejected invalid thread ID\n");
    
    assert(uthread_get_quantums(-1) == -1);
    printf("✓ Rejected negative thread ID for get_quantums\n");
    
    // Clean up
    uthread_terminate(0);
}

// Test many threads
void dummy_thread(void) {
    int tid = uthread_get_tid();
    for (int i = 0; i < 2; i++) {
        for (volatile long j = 0; j < 1000000; j++);
    }
    uthread_terminate(tid);
}

void test_many_threads() {
    printf("\n=== Test 3: Many Threads ===\n");
    
    assert(uthread_init(10000) == 0);  // 10ms quantum for faster switching
    
    int spawned = 0;
    int tids[MAX_THREAD_NUM];
    
    // Try to spawn MAX_THREAD_NUM threads
    for (int i = 0; i < MAX_THREAD_NUM - 1; i++) {  // -1 because main thread exists
        int tid = uthread_spawn(dummy_thread);
        if (tid != -1) {
            tids[spawned++] = tid;
        } else {
            break;
        }
    }
    
    printf("✓ Successfully spawned %d threads\n", spawned);
    
    // Try to spawn one more - should fail
    assert(uthread_spawn(dummy_thread) == -1);
    printf("✓ Correctly rejected thread when at maximum\n");
    
    // Let threads run a bit
    for (volatile long i = 0; i < 50000000; i++);
    
    // Clean up
    uthread_terminate(0);
}

// Main test runner
int main(void) {
    printf("=== Comprehensive uthread Library Tests ===\n");
    printf("Note: Each test terminates the main thread at the end\n");
    
    // Run only the basic test first to debug timer issue
    test_basic_functionality();
    
    // Uncomment these after fixing timer:
    // test_error_cases();
    // test_many_threads();
    
    return 0;
}