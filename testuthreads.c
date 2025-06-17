#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
#include <stdbool.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "uthreads.h"  // רק אחרי כל ההכללות



void thread_func1() {
    printf("Thread 1 started. Sleeping for 3 quantums...\n");
    uthread_sleep(3);
    printf("Thread 1 woke up!\n");
    uthread_terminate(uthread_get_tid());
}

void thread_func2() {
    printf("Thread 2 started. Blocking thread 3...\n");
    uthread_block(3);
    printf("Thread 2 done.\n");
    uthread_terminate(uthread_get_tid());
}

void thread_func3() {
    printf("Thread 3 started. Should be blocked by thread 2.\n");
    for (int i = 0; i < 5; ++i) {
        printf("Thread 3 running... iteration %d\n", i + 1);
        usleep(10000);
    }
    printf("ERROR: Thread 3 should have been blocked!\n");
    uthread_terminate(uthread_get_tid());
}

void thread_func4() {
    printf("Thread 4 started. Will resume thread 3 in 5 quantums.\n");
    uthread_sleep(5);
    printf("Thread 4 now resumes thread 3\n");
    uthread_resume(3);
    uthread_terminate(uthread_get_tid());
}

int main() {
    uthread_init(100000); // 100ms

    uthread_spawn(thread_func1);
    uthread_spawn(thread_func2);
    uthread_spawn(thread_func3);
    uthread_spawn(thread_func4);

    while (1); // keep main thread alive
    return 0;
}
