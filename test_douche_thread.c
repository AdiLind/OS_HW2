#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <stdatomic.h>
#include <stdint.h>
#include "uthreads.h"

static volatile unsigned long long bullied_count = 0;
static volatile unsigned long long nonbullied_count = 0;

static atomic_int test_done;
static void userland_sleep(int usecs)
{
    struct timespec start, current;
    clock_gettime(CLOCK_MONOTONIC, &start);

    uint64_t nanos = usecs * 1e3;

    while (1) {
        clock_gettime(CLOCK_MONOTONIC, &current);
        uint64_t elapsed = (current.tv_sec - start.tv_sec) * 1e9 +
                           (current.tv_nsec - start.tv_nsec);
        if (elapsed >= nanos)
            break;
    }
}

void douche_thread() {
    printf("Douche running!\n");
    userland_sleep(800000);
    uthread_block(1);
    printf("Douche was unblocked");
    uthread_terminate(2);
    uthread_terminate(3);
    atomic_store(&test_done, 1);
    uthread_terminate(uthread_get_tid());
}

void bullied_thread() {
    printf("Bullied running!\n");

    uthread_resume(1);
    while (true) {
        bullied_count++;
    }

    // not terminated because of design
}
void nonbullied_thread() {
    printf("Non-bullied running!\n");

    while (true) {
        nonbullied_count++;
    }

    // not terminated because of design
}

// main - 1 - 2 - 3
// 1 - 2 - 3 - main : 1 blocks itself
// 2 - 3 - main: 2 resumes 1
// 2 - 3 - main - 1



int main() {
    atomic_init(&test_done, 0);
    uthread_init(1000000);
    uthread_spawn(douche_thread);
    uthread_spawn(bullied_thread);
    uthread_spawn(nonbullied_thread);

    while(!atomic_load(&test_done));


    printf("%llu, %llu\n", bullied_count, nonbullied_count);
    uthread_terminate(0);
}