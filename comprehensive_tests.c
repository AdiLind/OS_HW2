/* how to compile 
gcc -std=c17 -Wall -Wextra -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L \
    comprehensive_tests.c uthreads.c -o test_comprehensive
./test_comprehensive

*/

#include "uthreads.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

/* Test counters */
static int tests_passed = 0;
static int tests_failed = 0;

/* Test macros */
#define TEST_START(name) \
    printf("🧪 Testing: %s... ", name); \
    fflush(stdout);

#define TEST_ASSERT(condition, message) \
    if (!(condition)) { \
        printf("❌ FAILED: %s\n", message); \
        tests_failed++; \
        return; \
    }

#define TEST_PASS() \
    printf("✅ PASSED\n"); \
    tests_passed++;

/* Helper function to redirect stderr temporarily */
static FILE* redirect_stderr_to_null(void) {
    fflush(stderr);
    return freopen("/dev/null", "w", stderr);
}

static void restore_stderr(FILE* original_stderr) {
    fflush(stderr);
    freopen("/dev/tty", "w", stderr);
}

/* ===================================================================== */
/*                           Test Functions                              */
/* ===================================================================== */

void test_uthread_init_valid_params(void) {
    TEST_START("uthread_init with valid parameters");
    
    int result = uthread_init(100000);
    TEST_ASSERT(result == 0, "uthread_init should return 0 on success");
    
    // Check initial state
    TEST_ASSERT(uthread_get_tid() == 0, "Main thread should have TID 0");
    TEST_ASSERT(uthread_get_total_quantums() == 1, "Total quantums should start at 1");
    TEST_ASSERT(uthread_get_quantums(0) == 1, "Main thread should have 1 quantum");
    
    TEST_PASS();
}

void test_uthread_init_invalid_params(void) {
    TEST_START("uthread_init with invalid parameters");
    
    FILE* original_stderr = redirect_stderr_to_null();
    
    // Test negative quantum
    int result1 = uthread_init(-1);
    TEST_ASSERT(result1 == -1, "uthread_init should return -1 for negative quantum");
    
    // Test zero quantum
    int result2 = uthread_init(0);
    TEST_ASSERT(result2 == -1, "uthread_init should return -1 for zero quantum");
    
    restore_stderr(original_stderr);
    TEST_PASS();
}

void test_uthread_init_multiple_calls(void) {
    TEST_START("multiple uthread_init calls");
    
    // First init
    int result1 = uthread_init(50000);
    TEST_ASSERT(result1 == 0, "First uthread_init should succeed");
    
    // Second init should work (re-initialize)
    int result2 = uthread_init(75000);
    TEST_ASSERT(result2 == 0, "Second uthread_init should also succeed");
    
    // Verify state reset
    TEST_ASSERT(uthread_get_tid() == 0, "After re-init, TID should be 0");
    TEST_ASSERT(uthread_get_total_quantums() == 1, "After re-init, quantums should reset to 1");
    
    TEST_PASS();
}

void test_get_functions_before_init(void) {
    TEST_START("get functions before initialization");
    
    // Note: This test assumes we haven't called uthread_init yet
    // In practice, this might be undefined behavior, but we test it anyway
    
    int tid = uthread_get_tid();
    int total = uthread_get_total_quantums();
    
    // Should return some default values (probably -1 and 0)
    printf("(tid=%d, total=%d) ", tid, total);
    
    TEST_PASS(); // We don't crash, so it's a pass
}

void test_get_quantums_invalid_tid(void) {
    TEST_START("uthread_get_quantums with invalid TID");
    
    // Make sure we're initialized
    uthread_init(100000);
    
    FILE* original_stderr = redirect_stderr_to_null();
    
    // Test negative TID
    int result1 = uthread_get_quantums(-1);
    TEST_ASSERT(result1 == -1, "Should return -1 for negative TID");
    
    // Test TID too large
    int result2 = uthread_get_quantums(MAX_THREAD_NUM);
    TEST_ASSERT(result2 == -1, "Should return -1 for TID >= MAX_THREAD_NUM");
    
    // Test unused TID (assuming TID 1 is not used yet)
    int result3 = uthread_get_quantums(1);
    TEST_ASSERT(result3 == -1, "Should return -1 for unused TID");
    
    restore_stderr(original_stderr);
    TEST_PASS();
}

void test_get_quantums_valid_tid(void) {
    TEST_START("uthread_get_quantums with valid TID");
    
    uthread_init(100000);
    
    // Test main thread (TID 0)
    int result = uthread_get_quantums(0);
    TEST_ASSERT(result == 1, "Main thread should have 1 quantum");
    
    TEST_PASS();
}

void test_boundary_values(void) {
    TEST_START("boundary values");
    
    // Test with very small quantum
    int result1 = uthread_init(1);
    TEST_ASSERT(result1 == 0, "Should accept quantum of 1 microsecond");
    
    // Test with very large quantum
    int result2 = uthread_init(1000000);  // 1 second
    TEST_ASSERT(result2 == 0, "Should accept large quantum values");
    
    // Test all possible TID values
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        if (i == 0) {
            // Main thread should exist
            int quantums = uthread_get_quantums(i);
            TEST_ASSERT(quantums >= 0, "Main thread should have non-negative quantums");
        } else {
            // Other threads should not exist yet
            FILE* original_stderr = redirect_stderr_to_null();
            int quantums = uthread_get_quantums(i);
            restore_stderr(original_stderr);
            TEST_ASSERT(quantums == -1, "Unused threads should return -1");
        }
    }
    
    TEST_PASS();
}

void test_state_consistency(void) {
    TEST_START("state consistency");
    
    uthread_init(100000);
    
    // Multiple calls to getters should return consistent results
    int tid1 = uthread_get_tid();
    int tid2 = uthread_get_tid();
    TEST_ASSERT(tid1 == tid2, "Multiple calls to get_tid should return same value");
    
    int total1 = uthread_get_total_quantums();
    int total2 = uthread_get_total_quantums();
    TEST_ASSERT(total1 == total2, "Multiple calls to get_total_quantums should return same value");
    
    int my_quantums1 = uthread_get_quantums(0);
    int my_quantums2 = uthread_get_quantums(0);
    TEST_ASSERT(my_quantums1 == my_quantums2, "Multiple calls to get_quantums should return same value");
    
    TEST_PASS();
}

void test_extreme_parameters(void) {
    TEST_START("extreme parameters");
    
    FILE* original_stderr = redirect_stderr_to_null();
    
    // Test with maximum integer
    int result1 = uthread_init(2147483647);  // INT_MAX
    TEST_ASSERT(result1 == 0, "Should handle very large quantum values");
    
    restore_stderr(original_stderr);
    TEST_PASS();
}

void test_library_robustness(void) {
    TEST_START("library robustness");
    
    // Test calling functions in different orders
    uthread_init(50000);
    
    int tid = uthread_get_tid();
    int total = uthread_get_total_quantums();
    int my_quantums = uthread_get_quantums(tid);
    
    // Re-init and check values change appropriately
    uthread_init(75000);
    
    int new_tid = uthread_get_tid();
    int new_total = uthread_get_total_quantums();
    int new_my_quantums = uthread_get_quantums(new_tid);
    
    TEST_ASSERT(new_tid == 0, "TID should be 0 after re-init");
    TEST_ASSERT(new_total == 1, "Total quantums should reset to 1");
    TEST_ASSERT(new_my_quantums == 1, "Main thread quantums should reset to 1");
    
    TEST_PASS();
}

/* ===================================================================== */
/*                           Stress Tests                                */
/* ===================================================================== */

void test_repeated_operations(void) {
    TEST_START("repeated operations stress test");
    
    uthread_init(100000);
    
    // Call each function many times
    for (int i = 0; i < 10000; i++) {
        uthread_get_tid();
        uthread_get_total_quantums();
        uthread_get_quantums(0);
    }
    
    // Values should still be consistent
    TEST_ASSERT(uthread_get_tid() == 0, "TID should remain 0");
    TEST_ASSERT(uthread_get_total_quantums() == 1, "Total quantums should remain 1");
    TEST_ASSERT(uthread_get_quantums(0) == 1, "Main thread quantums should remain 1");
    
    TEST_PASS();
}

/* ===================================================================== */
/*                              Main                                     */
/* ===================================================================== */

int main(void) {
    printf("🚀 Starting Comprehensive UThreads Tests\n");
    printf("==========================================\n\n");
    
    /* Basic functionality tests */
    test_uthread_init_valid_params();
    test_uthread_init_invalid_params();
    test_uthread_init_multiple_calls();
    
    /* Getter function tests */
    test_get_quantums_invalid_tid();
    test_get_quantums_valid_tid();
    
    /* Boundary and edge case tests */
    test_boundary_values();
    test_state_consistency();
    test_extreme_parameters();
    
    /* Robustness tests */
    test_library_robustness();
    test_repeated_operations();
    
    /* Summary */
    printf("\n==========================================\n");
    printf("📊 Test Results Summary:\n");
    printf("✅ Tests Passed: %d\n", tests_passed);
    printf("❌ Tests Failed: %d\n", tests_failed);
    printf("📈 Success Rate: %.1f%%\n", 
           (tests_failed + tests_passed > 0) ? 
           (100.0 * tests_passed / (tests_passed + tests_failed)) : 0.0);
    
    if (tests_failed == 0) {
        printf("🎉 All tests passed! The implemented functionality is working correctly.\n");
        return 0;
    } else {
        printf("🚨 Some tests failed. Please review the implementation.\n");
        return 1;
    }
}