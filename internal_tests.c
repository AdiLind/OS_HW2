/* how to compile 
gcc -std=c17 -Wall -Wextra -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L \
    internal_tests.c uthreads.c -o test_internal  
./test_internal

*/

#include "uthreads.h"
#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <sys/time.h>

/* 
 * These tests verify internal behavior that's not directly exposed by the API.
 * They test critical sections, signal handling setup, and internal state.
 */

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_START(name) \
    printf("🔬 Internal Test: %s... ", name); \
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

/* Global variable to test signal behavior */
static volatile int signal_received = 0;

/* Test signal handler */
static void test_signal_handler(int signum) {
    signal_received = signum;
}

void test_signal_mask_setup(void) {
    TEST_START("signal mask initialization");
    
    uthread_init(100000);
    
    // Test that we can register a SIGVTALRM handler
    struct sigaction sa;
    sa.sa_handler = test_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    int result = sigaction(SIGVTALRM, &sa, NULL);
    TEST_ASSERT(result == 0, "Should be able to register SIGVTALRM handler");
    
    TEST_PASS();
}

void test_multiple_init_cleanup(void) {
    TEST_START("multiple initialization cleanup");
    
    // Initialize with one quantum value
    int result1 = uthread_init(50000);
    TEST_ASSERT(result1 == 0, "First init should succeed");
    
    int tid1 = uthread_get_tid();
    int total1 = uthread_get_total_quantums();
    
    // Initialize again with different quantum value
    int result2 = uthread_init(100000);
    TEST_ASSERT(result2 == 0, "Second init should succeed");
    
    int tid2 = uthread_get_tid();
    int total2 = uthread_get_total_quantums();
    
    // State should be properly reset
    TEST_ASSERT(tid2 == 0, "TID should reset to 0");
    TEST_ASSERT(total2 == 1, "Total quantums should reset to 1");
    
    TEST_PASS();
}

void test_quantum_validation_edge_cases(void) {
    TEST_START("quantum validation edge cases");
    
    // Test just above zero
    int result1 = uthread_init(1);
    TEST_ASSERT(result1 == 0, "Quantum of 1 should be valid");
    
    // Test typical values
    int result2 = uthread_init(10000);  // 10ms
    TEST_ASSERT(result2 == 0, "10ms quantum should be valid");
    
    int result3 = uthread_init(1000000); // 1s
    TEST_ASSERT(result3 == 0, "1s quantum should be valid");
    
    // Test very large value (but still positive)
    int result4 = uthread_init(2000000000); // ~33 minutes
    TEST_ASSERT(result4 == 0, "Very large quantum should be valid");
    
    TEST_PASS();
}

void test_state_initialization(void) {
    TEST_START("internal state initialization");
    
    uthread_init(100000);
    
    // Test main thread state
    TEST_ASSERT(uthread_get_tid() == 0, "Main thread should be TID 0");
    TEST_ASSERT(uthread_get_total_quantums() == 1, "Should start with 1 quantum");
    TEST_ASSERT(uthread_get_quantums(0) == 1, "Main thread should have 1 quantum");
    
    // Test that all other TIDs are invalid (not initialized)
    for (int i = 1; i < 10; i++) {  // Test first 10 TIDs
        FILE* null_stderr = freopen("/dev/null", "w", stderr);
        int quantums = uthread_get_quantums(i);
        freopen("/dev/tty", "w", stderr);
        
        TEST_ASSERT(quantums == -1, "Uninitialized threads should return -1");
    }
    
    TEST_PASS();
}

void test_consistent_state_across_calls(void) {
    TEST_START("consistent state across API calls");
    
    uthread_init(100000);
    
    // Store initial values
    int initial_tid = uthread_get_tid();
    int initial_total = uthread_get_total_quantums();
    int initial_my_quantums = uthread_get_quantums(0);
    
    // Call functions multiple times and verify consistency
    for (int i = 0; i < 100; i++) {
        TEST_ASSERT(uthread_get_tid() == initial_tid, 
                   "TID should remain consistent across calls");
        TEST_ASSERT(uthread_get_total_quantums() == initial_total, 
                   "Total quantums should remain consistent");
        TEST_ASSERT(uthread_get_quantums(0) == initial_my_quantums, 
                   "Main thread quantums should remain consistent");
    }
    
    TEST_PASS();
}

void test_invalid_tid_ranges(void) {
    TEST_START("invalid TID range handling");
    
    uthread_init(100000);
    
    FILE* null_stderr = freopen("/dev/null", "w", stderr);
    
    // Test various invalid TID values
    int invalid_tids[] = {-1, -100, -1000, MAX_THREAD_NUM, MAX_THREAD_NUM + 1, 
                         MAX_THREAD_NUM + 100, 1000000};
    int num_invalid = sizeof(invalid_tids) / sizeof(invalid_tids[0]);
    
    for (int i = 0; i < num_invalid; i++) {
        int result = uthread_get_quantums(invalid_tids[i]);
        TEST_ASSERT(result == -1, "Invalid TID should return -1");
    }
    
    freopen("/dev/tty", "w", stderr);
    TEST_PASS();
}

void test_error_message_suppression(void) {
    TEST_START("error message handling");
    
    uthread_init(100000);
    
    // Redirect stderr to count error messages
    FILE* temp_file = tmpfile();
    FILE* original_stderr = stderr;
    stderr = temp_file;
    
    // Trigger some errors
    uthread_get_quantums(-1);
    uthread_get_quantums(MAX_THREAD_NUM);
    uthread_get_quantums(999);
    
    // Restore stderr
    stderr = original_stderr;
    
    // Check that error messages were written
    fseek(temp_file, 0, SEEK_END);
    long error_output_size = ftell(temp_file);
    fclose(temp_file);
    
    TEST_ASSERT(error_output_size > 0, "Error messages should be generated for invalid TIDs");
    
    TEST_PASS();
}

void test_library_limits(void) {
    TEST_START("library limits and constants");
    
    uthread_init(100000);
    
    // Verify constants are reasonable
    TEST_ASSERT(MAX_THREAD_NUM > 0, "MAX_THREAD_NUM should be positive");
    TEST_ASSERT(STACK_SIZE > 0, "STACK_SIZE should be positive");
    TEST_ASSERT(STACK_SIZE >= 1024, "STACK_SIZE should be at least 1KB");
    
    // Test behavior at the limits
    TEST_ASSERT(uthread_get_quantums(MAX_THREAD_NUM - 1) == -1, 
               "Last possible TID should be unused initially");
    
    TEST_PASS();
}

void test_memory_safety_basic(void) {
    TEST_START("basic memory safety");
    
    // Initialize multiple times to test for memory leaks/corruption
    for (int i = 0; i < 10; i++) {
        uthread_init(50000 + i * 1000);
        
        TEST_ASSERT(uthread_get_tid() == 0, "TID should always be 0 after init");
        TEST_ASSERT(uthread_get_total_quantums() == 1, "Total should reset to 1");
    }
    
    TEST_PASS();
}

/* Compilation test - ensure all required types and functions are available */
void test_compilation_requirements(void) {
    TEST_START("compilation requirements");
    
    // Test that all required types are defined
    thread_state_t state = THREAD_UNUSED;
    thread_entry_point entry = NULL;
    
    // Test that all enum values are defined
    TEST_ASSERT(THREAD_UNUSED == 0, "THREAD_UNUSED should be 0");
    TEST_ASSERT(THREAD_READY != THREAD_UNUSED, "States should be different");
    TEST_ASSERT(THREAD_RUNNING != THREAD_READY, "States should be different");
    TEST_ASSERT(THREAD_BLOCKED != THREAD_RUNNING, "States should be different");
    TEST_ASSERT(THREAD_TERMINATED != THREAD_BLOCKED, "States should be different");
    
    // Suppress unused variable warnings
    (void)state;
    (void)entry;
    
    TEST_PASS();
}

int main(void) {
    printf("🔬 Starting Internal UThreads Tests\n");
    printf("====================================\n\n");
    
    /* Signal and system tests */
    test_signal_mask_setup();
    test_multiple_init_cleanup();
    
    /* Parameter validation tests */
    test_quantum_validation_edge_cases();
    
    /* State management tests */
    test_state_initialization();
    test_consistent_state_across_calls();
    
    /* Error handling tests */
    test_invalid_tid_ranges();
    test_error_message_suppression();
    
    /* Limits and safety tests */
    test_library_limits();
    test_memory_safety_basic();
    
    /* Compilation tests */
    test_compilation_requirements();
    
    /* Summary */
    printf("\n====================================\n");
    printf("📊 Internal Test Results:\n");
    printf("✅ Tests Passed: %d\n", tests_passed);
    printf("❌ Tests Failed: %d\n", tests_failed);
    printf("📈 Success Rate: %.1f%%\n", 
           (tests_failed + tests_passed > 0) ? 
           (100.0 * tests_passed / (tests_passed + tests_failed)) : 0.0);
    
    if (tests_failed == 0) {
        printf("🎉 All internal tests passed!\n");
        return 0;
    } else {
        printf("🚨 Some internal tests failed.\n");
        return 1;
    }
}