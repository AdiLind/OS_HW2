/* 
 * Advanced Sleep and Blocking Tests
 * 
 * Compile:
 * gcc -std=c17 -Wall -Wextra -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L \
 *     advanced_sleep_block_tests.c uthreads.c -o test_advanced
 * 
 * Run: ./test_advanced
 */

#include "uthreads.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

/* Test counters and flags */
static int test_results[20] = {0}; // Array to store test results
static int tests_completed = 0;
static int total_tests = 0;

/* Helper macros */
#define TEST_START(name, test_id) \
    printf("\n🧪 Test %d: %s\n", test_id, name); \
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"); \
    total_tests++;

#define TEST_SUCCESS(test_id) \
    test_results[test_id] = 1; \
    tests_completed++; \
    printf("✅ Test %d PASSED\n", test_id);

#define TEST_FAIL(test_id, msg) \
    test_results[test_id] = 0; \
    printf("❌ Test %d FAILED: %s\n", test_id, msg);

#define BUSY_WORK(iterations) \
    for (volatile long i = 0; i < iterations; i++);

/* Global test state */
static volatile int thread_states[10] = {0}; // Track thread progress
static volatile int wakeup_times[10] = {0};  // Track when threads wake up
static volatile int block_resume_count = 0;

/* ========================================================================= */
/*                           BASIC SLEEP TESTS                              */
/* ========================================================================= */

void simple_sleep_thread(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Starting, will sleep for 2 quantums\n", tid);
    
    int sleep_start = uthread_get_total_quantums();
    thread_states[tid] = sleep_start;
    
    uthread_sleep(2);
    
    int wake_time = uthread_get_total_quantums();
    wakeup_times[tid] = wake_time;
    
    printf("[Thread %d] Woke up! Sleep start: %d, Wake time: %d, Slept for: %d\n", 
           tid, sleep_start, wake_time, wake_time - sleep_start);
    
    uthread_terminate(tid);
}

void test_basic_sleep(void) {
    TEST_START("Basic Sleep Functionality", 1);
    
    assert(uthread_init(50000) == 0); // 50ms quantum
    
    int tid = uthread_spawn(simple_sleep_thread);
    assert(tid > 0);
    
    // Let thread start and sleep
    BUSY_WORK(50000000);
    
    // Wait for thread to wake up and complete
    int iterations = 0;
    while (wakeup_times[tid] == 0 && iterations < 20) {
        printf("[Main] Waiting for thread to wake up... quantum: %d\n", 
               uthread_get_total_quantums());
        BUSY_WORK(100000000);
        iterations++;
    }
    
    if (wakeup_times[tid] > 0) {
        int sleep_duration = wakeup_times[tid] - thread_states[tid];
        if (sleep_duration >= 2) {
            TEST_SUCCESS(1);
        } else {
            TEST_FAIL(1, "Thread didn't sleep long enough");
        }
    } else {
        TEST_FAIL(1, "Thread never woke up");
    }
}

/* ========================================================================= */
/*                           MULTIPLE SLEEP TESTS                           */
/* ========================================================================= */

void multi_sleep_thread(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Multiple sleep test\n", tid);
    
    // First sleep
    printf("[Thread %d] First sleep (1 quantum)\n", tid);
    uthread_sleep(1);
    printf("[Thread %d] Woke from first sleep\n", tid);
    
    // Second sleep
    printf("[Thread %d] Second sleep (3 quantums)\n", tid);
    uthread_sleep(3);
    printf("[Thread %d] Woke from second sleep\n", tid);
    
    thread_states[tid] = 1; // Mark as completed
    uthread_terminate(tid);
}

void test_multiple_sleeps(void) {
    TEST_START("Multiple Sleep Operations", 2);
    
    assert(uthread_init(30000) == 0); // 30ms quantum
    
    int tid = uthread_spawn(multi_sleep_thread);
    assert(tid > 0);
    
    // Wait for completion
    int iterations = 0;
    while (thread_states[tid] == 0 && iterations < 30) {
        printf("[Main] Quantum: %d\n", uthread_get_total_quantums());
        BUSY_WORK(80000000);
        iterations++;
    }
    
    if (thread_states[tid] == 1) {
        TEST_SUCCESS(2);
    } else {
        TEST_FAIL(2, "Thread didn't complete multiple sleeps");
    }
}

/* ========================================================================= */
/*                           BASIC BLOCKING TESTS                           */
/* ========================================================================= */

void block_target_thread(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Started - waiting to be blocked\n", tid);
    
    thread_states[tid] = 1; // Signal that we're ready
    
    // Do work until blocked
    for (int i = 0; i < 20; i++) {
        printf("[Thread %d] Working... iteration %d\n", tid, i);
        BUSY_WORK(30000000);
        
        if (i == 10) {
            thread_states[tid] = 2; // Signal mid-progress
        }
    }
    
    // If we reach here, we weren't blocked properly
    thread_states[tid] = 99; // Error state
    printf("[Thread %d] ERROR: Should have been blocked!\n", tid);
    uthread_terminate(tid);
}

void blocking_controller_thread(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Controller - will block thread 2\n", tid);
    
    // Wait for target thread to be ready
    while (thread_states[2] < 1) {
        BUSY_WORK(10000000);
    }
    
    printf("[Thread %d] Blocking thread 2\n", tid);
    int result = uthread_block(2);
    printf("[Thread %d] Block result: %d\n", tid, result);
    
    // Wait a bit
    BUSY_WORK(100000000);
    
    printf("[Thread %d] Resuming thread 2\n", tid);
    uthread_resume(2);
    
    block_resume_count = 1; // Signal completion
    uthread_terminate(tid);
}

void test_basic_blocking(void) {
    TEST_START("Basic Block and Resume", 3);
    
    assert(uthread_init(40000) == 0); // 40ms quantum
    
    int tid1 = uthread_spawn(block_target_thread);
    int tid2 = uthread_spawn(blocking_controller_thread);
    
    assert(tid1 == 1);
    assert(tid2 == 2);
    
    // Wait for test completion
    int iterations = 0;
    while (block_resume_count == 0 && iterations < 25) {
        printf("[Main] Monitoring... quantum: %d\n", uthread_get_total_quantums());
        BUSY_WORK(100000000);
        iterations++;
    }
    
    if (block_resume_count == 1 && thread_states[1] != 99) {
        TEST_SUCCESS(3);
    } else {
        TEST_FAIL(3, "Blocking/Resume didn't work correctly");
    }
}

/* ========================================================================= */
/*                           SLEEP + BLOCK COMBINATION                      */
/* ========================================================================= */

void sleep_and_block_thread(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Started - will sleep then be blocked\n", tid);
    
    thread_states[tid] = 1; // Ready state
    
    printf("[Thread %d] Going to sleep for 4 quantums\n", tid);
    int sleep_start = uthread_get_total_quantums();
    
    uthread_sleep(4);
    
    int wake_time = uthread_get_total_quantums();
    printf("[Thread %d] Woke up after %d quantums\n", tid, wake_time - sleep_start);
    
    thread_states[tid] = 2; // Woke up successfully
    
    // Continue working
    for (int i = 0; i < 5; i++) {
        printf("[Thread %d] Post-sleep work %d\n", tid, i);
        BUSY_WORK(50000000);
    }
    
    thread_states[tid] = 3; // Completed
    uthread_terminate(tid);
}

void combo_controller_thread(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Combo controller\n", tid);
    
    // Wait for target to start sleeping
    while (thread_states[1] < 1) {
        BUSY_WORK(10000000);
    }
    
    // Block the sleeping thread
    printf("[Thread %d] Blocking sleeping thread 1\n", tid);
    uthread_block(1);
    
    // Wait several quantums
    for (int i = 0; i < 6; i++) {
        printf("[Thread %d] Waiting... quantum %d\n", tid, uthread_get_total_quantums());
        BUSY_WORK(100000000);
    }
    
    // Resume the thread
    printf("[Thread %d] Resuming thread 1\n", tid);
    uthread_resume(1);
    
    block_resume_count = 2; // Different marker
    uthread_terminate(tid);
}

void test_sleep_block_combination(void) {
    TEST_START("Sleep + Block Combination", 4);
    
    // Reset state
    for (int i = 0; i < 10; i++) {
        thread_states[i] = 0;
    }
    block_resume_count = 0;
    
    assert(uthread_init(60000) == 0); // 60ms quantum
    
    int tid1 = uthread_spawn(sleep_and_block_thread);
    int tid2 = uthread_spawn(combo_controller_thread);
    
    // Monitor progress
    int iterations = 0;
    while (block_resume_count != 2 && iterations < 30) {
        printf("[Main] Thread 1 state: %d, quantum: %d\n", 
               thread_states[1], uthread_get_total_quantums());
        BUSY_WORK(120000000);
        iterations++;
    }
    
    // Wait for thread 1 to complete
    iterations = 0;
    while (thread_states[1] < 3 && iterations < 15) {
        BUSY_WORK(100000000);
        iterations++;
    }
    
    if (thread_states[1] == 3 && block_resume_count == 2) {
        TEST_SUCCESS(4);
    } else {
        TEST_FAIL(4, "Sleep+Block combination failed");
    }
}

/* ========================================================================= */
/*                           ERROR HANDLING TESTS                           */
/* ========================================================================= */

void test_sleep_error_cases(void) {
    TEST_START("Sleep Error Cases", 5);
    
    assert(uthread_init(50000) == 0);
    
    // Test 1: Main thread cannot sleep
    printf("Testing main thread sleep...\n");
    assert(uthread_sleep(5) == -1);
    
    // Test 2: Invalid sleep duration
    printf("Testing negative sleep duration...\n");
    assert(uthread_sleep(-1) == -1);
    
    printf("Testing zero sleep duration...\n");
    assert(uthread_sleep(0) == -1);
    
    TEST_SUCCESS(5);
}

void test_block_error_cases(void) {
    TEST_START("Block Error Cases", 6);
    
    assert(uthread_init(50000) == 0);
    
    // Test 1: Cannot block main thread
    printf("Testing block main thread...\n");
    assert(uthread_block(0) == -1);
    
    // Test 2: Invalid TID
    printf("Testing block invalid TID...\n");
    assert(uthread_block(-1) == -1);
    assert(uthread_block(999) == -1);
    
    // Test 3: Block non-existent thread
    printf("Testing block non-existent thread...\n");
    assert(uthread_block(50) == -1);
    
    // Test 4: Resume invalid TID
    printf("Testing resume invalid TID...\n");
    assert(uthread_resume(-1) == -1);
    assert(uthread_resume(999) == -1);
    
    TEST_SUCCESS(6);
}

/* ========================================================================= */
/*                           STRESS TESTS                                   */
/* ========================================================================= */

void rapid_sleep_thread(void) {
    int tid = uthread_get_tid();
    
    for (int i = 0; i < 5; i++) {
        printf("[Thread %d] Rapid sleep %d\n", tid, i);
        uthread_sleep(1);
    }
    
    thread_states[tid] = 1;
    uthread_terminate(tid);
}

void test_rapid_sleep_operations(void) {
    TEST_START("Rapid Sleep Operations", 7);
    
    assert(uthread_init(20000) == 0); // Short quantum
    
    // Create multiple threads doing rapid sleeps
    int tids[3];
    for (int i = 0; i < 3; i++) {
        tids[i] = uthread_spawn(rapid_sleep_thread);
        assert(tids[i] > 0);
    }
    
    // Wait for all to complete
    int completed = 0;
    int iterations = 0;
    
    while (completed < 3 && iterations < 50) {
        completed = 0;
        for (int i = 0; i < 3; i++) {
            if (thread_states[tids[i]] == 1) {
                completed++;
            }
        }
        
        printf("[Main] Completed: %d/3, quantum: %d\n", 
               completed, uthread_get_total_quantums());
        BUSY_WORK(80000000);
        iterations++;
    }
    
    if (completed == 3) {
        TEST_SUCCESS(7);
    } else {
        TEST_FAIL(7, "Not all rapid sleep threads completed");
    }
}

/* ========================================================================= */
/*                           EDGE CASE TESTS                                */
/* ========================================================================= */

void double_block_thread(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Ready to be double-blocked\n", tid);
    
    thread_states[tid] = 1; // Ready
    
    // Wait to be blocked
    for (int i = 0; i < 50; i++) {
        BUSY_WORK(20000000);
        if (i == 25) {
            thread_states[tid] = 2; // Mid-point
        }
    }
    
    thread_states[tid] = 3; // Should not reach here if blocked properly
    uthread_terminate(tid);
}

void test_double_block_resume(void) {
    TEST_START("Double Block/Resume Operations", 8);
    
    // Reset state
    for (int i = 0; i < 10; i++) {
        thread_states[i] = 0;
    }
    
    assert(uthread_init(30000) == 0);
    
    int tid = uthread_spawn(double_block_thread);
    
    // Wait for thread to be ready
    while (thread_states[tid] < 1) {
        BUSY_WORK(10000000);
    }
    
    // Block twice
    printf("[Main] First block\n");
    assert(uthread_block(tid) == 0);
    
    printf("[Main] Second block (should be no-op)\n");
    assert(uthread_block(tid) == 0);
    
    // Wait a bit
    BUSY_WORK(100000000);
    
    // Resume twice
    printf("[Main] First resume\n");
    assert(uthread_resume(tid) == 0);
    
    printf("[Main] Second resume (should be no-op)\n");
    assert(uthread_resume(tid) == 0);
    
    // Wait for completion
    int iterations = 0;
    while (thread_states[tid] < 3 && iterations < 20) {
        BUSY_WORK(80000000);
        iterations++;
    }
    
    if (thread_states[tid] == 3) {
        TEST_SUCCESS(8);
    } else {
        TEST_FAIL(8, "Double block/resume test failed");
    }
}

/* ========================================================================= */
/*                           MAIN TEST RUNNER                               */
/* ========================================================================= */

void print_summary(void) {
    printf("\n");
    printf("🎯 TEST SUMMARY\n");
    printf("═══════════════════════════════════════════════════════════\n");
    
    int passed = 0;
    for (int i = 1; i <= total_tests; i++) {
        if (test_results[i] == 1) {
            printf("✅ Test %d: PASSED\n", i);
            passed++;
        } else {
            printf("❌ Test %d: FAILED\n", i);
        }
    }
    
    printf("═══════════════════════════════════════════════════════════\n");
    printf("📊 Results: %d/%d tests passed (%.1f%%)\n", 
           passed, total_tests, (100.0 * passed) / total_tests);
    
    if (passed == total_tests) {
        printf("🎉 ALL TESTS PASSED! Your implementation is working correctly!\n");
    } else {
        printf("🚨 Some tests failed. Please review the implementation.\n");
    }
}

int main(void) {
    printf("🔬 ADVANCED SLEEP AND BLOCKING TESTS\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("This test suite comprehensively tests:\n");
    printf("• Basic sleep functionality\n");
    printf("• Multiple sleep operations\n");
    printf("• Basic blocking and resuming\n");
    printf("• Sleep + Block combinations\n");
    printf("• Error handling\n");
    printf("• Stress tests\n");
    printf("• Edge cases\n");
    printf("═══════════════════════════════════════════════════════════\n");
    
    // Run all tests
    test_basic_sleep();
    test_multiple_sleeps();
    test_basic_blocking();
    test_sleep_block_combination();
    test_sleep_error_cases();
    test_block_error_cases();
    test_rapid_sleep_operations();
    test_double_block_resume();
    
    // Print summary
    print_summary();
    
    return (tests_completed == total_tests) ? 0 : 1;
}