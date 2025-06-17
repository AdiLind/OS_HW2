#include "uthreads.h"
#include <stdatomic.h>

static atomic_int test_done;

void sleeper_thread() {
    int current_time = uthread_get_total_quantums();
    int my_current_time = uthread_get_quantums(uthread_get_tid());
    uthread_sleep(10);

    int delta = uthread_get_total_quantums() - current_time;
    if (delta < 10) {
        printf("Error! Should have been idle for at least 10 quantuns, but only %d quantums have passed!\n",
            delta);
        exit(1);
    }

    if (uthread_get_quantums(uthread_get_tid()) - my_current_time > 1) {
        printf("Error! Quantums for current thread shouldn't have changed by more than 1 during sleep!\n");
        exit(1);
    }

    atomic_store(&test_done, 1);

    uthread_terminate(uthread_get_tid());
}

int main() {
    atomic_init(&test_done, 0);

    uthread_init(100000);

    if (uthread_sleep(10) != -1) {
        printf("Error! Main thread should not be able to sleep!\n");
        return 1;
    }

    uthread_spawn(sleeper_thread);

    while(!atomic_load(&test_done));
    printf("Test passed!\n");

    uthread_terminate(0);
}