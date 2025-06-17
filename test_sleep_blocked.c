#include "uthreads.h"
#include <stdatomic.h>

static atomic_int sleeper_thread_done_sleeping;
static atomic_int test_done;

void sleeper_thread() {
    printf("Sleeper thread sleeping!\n");

    uthread_sleep(10);

    printf("Sleeper thread finished sleeping!\n");
    atomic_store(&sleeper_thread_done_sleeping, 1);

    uthread_terminate(uthread_get_tid());
}

void manager_thread() {
    int sleeper_tid = uthread_spawn(sleeper_thread);

    printf("Waiting for sleeping thread to begin sleeping!\n");
    uthread_sleep(3);
    
    if (atomic_load(&sleeper_thread_done_sleeping)) {
        printf("Error!\n");
        exit(1);
    }

    printf("Assuming that sleeper thread started sleeping, blocking it\n");
    uthread_block(sleeper_tid);

    uthread_sleep(10);

    if (atomic_load(&sleeper_thread_done_sleeping)) {
        printf("Error!\n");
        exit(1);
    }

    uthread_resume(sleeper_tid);
    uthread_sleep(10);
    
    if (!atomic_load(&sleeper_thread_done_sleeping)) {
        printf("Error! Sleeper thread should have woken up and finished!\n");
        exit(1);
    }

    atomic_store(&test_done, 1);

    uthread_terminate(uthread_get_tid());
}

int main() {
    uthread_init(100000);

    uthread_spawn(manager_thread);

    while (!atomic_load(&test_done));
    printf("Test done successfully!\n");
    uthread_terminate(0);
}