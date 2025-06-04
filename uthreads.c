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

static void enqueue_ready(int tid) {
    if (ready_queue_count >= READY_QUEUE_SIZE) {
        fprintf(stderr, "thread library error: ready queue is full\n");
        return;
    }
    
    ready_queue[ready_queue_rear] = tid;
    ready_queue_rear = (ready_queue_rear + 1) % READY_QUEUE_SIZE; //make it circular
    ready_queue_count++;
}
static int dequeue_ready() {
    if (is_queue_empty()) {
        fprintf(stderr, "thread library error: ready queue is empty\n");
        return -1;  
    }
    
    int tid = ready_queue[ready_queue_front];
    ready_queue_front = (ready_queue_front + 1) % READY_QUEUE_SIZE;
    ready_queue_count--;
    return tid;
}

static bool is_queue_empty() {
    return ready_queue_count == 0;
}

/*Helper functions*/

static int find_unused_thread_slot() {
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

static void enter_critical_section() {
    if(-1 == sigprocmask(SIG_BLOCK, &signal_mask, NULL)){
        fprintf(stderr, "System error: failed to block signals, masking failed\n");
        exit(EXIT_FAILURE);
    }
}

static void exit_critical_section() {
    if(-1 == sigprocmask(SIG_UNBLOCK, &signal_mask, NULL)){
        fprintf(stderr, "System error: failed to unblock signals, masking failed\n");
        exit(EXIT_FAILURE);
    }
}

/* <---Context Switch--->*/
typedef unsigned long address_t;

// Thread Control Block (TCB) structure from the code example
#define JB_SP 6
#define JB_PC 7 

// translate_address function to adjust the stack pointer specifically for X86_64 architecture
static address_t translate_address(address_t addr) {
    address_t ret;
    asm volatile("xor %%fs:0x30, %0\n"
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
    
    // Manually set stack pointer and program counter
    threads_control_block[tid].env->__jmpbuf[JB_SP] = translate_address(sp);
    threads_control_block[tid].env->__jmpbuf[JB_PC] = translate_address(pc);
    
    // Clear saved signal mask
    sigemptyset(&threads_control_block[tid].env->__saved_mask);
}
