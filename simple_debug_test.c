/* 
 * Simple Debug Test - Let's see what's actually happening
 * 
 * Compile:
 * gcc -std=c17 -Wall -Wextra -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L \
 *     simple_debug_test.c uthreads.c -o simple_debug
 * 
 * Run: ./simple_debug
 */

#include "uthreads.h"
#include <stdio.h>
#include <assert.h>

static volatile int thread_started = 0;
static volatile int thread_completed = 0;
static volatile int sleep_started = 0;
static volatile int sleep_completed = 0;

void simple_test_thread(void) {
    int tid = uthread_get_tid();
    printf("🧵 [Thread %d] Started!\n", tid);
    thread_started = 1;
    
    // Do some work
    printf("🧵 [Thread %d] Doing work...\n", tid);
    for (volatile long i = 0; i < 50000000; i++);
    
    printf("🧵 [Thread %d] About to sleep for 2 quantums\n", tid);
    printf("🧵 [Thread %d] Current total quantums: %d\n", tid, uthread_get_total_quantums());
    
    sleep_started = uthread_get_total_quantums();
    
    printf("🧵 [Thread %d] Calling uthread_sleep(2)...\n", tid);
    uthread_sleep(2);
    
    printf("🧵 [Thread %d] Woke up from sleep!\n", tid);
    sleep_completed = uthread_get_total_quantums();
    
    printf("🧵 [Thread %d] Sleep duration: %d quantums\n", tid, sleep_completed - sleep_started);
    
    thread_completed = 1;
    printf("🧵 [Thread %d] Terminating\n", tid);
    uthread_terminate(tid);
}

int main(void) {
    printf("🚀 SIMPLE DEBUG TEST\n");
    printf("====================\n");
    
    printf("📋 Step 1: Initialize library\n");
    if (uthread_init(100000) != 0) {  // 100ms quantum
        printf("❌ Init failed!\n");
        return 1;
    }
    printf("✅ Init successful\n");
    
    printf("📋 Step 2: Check initial state\n");
    printf("   - Main TID: %d\n", uthread_get_tid());
    printf("   - Total quantums: %d\n", uthread_get_total_quantums());
    printf("   - Main quantums: %d\n", uthread_get_quantums(0));
    
    printf("📋 Step 3: Test timer by doing work\n");
    printf("   - Quantums before work: %d\n", uthread_get_total_quantums());
    
    // Do heavy work to see if timer fires
    for (int i = 0; i < 10; i++) {
        printf("   - Work iteration %d, quantums: %d\n", i, uthread_get_total_quantums());
        for (volatile long j = 0; j < 100000000; j++);
    }
    
    int quantums_after_work = uthread_get_total_quantums();
    printf("   - Quantums after work: %d\n", quantums_after_work);
    
    if (quantums_after_work > 1) {
        printf("✅ Timer is working! Quantums increased.\n");
    } else {
        printf("❌ Timer NOT working! Quantums didn't increase.\n");
        printf("   This is the main problem!\n");
    }
    
    printf("📋 Step 4: Spawn thread\n");
    int tid = uthread_spawn(simple_test_thread);
    if (tid == -1) {
        printf("❌ Spawn failed!\n");
        return 1;
    }
    printf("✅ Spawned thread with TID: %d\n", tid);
    
    printf("📋 Step 5: Wait for thread to start\n");
    int wait_iterations = 0;
    while (thread_started == 0 && wait_iterations < 20) {
        printf("   - Waiting for thread to start... iteration %d, quantums: %d\n", 
               wait_iterations, uthread_get_total_quantums());
        for (volatile long i = 0; i < 100000000; i++);
        wait_iterations++;
    }
    
    if (thread_started) {
        printf("✅ Thread started successfully\n");
    } else {
        printf("❌ Thread never started! Context switch problem?\n");
        return 1;
    }
    
    printf("📋 Step 6: Wait for thread to sleep\n");
    wait_iterations = 0;
    while (sleep_started == 0 && wait_iterations < 20) {
        printf("   - Waiting for sleep to start... iteration %d, quantums: %d\n", 
               wait_iterations, uthread_get_total_quantums());
        for (volatile long i = 0; i < 100000000; i++);
        wait_iterations++;
    }
    
    if (sleep_started > 0) {
        printf("✅ Thread entered sleep at quantum %d\n", sleep_started);
    } else {
        printf("❌ Thread never entered sleep!\n");
        return 1;
    }
    
    printf("📋 Step 7: Wait for thread to wake up\n");
    wait_iterations = 0;
    while (sleep_completed == 0 && wait_iterations < 30) {
        printf("   - Waiting for wake up... iteration %d, quantums: %d\n", 
               wait_iterations, uthread_get_total_quantums());
        for (volatile long i = 0; i < 100000000; i++);
        wait_iterations++;
    }
    
    if (sleep_completed > 0) {
        int sleep_duration = sleep_completed - sleep_started;
        printf("✅ Thread woke up at quantum %d (slept %d quantums)\n", 
               sleep_completed, sleep_duration);
        
        if (sleep_duration >= 2) {
            printf("✅ Sleep duration correct!\n");
        } else {
            printf("❌ Sleep duration too short! Expected >= 2, got %d\n", sleep_duration);
        }
    } else {
        printf("❌ Thread never woke up!\n");
    }
    
    printf("📋 Step 8: Wait for thread completion\n");
    wait_iterations = 0;
    while (thread_completed == 0 && wait_iterations < 20) {
        printf("   - Waiting for completion... iteration %d\n", wait_iterations);
        for (volatile long i = 0; i < 100000000; i++);
        wait_iterations++;
    }
    
    if (thread_completed) {
        printf("✅ Thread completed successfully\n");
    } else {
        printf("❌ Thread never completed\n");
    }
    
    printf("📋 Final Summary:\n");
    printf("   - Thread started: %s\n", thread_started ? "YES" : "NO");
    printf("   - Sleep started: %s (quantum %d)\n", sleep_started ? "YES" : "NO", sleep_started);
    printf("   - Sleep completed: %s (quantum %d)\n", sleep_completed ? "YES" : "NO", sleep_completed);
    printf("   - Thread completed: %s\n", thread_completed ? "YES" : "NO");
    printf("   - Final quantums: %d\n", uthread_get_total_quantums());
    
    printf("🏁 Test complete\n");
    uthread_terminate(0);
    
    return 0;
}