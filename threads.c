// Creates a new thread to run process start_routine. Initializes threas subsystem uppon first call. 
#include "ec440threads.h"
#include <setjmp.h>
#include <stdlib.h>
#include <signal.h>
#include <stdio.h> 
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

/**********************************************/

#define JB_RBX 0
#define JB_RBP 1
#define JB_R12 2
#define JB_R13 3
#define JB_R14 4
#define JB_R15 5
#define JB_RSP 6
#define JB_PC 7

#define _XOPEN_SOURCE 500

#define STACK_SIZE 32767

#define max_threads 128 //max number of threads

#define uquanta 50000 // quanta: 50 ms (50,000 micro s) for each thread executuion time 

static int first_time = 1; // determines if helper is called 

pthread_t currentthread = 0; //current thread index

static int mainthread = 0; // to see if we are in main process or thread (1 if yes, 0 if no)

static int numthreads = 1; //number of threads running 

struct sigaction sighandler; 

struct TCB{ 
    int status; //0 = exited, 1 = running, 2 = ready, 3 = blocked, 4 = unused
    void* sp; //stack pointer 
    jmp_buf regs; // execution context. 
    pthread_t threadid; //thread ID 
}; 

static struct TCB TCBlist[ max_threads ]; //List of TCBs. 

/*******************pthread_create***************************/
int pthread_create(
    pthread_t *thread,
    const pthread_attr_t *attr, 
    void *(*start_routine)(void*), 
    void *arg)
{ 
    if (first_time){
        pthread_create_helper(); 
        first_time = 0;
        TCBlist[currentthread].status = 1; //running
        mainthread = setjmp(TCBlist[currentthread].regs); //return 0 on set, returns nonzero integer on return from longjmp. 
        //TCBlist[0].regs has main context. 
        TCBlist[currentthread].threadid = currentthread;
    } //initilizes thread subsystem. 

    if (numthreads == max_threads){
        perror("Did not thread. Max number of threads reached.");
        return -1;
    } //if max number of threads reach, returns -1 and does not make a new thread.

    pthread_t newthread = numthreads; 

    while(TCBlist[newthread].status != 4){
        newthread++; 
    }

    *thread = TCBlist[newthread].threadid;

    setjmp(TCBlist[newthread].regs);

    TCBlist[newthread].regs->__jmpbuf[JB_PC] = ptr_mangle((unsigned long int)start_thunk); //sets program counter. 
    //starthunk moves value of R13 to RDI. Then jumps to address of R12. so R13 should be arg, and R12 should be function. 
    TCBlist[newthread].regs->__jmpbuf[JB_R13] = (long) arg; 

    TCBlist[newthread].regs->__jmpbuf[JB_R12]  = (unsigned long int) start_routine; 

    TCBlist[newthread].sp = malloc(STACK_SIZE); //malloc points to bottom of stack. 

    //Want sp to point to top of new stack. (stacks grow down for us).
    void* bottom = TCBlist[newthread].sp + STACK_SIZE; 
    void* stack = bottom - sizeof(&pthread_exit); 
    void (*temp)(void*) = (void*) &pthread_exit; //pthread exit will be called when start_routine returns. 
    stack = memcpy(stack,&temp, sizeof(temp)); 

    TCBlist[newthread].regs->__jmpbuf[JB_RSP] = ptr_mangle((unsigned long int)stack);

    TCBlist[newthread].status = 2; 
    //TCBlist[newthread].threadid = newthread; 

    numthreads++; 

    schedule(SIGALRM); 

 return 0; 
} 

/**********************pthread_create_helper************************/
void pthread_create_helper(){
   // This function is called only when pthread_create is called for the first time.
   // First, it initializes the thread array, all relevant data structures, and a stack space.  
   // It sets up a stack space using malloc for the threads

    int i; 

    for (i=0 ; i < max_threads ; ++i) {
        TCBlist[ i ].status = 4; //0 = exited, 1 = running, 2 = ready, 3 = blocked, 4 = unused
        TCBlist[ i ].sp = 0;
        TCBlist[i].threadid = i; 
    }

    //respond to SIGALRM. -> SIGALRM calls schedule. 
    sighandler.sa_handler = schedule; // set handler function
    sigemptyset(&sighandler.sa_mask);
    sighandler.sa_flags = SA_NODEFER; 

    if(signal(SIGALRM,schedule)==SIG_ERR){
		perror("failed to set signal handler");
		exit(1);
	};

    useconds_t timer = uquanta;
    ualarm(timer,timer); 

} 

/**********************schedule************************/
void schedule(int signal){ 
    if(TCBlist[currentthread].status ==1) /*running)*/{
    TCBlist[currentthread].status = 2; /*ready*/
    }

    if(numthreads == 0){
        exit(0); 
    } //if no more threads, exit. 

    // Find Next Ready Thread 
    pthread_t FindID = currentthread; 
    FindID++; 

    while(1){
        if(FindID == max_threads+1){
            FindID = 0; 
        }
        else if (numthreads!= 0){

            if(TCBlist[FindID].status == 2){
            break; 
            }

            FindID++; 
        }

    }

    // Saves current thread context 
    int setup = 0; 
    if( TCBlist[currentthread].status != 0){
        setup = setjmp(TCBlist[currentthread].regs);
    }

    //Runs Thread
    if(!setup){
        currentthread = FindID; 
        TCBlist[currentthread].status = 1; 
        longjmp(TCBlist[currentthread].regs,1);
    } 
}

/*******************zthread_self***************************/
pthread_t pthread_self(void){
    return TCBlist[currentthread].threadid;
}

/******************pthread_exit****************************/
void pthread_exit(void *value_ptr){ 

    numthreads--; 

    free(TCBlist[currentthread].sp);

    TCBlist[currentthread].status = 4; 

    schedule(SIGALRM);

    exit(0);
}

////////////////////////// Synchronization //////////////////////

///////// Mutex Functions ///////////////
int pthread_mutex_init(pthread_mutex_t *restrict mutex, const pthread_mutexattr_t *restrict attr){
    // Initializes a mutex. attr is always NULL. Returns 0. 
    
    return 0; 
}

int pthread_mutex_destroy(pthread_mutex_t *mutex){
    //Destroys referenced mutex. Return 0.

    return 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
    /*Locks referenced mutex. If not yet locked, current thread attains lock and proceeds.
     If already locked, thread blocks until mutex is available. 
     If multiple threads are waitng for mutex, order they attain mutex is undefined. 
     Return 0 on success or error code else. */

    return 0; 
}

int pthread_mutex_unlock(pthread_mutex_t *mutex){
    /*Unlocks referenced mutex. If another thread is waiting for mutex, its awoken and run.
    Note: When thread awoken, it will finish aquiring the lock. 
    Return 0 on success or error code else. */

    return 0; 
}

///////// Barrier Functions ///////////////

int pthread_barrier_init(pthread_barrier_t *restrict barrier, const pthread_barrierattr_t *restrict attr, unsigned count){
    /*Initializes given barrier. attr always NULL. Count specifies how many 
    threads must enter barrier before any threads can exit barrier. 
    Return 0 on success. Error (EINVAL) if count = 0. */
    return 0; 
}

int pthread_barrier_destroy(pthread_barrier_t *barrier){
    /*Destroys referenced barrier. Return 0 on success.*/
    return 0; 
}

int pthread_barrier_wait(pthread_barrier_t *barrier){
    /* Enteres refrerenced barrier. Calling thread will not proceed until 
    required number of threads (from count in init) have entered the barrier*/
    return 0;
}

///// Internal Functions //////
static void lock(){
// Disable timer that calls schedule routine.
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set,SIGALRM);
    sigprocmask(SIG_BLOCK, &set,NULL);
}

static void unlock(){
// Re-enable timer. 
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set,SIGALRM);
    sigprocmask(SIG_UNBLOCK, &set,NULL);
}