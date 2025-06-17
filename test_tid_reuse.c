#include "uthreads.h"

void busy_beaver() {
    while (true);
}

int main() {
    uthread_init(1);

    printf("Starting thread with id 1\n");

    if (uthread_spawn(busy_beaver) != 1) {
        printf("Error!\n");
        return 1;
    }
    
    uthread_terminate(1);

    printf("Terminating thread 1, expected reuse\n");

    if (uthread_spawn(busy_beaver) != 1) {
        printf("Error! Should have reused 1\n");
        return 1;
    }

    printf("Starting threads 2 and 3\n");
    uthread_spawn(busy_beaver);
    uthread_spawn(busy_beaver);

    printf("Terminating thread 2, expecting reuse\n");
    uthread_terminate(2);

    if (uthread_spawn(busy_beaver) != 2) {
        printf("Error! Should have reused 1\n");
        return 1;
    }

    if (uthread_spawn(busy_beaver) != 4) {
        printf("Error! wtf\n");
        return 1;
    }

    printf("Test passed successfully!\n");

    uthread_terminate(0);
}