#include "uthreads.h"
#include <stdio.h>

void simple_test_thread(void) {
    printf("Thread started!\n");
    uthread_terminate(uthread_get_tid());
}

int main(void) {
    printf("Testing timer with 10ms quantum...\n");
    
    if (uthread_init(10000) != 0) {  // 10ms
        printf("Init failed\n");
        return 1;
    }
    
    printf("Initial quantums: %d\n", uthread_get_total_quantums());
    
    // Just busy loop and watch quantums increment
    for (int i = 0; i < 100; i++) {
        printf("Iteration %d, quantums: %d\n", i, uthread_get_total_quantums());
        
        // CPU intensive work to trigger timer
        volatile long sum = 0;
        for (long j = 0; j < 50000000L; j++) {
            sum += j;
        }
        
        // Should see quantums increasing every ~10ms of CPU time
        if (i == 50 && uthread_get_total_quantums() == 1) {
            printf("ERROR: Timer not working!\n");
            break;
        }
    }
    
    uthread_terminate(0);
    return 0;
}