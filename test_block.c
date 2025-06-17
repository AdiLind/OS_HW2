#include "uthreads.h"
#include <stdatomic.h>

static atomic_int blocked_should_run;
static atomic_int blocked_ran;
static atomic_int test_done;
void blocked() {
    printf("Started blocked logic!\n");

    while (!atomic_load(&blocked_should_run));

    atomic_store(&blocked_ran, 1);

    uthread_terminate(uthread_get_tid());
}

void blocker() {
    printf("Started blocker logic!\n");

    uthread_block(1);
    atomic_store(&blocked_should_run, 1);
    uthread_sleep(10);

    if (atomic_load(&blocked_ran)) {
        printf("Error! Blocked shouldn't have ran!\n");
        exit(1);
    }

    uthread_resume(1);
    uthread_sleep(10);

    if (!atomic_load(&blocked_ran)) {
        printf("Error! Blocked should have ran!\n");
        exit(1);
    }

    atomic_store(&test_done, 1);
    uthread_terminate(uthread_get_tid());
}

int main() {
    uthread_init(100000);

    if (uthread_block(0) == 0) { 
        printf("Shouldn't be able to block main thread!\n");
        return 1;
    }

    if (uthread_block(1) == 0) {
        printf("Shouldn't be able to block a non-existent thread!\n");
        return 1;
    }

    if (uthread_resume(1) == 0) {
        printf("Shouldn't be able to resume a non-existent thread!\n");
        return 1;
    }

    uthread_spawn(blocked); // Tid = 1
    uthread_spawn(blocker); // Tid = 2

    while (!atomic_load(&test_done));

    printf("Test done successfully!\n");
    uthread_terminate(0);
}