
#include "uthreads.h"
#include <signal.h>

/* <!---- Global Variables ---> */
static thread_t threads_control_block[MAX_THREAD_NUM];  // thread control blocks array
static char thread_stacks[MAX_THREAD_NUM][STACK_SIZE];  // stack for for each thread
static int current_running_tid = -1;  
static int total_quantums = 0;  
static struct itimerval timer;  // for quantum scheduling
static sigset_t signal_mask;  // signal the critical sections.
static volatile int in_critical_section = 0;


typedef enum {
    BLOCK_REASON_NONE = 0,      
    BLOCK_REASON_SLEEP,         
    BLOCK_REASON_USER_BLOCK,   
    BLOCK_REASON_BOTH          
} block_reason_t;

static block_reason_t thread_block_reason[MAX_THREAD_NUM];

// implement queue for manage READY threads
#define READY_QUEUE_SIZE MAX_THREAD_NUM

static int ready_queue[READY_QUEUE_SIZE];
static int ready_queue_front = 0;
static int ready_queue_rear = 0;
static int ready_queue_count = 0;

// declare helper functions
static bool is_queue_empty(void);
static void enqueue_ready(int tid);
static int dequeue_ready(void);
static int find_unused_thread_slot(void);
static thread_t* get_thread_by_tid(int tid);
static void enter_critical_section(void);
static void exit_critical_section(void);

/* <--queue functions--> */

static void enqueue_ready(int tid) {
    if (ready_queue_count >= READY_QUEUE_SIZE) {
        fprintf(stderr, "thread library error: ready queue is full\n");
        return;
    }
    
    ready_queue[ready_queue_rear] = tid;
    ready_queue_rear = (ready_queue_rear + 1) % READY_QUEUE_SIZE; //make the queue circular
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
        if (threads_control_block[i].state == THREAD_UNUSED ||
            threads_control_block[i].state == THREAD_TERMINATED) {
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

/* <---Critical Section Controller--->*/

static void enter_critical_section(void) {
    in_critical_section = 1;
    if(-1 == sigprocmask(SIG_BLOCK, &signal_mask, NULL)){
        fprintf(stderr, "system error: masking failed\n");
        exit(1);
    }
}

static void exit_critical_section(void) {
    in_critical_section = 0;
    if(-1 == sigprocmask(SIG_UNBLOCK, &signal_mask, NULL)){
        fprintf(stderr, "system error: masking failed\n");
        exit(1);
    }
}

/*  <---Thread Setup---> */

typedef unsigned long address_t;

//Thread Control Block (TCB) structure from the code example
#define JB_SP 6
#define JB_PC 7 

//translate_address function to adjust the stack pointer specifically for X86_64 architecture
static address_t translate_address(address_t addr) {
    address_t ret;
    __asm__ volatile("xor %%fs:0x30, %0\n"
                 "rol $0x11, %0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

void setup_thread(int tid, char *stack, thread_entry_point entry_point) {
    // validation
    if (tid < 0 || tid >= MAX_THREAD_NUM) {
        fprintf(stderr, "system error: invalid tid in setup_thread\n");
        exit(1);
    }

    if (stack == NULL) {
        fprintf(stderr, "system error: null stack in setup_thread\n");
        exit(1);
    }

    address_t sp = (address_t)stack + STACK_SIZE - sizeof(address_t); // top of the stack
    address_t pc = (address_t)entry_point;
    
    // save thread context into jump buffer
    sigsetjmp(threads_control_block[tid].env, 1);
    
    //set stack pointer and program counter
    threads_control_block[tid].env->__jmpbuf[JB_SP] = translate_address(sp);
    threads_control_block[tid].env->__jmpbuf[JB_PC] = translate_address(pc);
    
    //clear saved signal mask
    sigemptyset(&threads_control_block[tid].env->__saved_mask);
}

 /* <---- Scheduler ---> */

void schedule_next(void){
    thread_t* current_thread = NULL;

    // catch the current running thread 
    if (current_running_tid >= 0 && current_running_tid < MAX_THREAD_NUM) 
    {
        current_thread = &threads_control_block[current_running_tid];
        
        //if is still RUNNING (preempted by timer) so we change it to READY
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
        if(candidate_tid >=0 && candidate_tid < MAX_THREAD_NUM)
        {
            thread_t* candidate = &threads_control_block[candidate_tid];
            if(candidate->state == THREAD_READY)
            {
                next_tid = candidate_tid;
                break;
            }
        }
    }

    //If no READY thread found- so its means that no runnable threads - this is a serious error
    if (next_tid == -1) {
        fprintf(stderr, "thread library error: no runnable threads\n");
        exit(1);
    }

    //if we reach to this section so we can make a context switch
    thread_t* next_thread = &threads_control_block[next_tid];
    context_switch(current_thread, next_thread);
}

/*  <---Timer Handler---> */
void timer_handler(int signum) {
    (void)signum;  //just to remove the warning warning while compiling

    if (in_critical_section) {
        return;  // Ignore timer signals if in critical section the signal is blicked anyway
    }
    
    total_quantums++;
    
    if(current_running_tid >= 0 && current_running_tid < MAX_THREAD_NUM)
    {
        threads_control_block[current_running_tid].quantums++;
    }

    // check if there is some threads that need to wakeup
    for (int i=0; i < MAX_THREAD_NUM; i++)
    {
        thread_t* thread = &threads_control_block[i];
        
        // check if thread should wakeup - he's still sleeping but sleep time has expired
        if(thread->sleep_until > 0 && thread->sleep_until <= total_quantums)
        {
            thread->sleep_until = 0; // Clear sleep timer
            
            //move sleeping thread to READY only and check if he ddidnt blocked by user
            if(thread->state == THREAD_BLOCKED)
            {
                if(thread_block_reason[i] == BLOCK_REASON_SLEEP) {
                    thread->state = THREAD_READY;
                    thread_block_reason[i] = BLOCK_REASON_NONE;
                    enqueue_ready(i);
                } else if(thread_block_reason[i] == BLOCK_REASON_BOTH) {
                    thread_block_reason[i] = BLOCK_REASON_USER_BLOCK;
                }
                
            }
        }
    }
    
    schedule_next();
}

/* <---Context Switch---> */

void context_switch(thread_t *current, thread_t *next) {
    // validate the next thread
    if (next == NULL || next->state == THREAD_TERMINATED || next->state == THREAD_UNUSED) {
        fprintf(stderr, "thread library error: invalid next thread in context_switch\n");
        exit(1);
    }
    
    if (current != NULL && current->state != THREAD_TERMINATED) 
    {
        // Save current thread's context by sigsetjmp if its success its will return 0 and nonzero if we return from siglongjmp
        if (sigsetjmp(current->env, 1) != 0) {
            current_running_tid = current->tid;
            return;
        }
    }
    
    
    current_running_tid = next->tid;
    next->state = THREAD_RUNNING;
    
    // continue to next thread
    siglongjmp(next->env, 1);
    fprintf(stderr, "system error: siglongjmp failed\n");
    exit(1);
}

/* <==== API FUNCTIONS ====>*/

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
        thread_block_reason[i] = BLOCK_REASON_NONE;
    }
    
    // set main thread (tid = 0)
    threads_control_block[0].state = THREAD_RUNNING;
    threads_control_block[0].quantums = 1;
    current_running_tid = 0;
    total_quantums = 1;

    sigsetjmp(threads_control_block[0].env, 1); //save the main thread context
    
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
    timer.it_value.tv_sec = quantum_usecs / 1000000; //convert to seconds
    timer.it_value.tv_usec = quantum_usecs % 1000000; // convert to microseconds
    timer.it_interval.tv_sec = quantum_usecs / 1000000;
    timer.it_interval.tv_usec = quantum_usecs % 1000000;

    if(setitimer(ITIMER_VIRTUAL, &timer, NULL) == -1)
    {
        fprintf(stderr, "system error: setitimer failed\n");
        exit(1);
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
        fprintf(stderr, "thread library error: entry point is null\n");
        exit_critical_section();
        return -1;
    }

    //find the first unused thread id
    int new_tid =find_unused_thread_slot();
    if(-1 == new_tid)
    {
        fprintf(stderr, "thread library error: exceeded maximum number of threads\n");
        exit_critical_section();
        return -1;
    }

    // set the new thread to his TCB
    threads_control_block[new_tid].state = THREAD_READY;
    threads_control_block[new_tid].quantums = 0;
    threads_control_block[new_tid].sleep_until = 0;
    threads_control_block[new_tid].entry = entry_point;
    thread_block_reason[new_tid] = BLOCK_REASON_NONE;

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
        fprintf(stderr, "thread library error: invalid thread ID\n");
        exit_critical_section();
        return -1;
    }

    thread_to_terminate->state = THREAD_TERMINATED;
    thread_block_reason[tid] = BLOCK_REASON_NONE;

    // if tid == 0 -> we terminate the main tread so we should kil the process
    if(0 == tid)
    {
        // stop the timer
        struct itimerval stop_timer ;
        stop_timer.it_interval.tv_sec = 0;
        stop_timer.it_interval.tv_usec = 0;
        stop_timer.it_value.tv_sec = 0;
        stop_timer.it_value.tv_usec = 0;

        setitimer(ITIMER_VIRTUAL, &stop_timer, NULL);

        //clean all the threads
        for(int i = 0; i < MAX_THREAD_NUM; i++)
        {
                if (threads_control_block[i].state != THREAD_UNUSED && 
                    threads_control_block[i].state != THREAD_TERMINATED) 
                {
                    //mark threads as terminated
                    threads_control_block[i].state = THREAD_TERMINATED;
                }
        }

        exit(0);
    }

    // if we terminate the current running thread we should switch to the next thread
     if (tid == current_running_tid) {
        exit_critical_section();
        schedule_next();

        //if we reach here, something went wrong :(
        fprintf(stderr, "thread library error: failed to switch from terminated thread\n");
        exit(1);
    }
    
    //If terminating a different thread, just mark it as terminated
    exit_critical_section();
    return 0;
}

int uthread_block(int tid) {
    enter_critical_section();

    if(tid == 0)
    {
        fprintf(stderr, "thread library error: cannot block the main thread\n");
        exit_critical_section();
        return -1;
    }

    thread_t* thread_to_block = get_thread_by_tid(tid);
    if(thread_to_block == NULL) {
        fprintf(stderr, "thread library error: invalid thread ID\n");
        exit_critical_section();
        return -1;
    }

    if(thread_to_block->state == THREAD_BLOCKED) {
        // if the thread is alredy blocked we just need to update the status to be both
        if(thread_block_reason[tid] == BLOCK_REASON_SLEEP) {
            thread_block_reason[tid] = BLOCK_REASON_BOTH; // Sleep + User block
        }
        exit_critical_section();
        return 0; 
    }
    
    //block the thread and update the reason
    if (thread_to_block->state == THREAD_RUNNING) {
        thread_to_block->state = THREAD_BLOCKED;
        if(thread_block_reason[tid] == BLOCK_REASON_SLEEP) {
            thread_block_reason[tid] = BLOCK_REASON_BOTH; // Sleep + User block
        } else {
            thread_block_reason[tid] = BLOCK_REASON_USER_BLOCK; 
        }
        
        if (tid == current_running_tid) {
            exit_critical_section();
            schedule_next();
            return 0;
        }
    } else if (thread_to_block->state == THREAD_READY) {
        thread_to_block->state = THREAD_BLOCKED;
        
        if(thread_block_reason[tid] == BLOCK_REASON_SLEEP) {
            thread_block_reason[tid] = BLOCK_REASON_BOTH; // Sleep + User block
        } else {
            thread_block_reason[tid] = BLOCK_REASON_USER_BLOCK; 
        }
    } else if (thread_to_block->state == THREAD_BLOCKED) {
        if(thread_block_reason[tid] == BLOCK_REASON_SLEEP) {
            thread_block_reason[tid] = BLOCK_REASON_BOTH; // Sleep + User block
        }
    } else {
        fprintf(stderr, "thread library error: cannot block terminated or unused thread\n");
        exit_critical_section();
        return -1;
    }
    exit_critical_section();
    return 0;
}

int uthread_resume(int tid)
{
    enter_critical_section();
    //get the thread by tid
    thread_t* thread_to_resume = get_thread_by_tid(tid);
    if(thread_to_resume == NULL) {
        fprintf(stderr, "thread library error: invalid thread ID\n");
        exit_critical_section();
        return -1;
    }

    switch(thread_to_resume->state)
    {
        case THREAD_BLOCKED:
            if(thread_block_reason[tid] == BLOCK_REASON_USER_BLOCK) {
                thread_to_resume->state = THREAD_READY;
                thread_block_reason[tid] = BLOCK_REASON_NONE;
                enqueue_ready(tid);
            } else if(thread_block_reason[tid] == BLOCK_REASON_BOTH) {
                thread_block_reason[tid] = BLOCK_REASON_SLEEP;
            }
            
            break;
            
        case THREAD_RUNNING:
        case THREAD_READY:
            
            thread_block_reason[tid] = BLOCK_REASON_NONE;
            break;
            
        case THREAD_TERMINATED:
        case THREAD_UNUSED:
            //if the thread is terminated or unused threads we cannot resume it -> error
            fprintf(stderr, "thread library error: cannot resume terminated or unused thread\n");
            exit_critical_section();
            return -1;
            
        default:
            break;
    }
    
    exit_critical_section();
    return 0;
}

int uthread_sleep(int num_quantums) {
    enter_critical_section();
    
    int tid = current_running_tid;
    //validatuion
    if (num_quantums <= 0) 
    {
        fprintf(stderr, "thread library error: sleep must be positive\n");
        exit_critical_section();
        return -1;
    }


    /* i change this section because the discussion in Piazza, We dont delete this section in order to be able to appeal and complain */
    // if (num_quantums <= 0) {
    //     fprintf(stderr, "thread library error: sleep must be positive\n");
    //     exit_critical_section();
    //     return -1;
    // }

    //the main thread cannot sleep
    if (tid == 0) {
        fprintf(stderr, "thread library error: main thread cannot sleep\n");
        exit_critical_section();
        return -1;
    }

    thread_t* current_thread = &threads_control_block[tid];
    
    //set sleep duration- we sleep until: current + num_quantums + 1
    current_thread->sleep_until = total_quantums + num_quantums + 1;
    current_thread->state = THREAD_BLOCKED;
    
    if(thread_block_reason[tid] == BLOCK_REASON_USER_BLOCK) {
        thread_block_reason[tid] = BLOCK_REASON_BOTH; // User block + Sleep
    } else {
        thread_block_reason[tid] = BLOCK_REASON_SLEEP; 
    }
    
    exit_critical_section();
    schedule_next();
    
    return 0; //this line should never be reached
}