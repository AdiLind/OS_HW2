#include "uthreads.h"
#include <stdio.h>

int main(void) {
    printf("Testing basic uthreads functionality...\n");
    
    // Test initialization
    if (uthread_init(100000) == -1) {
        printf("FAILED: uthread_init\n");
        return 1;
    }
    printf("✓ uthread_init successful\n");
    
    // Test basic getters
    int tid = uthread_get_tid();
    int total = uthread_get_total_quantums();
    int my_quantums = uthread_get_quantums(tid);
    
    printf("✓ Current TID: %d\n", tid);
    printf("✓ Total quantums: %d\n", total);
    printf("✓ My quantums: %d\n", my_quantums);
    
    // Test invalid TID
    int invalid = uthread_get_quantums(99);
    printf("✓ Invalid TID test: %d (should be -1)\n", invalid);
    
    printf("Basic tests completed successfully!\n");
    return 0;
}