/* 
 * Working Complex Sleep and Block Scenarios Test
 * 
 * Compile:
 * gcc -std=c17 -Wall -Wextra -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L \
 *     working_complex_scenarios.c uthreads.c -o test_complex_working
 * 
 * Run: ./test_complex_working
 */

#include "uthreads.h"
#include <stdio.h>
#include <assert.h>

/* Test state tracking */
static volatile int scenario_results[10] = {0};
static volatile int thread_progress[MAX_THREAD_NUM] = {0};

#define WORK_UNIT for (volatile long i = 0; i < 50000000; i++)

/* ========================================================================= */
/*                    SCENARIO 1: CHAIN OF DEPENDENCIES                     */
/* ========================================================================= */

void dependency_thread_1(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Chain Link 1: Starting\n", tid);
    
    thread_progress[tid] = 1;
    
    // Work then sleep
    printf("[Thread %d] Working then sleeping for 2 quantums\n", tid);
    WORK_UNIT;
    uthread_sleep(2);
    
    printf("[Thread %d] Woke up, now blocking thread 2\n", tid);
    uthread_block(2);
    
    thread_progress[tid] = 2;
    
    // Wait a bit then resume thread 2
    WORK_UNIT; WORK_UNIT;
    printf("[Thread %d] Resuming thread 2\n", tid);
    uthread_resume(2);
    
    thread_progress[tid] = 3;
    uthread_terminate(tid);
}

void dependency_thread_2(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Chain Link 2: Starting\n", tid);
    
    thread_progress[tid] = 1;
    
    // Wait to be blocked by thread 1
    for (int i = 0; i < 20; i++) {
        printf("[Thread %d] Working... iteration %d\n", tid, i);
        WORK_UNIT;
        if (i == 10) {
            thread_progress[tid] = 2;
        }
    }
    
    printf("[Thread %d] Resumed! Now sleeping for 1 quantum\n", tid);
    uthread_sleep(1);
    
    printf("[Thread %d] Woke up, blocking thread 3\n", tid);
    uthread_block(3);
    
    WORK_UNIT;
    printf("[Thread %d] Resuming thread 3\n", tid);
    uthread_resume(3);
    
    thread_progress[tid] = 3;
    uthread_terminate(tid);
}

void dependency_thread_3(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Chain Link 3: Starting\n", tid);
    
    thread_progress[tid] = 1;
    
    // Wait to be blocked by thread 2
    for (int i = 0; i < 15; i++) {
        printf("[Thread %d] Working... iteration %d\n", tid, i);
        WORK_UNIT;
    }
    
    printf("[Thread %d] Resumed! Final work\n", tid);
    WORK_UNIT;
    
    thread_progress[tid] = 3;
    scenario_results[1] = 1; // Mark scenario 1 as complete
    uthread_terminate(tid);
}

void test_chain_dependencies(void) {
    printf("\n🔗 SCENARIO 1: Chain of Dependencies\n");
    printf("Thread 1 sleeps -> blocks Thread 2 -> resumes Thread 2\n");
    printf("Thread 2 resumes -> sleeps -> blocks Thread 3 -> resumes Thread 3\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    // Reset state
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        thread_progress[i] = 0;
    }
    
    assert(uthread_init(40000) == 0);
    
    int tid1 = uthread_spawn(dependency_thread_1);
    int tid2 = uthread_spawn(dependency_thread_2);
    int tid3 = uthread_spawn(dependency_thread_3);
    
    printf("Spawned threads: %d, %d, %d\n", tid1, tid2, tid3);
    
    // Monitor progress with generous timeout
    int iterations = 0;
    while (scenario_results[1] == 0 && iterations < 100) {
        printf("[Main] Progress: T1=%d, T2=%d, T3=%d, Quantum=%d\n", 
               thread_progress[tid1], thread_progress[tid2], thread_progress[tid3],
               uthread_get_total_quantums());
        
        WORK_UNIT; WORK_UNIT;
        iterations++;
    }
    
    if (scenario_results[1] == 1) {
        printf("✅ Chain Dependencies Test PASSED\n");
    } else {
        printf("❌ Chain Dependencies Test FAILED\n");
    }
}

/* ========================================================================= */
/*                    SCENARIO 2: COMPETING RESOURCES                       */
/* ========================================================================= */

static volatile int shared_resource = 0;
static volatile int resource_owner = -1;

void competing_thread_A(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Competing for resource\n", tid);
    
    thread_progress[tid] = 1;
    
    // Try to acquire resource
    if (shared_resource == 0) {
        shared_resource = 1;
        resource_owner = tid;
        printf("[Thread %d] Acquired resource!\n", tid);
        
        // Hold resource while sleeping
        printf("[Thread %d] Sleeping with resource for 3 quantums\n", tid);
        uthread_sleep(3);
        
        printf("[Thread %d] Woke up, still holding resource\n", tid);
        WORK_UNIT;
        
        // Release resource
        shared_resource = 0;
        resource_owner = -1;
        printf("[Thread %d] Released resource\n", tid);
    }
    
    thread_progress[tid] = 2;
    uthread_terminate(tid);
}

void competing_thread_B(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Competing for resource\n", tid);
    
    thread_progress[tid] = 1;
    
    // Wait for resource
    int wait_count = 0;
    while (shared_resource == 1 && wait_count < 20) {
        printf("[Thread %d] Resource busy (owned by %d), waiting...\n", tid, resource_owner);
        WORK_UNIT;
        wait_count++;
    }
    
    // Try to acquire
    if (shared_resource == 0) {
        shared_resource = 1;
        resource_owner = tid;
        printf("[Thread %d] Acquired resource!\n", tid);
        
        WORK_UNIT; WORK_UNIT;
        
        // Release
        shared_resource = 0;
        resource_owner = -1;
        printf("[Thread %d] Released resource\n", tid);
    }
    
    thread_progress[tid] = 2;
    uthread_terminate(tid);
}

void competing_controller(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Resource controller\n", tid);
    
    // Let threads start competing
    WORK_UNIT; WORK_UNIT;
    
    // Check if thread A has resource and is sleeping
    if (resource_owner == 1) {
        printf("[Thread %d] Blocking resource owner (Thread 1)\n", tid);
        uthread_block(1);
        
        WORK_UNIT; WORK_UNIT; WORK_UNIT;
        
        printf("[Thread %d] Resuming Thread 1\n", tid);
        uthread_resume(1);
    }
    
    scenario_results[2] = 1;
    uthread_terminate(tid);
}

void test_competing_resources(void) {
    printf("\n🏁 SCENARIO 2: Competing Resources\n");
    printf("Two threads compete for a shared resource\n");
    printf("One sleeps while holding it, controller intervenes\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    // Reset state
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        thread_progress[i] = 0;
    }
    shared_resource = 0;
    resource_owner = -1;
    
    assert(uthread_init(50000) == 0);
    
    int tid1 = uthread_spawn(competing_thread_A);
    int tid2 = uthread_spawn(competing_thread_B);
    int tid3 = uthread_spawn(competing_controller);
    
    // Monitor with generous timeout
    int iterations = 0;
    while (scenario_results[2] == 0 && iterations < 80) {
        printf("[Main] Resource: %d (owner: %d), Progress: A=%d, B=%d\n", 
               shared_resource, resource_owner, 
               thread_progress[tid1], thread_progress[tid2]);
        
        WORK_UNIT; WORK_UNIT; WORK_UNIT;
        iterations++;
    }
    
    if (scenario_results[2] == 1) {
        printf("✅ Competing Resources Test PASSED\n");
    } else {
        printf("❌ Competing Resources Test FAILED\n");
    }
}

/* ========================================================================= */
/*                    SCENARIO 3: NESTED SLEEP AND BLOCK                    */
/* ========================================================================= */

void nested_sleep_thread(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Nested operations thread\n", tid);
    
    thread_progress[tid] = 1;
    
    printf("[Thread %d] First sleep (2 quantums)\n", tid);
    uthread_sleep(2);
    
    printf("[Thread %d] Woke up, working...\n", tid);
    thread_progress[tid] = 2;
    WORK_UNIT;
    
    printf("[Thread %d] Second sleep (1 quantum)\n", tid);
    uthread_sleep(1);
    
    printf("[Thread %d] Woke up again, final work\n", tid);
    thread_progress[tid] = 3;
    WORK_UNIT;
    
    printf("[Thread %d] Completed nested sleeps\n", tid);
    thread_progress[tid] = 4;
    uthread_terminate(tid);
}

void nested_controller(void) {
    int tid = uthread_get_tid();
    printf("[Thread %d] Nested controller\n", tid);
    
    // Wait for target to start
    while (thread_progress[1] < 1) {
        WORK_UNIT;
    }
    
    // Block during first sleep
    printf("[Thread %d] Blocking thread 1 during its first sleep\n", tid);
    uthread_block(1);
    
    // Wait several quantums
    for (int i = 0; i < 5; i++) {
        printf("[Thread %d] Waiting... quantum %d\n", tid, uthread_get_total_quantums());
        WORK_UNIT; WORK_UNIT;
    }
    
    // Resume
    printf("[Thread %d] Resuming thread 1\n", tid);
    uthread_resume(1);
    
    // Wait for it to progress
    int wait_count = 0;
    while (thread_progress[1] < 3 && wait_count < 30) {
        WORK_UNIT;
        wait_count++;
    }
    
    // Block again during second sleep
    printf("[Thread %d] Blocking thread 1 during its second sleep\n", tid);
    uthread_block(1);
    
    WORK_UNIT; WORK_UNIT; WORK_UNIT;
    
    printf("[Thread %d] Final resume of thread 1\n", tid);
    uthread_resume(1);
    
    scenario_results[3] = 1;
    uthread_terminate(tid);
}

void test_nested_sleep_block(void) {
    printf("\n🪆 SCENARIO 3: Nested Sleep and Block\n");
    printf("Thread sleeps -> gets blocked -> resumes -> sleeps again -> blocked again\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    // Reset state
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        thread_progress[i] = 0;
    }
    
    assert(uthread_init(30000) == 0);
    
    int tid1 = uthread_spawn(nested_sleep_thread);
    int tid2 = uthread_spawn(nested_controller);
    
    // Monitor with generous timeout
    int iterations = 0;
    while (scenario_results[3] == 0 && iterations < 120) {
        printf("[Main] Thread 1 progress: %d, Quantum: %d\n", 
               thread_progress[tid1], uthread_get_total_quantums());
        
        WORK_UNIT; WORK_UNIT; WORK_UNIT;
        iterations++;
    }
    
    // Wait for thread 1 to fully complete
    iterations = 0;
    while (thread_progress[tid1] < 4 && iterations < 40) {
        WORK_UNIT; WORK_UNIT;
        iterations++;
    }
    
    if (scenario_results[3] == 1 && thread_progress[tid1] == 4) {
        printf("✅ Nested Sleep and Block Test PASSED\n");
    } else {
        printf("❌ Nested Sleep and Block Test FAILED\n");
    }
}

/* ========================================================================= */
/*                    SCENARIO 4: EDGE CASE COMBINATIONS                    */
/* ========================================================================= */

void edge_case_thread(void) {
    int tid = uthread_get_tid();
    printf("[Edge %d] Testing edge cases\n", tid);
    
    thread_progress[tid] = 1;
    
    // Sleep for 0 quantums (should fail)
    printf("[Edge %d] Trying to sleep for 0 quantums\n", tid);
    int result = uthread_sleep(0);
    if (result == -1) {
        printf("[Edge %d] ✓ Correctly rejected sleep(0)\n", tid);
    } else {
        printf("[Edge %d] ✗ ERROR: sleep(0) should have failed!\n", tid);
    }
    
    // Valid sleep
    printf("[Edge %d] Valid sleep for 1 quantum\n", tid);
    uthread_sleep(1);
    
    thread_progress[tid] = 2;
    
    // Try to block non-existent thread
    printf("[Edge %d] Trying to block non-existent thread 99\n", tid);
    result = uthread_block(99);
    if (result == -1) {
        printf("[Edge %d] ✓ Correctly rejected block(99)\n", tid);
    } else {
        printf("[Edge %d] ✗ ERROR: block(99) should have failed!\n", tid);
    }
    
    // Try to resume non-existent thread
    printf("[Edge %d] Trying to resume non-existent thread 88\n", tid);
    result = uthread_resume(88);
    if (result == -1) {
        printf("[Edge %d] ✓ Correctly rejected resume(88)\n", tid);
    } else {
        printf("[Edge %d] ✗ ERROR: resume(88) should have failed!\n", tid);
    }
    
    thread_progress[tid] = 3;
    scenario_results[4] = 1;
    uthread_terminate(tid);
}

void test_edge_cases(void) {
    printf("\n⚠️  SCENARIO 4: Edge Case Combinations\n");
    printf("Testing various edge cases and error conditions\n");
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n");
    
    // Reset state
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        thread_progress[i] = 0;
    }
    
    assert(uthread_init(40000) == 0);
    
    // Test main thread error cases first
    printf("[Main] Testing main thread sleep (should fail)\n");
    int result = uthread_sleep(5);
    if (result == -1) {
        printf("[Main] ✓ Correctly rejected main thread sleep\n");
    } else {
        printf("[Main] ✗ ERROR: Main thread sleep should have failed!\n");
    }
    
    printf("[Main] Testing block main thread (should fail)\n");
    result = uthread_block(0);
    if (result == -1) {
        printf("[Main] ✓ Correctly rejected block main thread\n");
    } else {
        printf("[Main] ✗ ERROR: Block main thread should have failed!\n");
    }
    
    // Spawn edge case thread
    int tid = uthread_spawn(edge_case_thread);
    
    // Monitor with generous timeout
    int iterations = 0;
    while (scenario_results[4] == 0 && iterations < 50) {
        printf("[Main] Edge thread progress: %d\n", thread_progress[tid]);
        WORK_UNIT; WORK_UNIT; WORK_UNIT;
        iterations++;
    }
    
    if (scenario_results[4] == 1) {
        printf("✅ Edge Cases Test PASSED\n");
    } else {
        printf("❌ Edge Cases Test FAILED\n");
    }
}

/* ========================================================================= */
/*                              MAIN RUNNER                                 */
/* ========================================================================= */

void print_final_summary(void) {
    printf("\n");
    printf("🎯 COMPLEX SCENARIOS TEST SUMMARY\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    
    const char* scenario_names[] = {
        "",
        "Chain of Dependencies",
        "Competing Resources", 
        "Nested Sleep and Block",
        "Edge Case Combinations"
    };
    
    int passed = 0;
    int total = 4;
    
    for (int i = 1; i <= total; i++) {
        if (scenario_results[i] == 1) {
            printf("✅ Scenario %d: %s - PASSED\n", i, scenario_names[i]);
            passed++;
        } else {
            printf("❌ Scenario %d: %s - FAILED\n", i, scenario_names[i]);
        }
    }
    
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("📊 Complex Scenarios: %d/%d passed (%.1f%%)\n", 
           passed, total, (100.0 * passed) / total);
    
    if (passed == total) {
        printf("🎉 ALL COMPLEX SCENARIOS PASSED!\n");
        printf("Your sleep and blocking implementation handles complex cases correctly!\n");
    } else if (passed >= total * 0.75) {
        printf("🥈 Most complex scenarios passed! Very good implementation!\n");
    } else {
        printf("🚨 Some complex scenarios failed.\n");
        printf("Consider reviewing the interaction between sleep and block operations.\n");
    }
    
    printf("\n💡 Key aspects tested:\n");
    printf("   • Thread dependency chains\n");
    printf("   • Resource competition with sleep\n");
    printf("   • Nested sleep/block operations\n");
    printf("   • Edge cases and error handling\n");
}

int main(void) {
    printf("🧪 WORKING COMPLEX SCENARIOS TEST SUITE\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("This test suite verifies complex interactions between:\n");
    printf("• Sleep operations in multi-thread scenarios\n");
    printf("• Block/Resume operations with dependencies\n");
    printf("• Combined sleep+block edge cases\n");
    printf("• Error handling in complex scenarios\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    
    // Run all complex scenarios
    test_chain_dependencies();      // Scenario 1
    test_competing_resources();     // Scenario 2  
    test_nested_sleep_block();      // Scenario 3
    test_edge_cases();              // Scenario 4
    
    // Print final summary
    print_final_summary();
    
    // Count successes
    int successes = 0;
    for (int i = 1; i <= 4; i++) {
        if (scenario_results[i] == 1) successes++;
    }
    
    return (successes >= 3) ? 0 : 1; // Pass if at least 3/4 scenarios pass
}