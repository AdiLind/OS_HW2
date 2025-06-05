#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "uthreads.h"
#include <signal.h>

/* Global Variables */
static thread_t threads_control_block[MAX_THREAD_NUM];  // Array of thread control blocks.
static char thread_stacks[MAX_THREAD_NUM][STACK_SIZE];  // Stacks for each thread
static int current_running_tid = -1;  
static int total_quantums = 0;  
static struct itimerval timer;  // Timer for quantum scheduling.
static sigset_t signal_mask;  // Signal the critical sections.


// implement queue for manage READY threads
#define READY_QUEUE_SIZE MAX_THREAD_NUM

static int ready_queue[READY_QUEUE_SIZE];
static int ready_queue_front = 0;
static int ready_queue_rear = 0;
static int ready_queue_count = 0;

// declare helper functions
static bool is_queue_empty(void);

static void enqueue_ready(int tid) {
    if (ready_queue_count >= READY_QUEUE_SIZE) {
        fprintf(stderr, "thread library error: ready queue is full\n");
        return;
    }
    
    ready_queue[ready_queue_rear] = tid;
    ready_queue_rear = (ready_queue_rear + 1) % READY_QUEUE_SIZE; //make it circular
    ready_queue_count++;
}
static int dequeue_ready(void) {
    if (is_queue_empty()) {
        fprintf(stderr, "thread library error: ready queue is empty\n");
        return -1;  
    }
    
    int tid = ready_queue[ready_queue_front];
    ready_queue_front = (ready_queue_front + 1) % READY_QUEUE_SIZE;
    ready_queue_count--;
    return tid;
}

static bool is_queue_empty(void) {
    return ready_queue_count == 0;
}

/* <--Helper functions--> */

static int find_unused_thread_slot(void) {
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        if (threads_control_block[i].state == THREAD_UNUSED) {
            return i;
        }
    }
    return -1;  
}

static thread_t* get_thread_by_tid(int tid) {
    if (tid < 0 || tid >= MAX_THREAD_NUM) {
        return NULL;  
    }

    if(threads_control_block[tid].state == THREAD_UNUSED) {
        return NULL;  
    }

    return &threads_control_block[tid];
}

static void enter_critical_section(void) {
    if(-1 == sigprocmask(SIG_BLOCK, &signal_mask, NULL)){
        fprintf(stderr, "System error: failed to block signals, masking failed\n");
        exit(EXIT_FAILURE);
    }
}

static void exit_critical_section(void) {
    if(-1 == sigprocmask(SIG_UNBLOCK, &signal_mask, NULL)){
        fprintf(stderr, "System error: failed to unblock signals, masking failed\n");
        exit(EXIT_FAILURE);
    }
}

void schedule_next(void){
    thread_t* current_thread = NULL;

    // catch the current running thread 
    if (current_running_tid >= 0) {
        current_thread = &threads_control_block[current_running_tid];
        
        // If is still RUNNING (preempted by timer) so we change it to READY
        if (current_thread->state == THREAD_RUNNING) {
            current_thread->state = THREAD_READY;
            enqueue_ready(current_running_tid);
        }
    }

    int next_tid = -1;
    //searching thread from the ready queue
    while(!is_queue_empty())
    {
        int candidate_tid = dequeue_ready();
        thread_t* candidate = &threads_control_block[candidate_tid];

        if(candidate->state == THREAD_READY)
        {
            next_tid = candidate_tid;
            break;
        }
    }

    if(-1 == next_tid && current_running_tid >=0)
    {
        thread_t* curr_thread = &threads_control_block[current_running_tid];
        if(curr_thread->state == THREAD_READY)
        {
            return; //we will continue with the same thread
        }
    }

    if(-1 == next_tid)
    {
        fprintf(stderr, "thread library error: no threads available to run\n");
        exit(1);
    }

    //if we reach to this section so we can make a context switch
    thread_t* next_thread = &threads_control_block[next_tid];
    context_switch(current_thread, next_thread);

}


void timer_handler(int signum) {
    total_quantums++;
    
    if(current_running_tid >= 0)
    {
        threads_control_block[current_running_tid].quantums++;
    }

    // check if there is some threads that need to wakeup
    for (int i=0; i < MAX_THREAD_NUM; i++)
    {
        if(threads_control_block[i].state == THREAD_BLOCKED &&
            threads_control_block[i].sleep_until >0 &&
            threads_control_block[i].sleep_until <= total_quantums)
        {
            threads_control_block[i].sleep_until =0; // wakeup the thread
            threads_control_block[i].state = THREAD_READY;
            enqueue_ready(i);
        }
    }
    
    schedule_next();

}

/* <---Context Switch--->*/
typedef unsigned long address_t;

// Thread Control Block (TCB) structure from the code example
#define JB_SP 6
#define JB_PC 7 

// translate_address function to adjust the stack pointer specifically for X86_64 architecture
static address_t translate_address(address_t addr) {
    address_t ret;
    __asm__ volatile("xor %%fs:0x30, %0\n"
                 "rol $0x11, %0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

void setup_thread(int tid, char *stack, thread_entry_point entry_point) {

    address_t sp = (address_t)stack + STACK_SIZE - sizeof(address_t); // top of the stack
    address_t pc = (address_t)entry_point;
    
    // save thread context into jump buffer
    sigsetjmp(threads_control_block[tid].env, 1);
    
    //set stack pointer and program counter
    threads_control_block[tid].env->__jmpbuf[JB_SP] = translate_address(sp);
    threads_control_block[tid].env->__jmpbuf[JB_PC] = translate_address(pc);
    
    // Clear saved signal mask
    sigemptyset(&threads_control_block[tid].env->__saved_mask);
}


void context_switch(thread_t *current, thread_t *next) {
    if (current != NULL) {
        // Save current thread's context
        int return_val = sigsetjmp(current->env, 1);

        if (return_val != 0) {
            // We returned from siglongjmp - thread is resuming
            return;
        }
    }
    
    // Update current running thread
    current_running_tid = next->tid;
    next->state = THREAD_RUNNING;
    
    // continue to next thread
    siglongjmp(next->env, 1);
}

/* start of api functions implemnetations */

int uthread_init(int quantum_usecs) {
    if (quantum_usecs <= 0) {
        fprintf(stderr, "thread library error: quantum must be positive\n");
        return -1;
    }
    
    // set all threads to unused state
    for (int i = 0; i < MAX_THREAD_NUM; i++) {
        threads_control_block[i].tid = i;
        threads_control_block[i].state = THREAD_UNUSED;
        threads_control_block[i].quantums = 0;
        threads_control_block[i].sleep_until = 0;
        threads_control_block[i].entry = NULL;
    }
    
    // set main thread (tid = 0)
    threads_control_block[0].state = THREAD_RUNNING;
    threads_control_block[0].quantums = 1;  // TODO - TOCHECK: we should start with quantum 1 (?) 
    current_running_tid = 0;
    total_quantums = 1;
    
    //queue
    ready_queue_front = 0;
    ready_queue_rear = 0;
    ready_queue_count = 0;
    
    // create an empty set of signals
    if (sigemptyset(&signal_mask) == -1) {
        fprintf(stderr, "system error: signal initialization failed\n");
        exit(1);
    }
    if (sigaddset(&signal_mask, SIGVTALRM) == -1) {
        fprintf(stderr, "system error: signal initialization failed\n");
        exit(1);
    }

    //set up signal handler for SIGVTALRM

    struct sigaction sa;
    sa.sa_handler = timer_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags =0;

    if(sigaction(SIGVTALRM, &sa, NULL) == -1)
    {
        fprintf(stderr, "system error: sigaction failed\n");
        exit(1);
    }

    // set the virtual time configuration (sec, micSec, interval)
    timer.it_value.tv_sec = 1; //sec
    timer.it_value.tv_usec = 0; //microsecond
    timer.it_interval.tv_usec = 1; // interval
    timer.it_interval.tv_usec = 0;

    if(setitimer(ITIMER_VIRTUAL, &timer, NULL) == -1)
    {
        fprintf(stderr, "system error: setitimer failed\n");
    }



    
    return 0;
}

int uthread_get_tid(void) {
    return current_running_tid;
}

int uthread_get_total_quantums(void) {
    return total_quantums;
}


int uthread_get_quantums(int tid) {
    thread_t* thread = get_thread_by_tid(tid);
    if (thread == NULL) {
        fprintf(stderr, "thread library error: invalid thread ID\n");
        return -1;
    }
    return thread->quantums;
}

int uthread_spawn(thread_entry_point entry_point)
{
    enter_critical_section();

    //validation 
    if(entry_point == NULL)
    {
        fprintf(stderr, "thread lib error: entry point is null");
        exit_critical_section();
        exit(1);
    }

    //find the first unused thread id
    int new_tid =find_unused_thread_slot();
    if(-1 == new_tid)
    {
        fprintf(stderr, "thread lib error: not found free slot for thread -> exceeded maximum num of threads");
        exit(1);
    }

    // set the new thread to his TCB
    threads_control_block[new_tid].state = THREAD_READY;
    threads_control_block[new_tid].quantums = 0;
    threads_control_block[new_tid].sleep_until = 0;
    threads_control_block[new_tid].entry = entry_point;

    //set the thread context
    setup_thread(new_tid, thread_stacks[new_tid], entry_point);
    enqueue_ready(new_tid);

    exit_critical_section();

    return new_tid;
}


int uthread_terminate(int tid)
{
    enter_critical_section();

    thread_t* thread_to_terminate = get_thread_by_tid(tid);
    if(thread_to_terminate == NULL)
    {
        fprintf(stderr, "thread lib error: invalid thread ID");
        exit_critical_section();
        exit(1);
    }

    thread_to_terminate->state = THREAD_TERMINATED;

    // if tid == 0 -> we terminate the main tread so we should kil the process
    if(0 == tid)
    {
        struct itimerval stop_timer = {{0,0}, {0,0}};
        setitimer(ITIMER_VIRTUAL, &stop_timer, NULL);
        
    }
}
