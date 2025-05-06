#include "uthreads.h"
#include <iostream>
#include <queue>  // Include the queue header
#include <memory>
#include <setjmp.h>
#include <signal.h>
#include <unordered_map>
#include <array>
#include <sys/time.h>

#define MAIN_THREAD_ID 0

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7


/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
        "rol    $0x11,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

typedef enum state
{
    RUNNING, READY, BLOCKED
} state;




struct Thread
{
    int id;
    sigjmp_buf env;
    state thread_state;
    int quantums_run;
    // std::shared_ptr<char[]> stack;
    std::array<char, STACK_SIZE> stack;
};

std::unordered_map<int, Thread *> threads;
std::deque<int> ready_queue; //deque aloowed more operations than queue
int current_thread_id = -1;
int quantum_usecs = 0;
int total_quantums = 0;
int sleep_quantums_left = 0; // for uthread_sleep()
struct itimerval timer; // Timer
void scheduler(int sig); //function that handles quantum end (implimented at the end)

// Helper: set timer
void set_timer()
{
    timer.it_value.tv_sec = quantum_usecs / 1000000;
    timer.it_value.tv_usec = quantum_usecs % 1000000;
    timer.it_interval.tv_sec = timer.it_value.tv_sec;
    timer.it_interval.tv_usec = timer.it_value.tv_usec;
    if (setitimer(ITIMER_VIRTUAL, &timer, nullptr) < 0)
    {
        std::cerr << "system error: setitimer failed" << std::endl;
        exit(1);
    }
}

void jump_to_thread(int tid)
{
    current_thread_id = tid;
    Thread *cur_thread = threads.at(tid);
    siglongjmp(cur_thread->env, 1);
    //@michael what about state to running and the acttual resuming?
}

int uthread_init(int quantum_usecs)
{
    if(quantum_usecs <= 0)
    {
        return -1;
    }
    ::quantum_usecs = quantum_usecs;

    Thread main_thread{};
    main_thread.id = MAIN_THREAD_ID;
    main_thread.thread_state = RUNNING;
    main_thread.quantums_run=0;

    current_thread_id = MAIN_THREAD_ID;
    total_quantums = 1;

    struct sigaction sa = {};
    sa.sa_handler = scheduler; //set the function to be called after each quantum
    if (sigaction(SIGVTALRM, &sa, nullptr) < 0)
    {
        std::cerr << "system error: sigaction failed" << std::endl;
        exit(1);
    }
    set_timer();
    return 0;
}

// Helper: find smallest available tid
int find_available_id()
{
    while (true)
    {
        int i = rand();
        if (threads.find(i) == threads.end())
        {
            return i;
        }
    }
}


int uthread_spawn(thread_entry_point entry_point)
{
    if (entry_point == nullptr)
    {
        fprintf(stderr, "thread library error: The entry point should not be null.\n");
        return -1;
    }

    if (ready_queue.size() + 1 > MAX_THREAD_NUM) {
        fprintf(stderr, "thread library error: The number of threads exceeds the limit.\n");
        return -1;
    }

    Thread *thread = new Thread();
    thread->id = find_available_id();
    thread->thread_state = READY;
    thread->quantums_run = 0;

    // thread.stack = std::make_unique<char[]>(STACK_SIZE);
    thread->stack = {};
    address_t sp = (address_t) thread->stack.data() + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) entry_point;
    sigsetjmp(thread->env, 1);
    (thread->env->__jmpbuf)[JB_SP] = translate_address(sp);
    (thread->env->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&thread->env->__saved_mask);

    threads[thread->id] = thread;
    ready_queue.push_back(thread->id);
    return thread->id;
}

int uthread_terminate(int tid)
{
    if (threads.count(tid) == 0)
    {
        fprintf(stderr, "thread library error: The thread with ID %d does not exist.\n", tid);
        return -1;
    }

    if (tid == MAIN_THREAD_ID)
    {
        // Terminate the entire process
        for (const auto &pair : threads)
        {
            const Thread *thread = pair.second;
            if (thread->id != MAIN_THREAD_ID)
            {
                delete thread;
            }

        }

        while (!ready_queue.empty())
        {
            ready_queue.pop_front();
        }

        threads.clear();
        exit(0);
    }
    // Terminate threads whose id != 0

    // remove from queue
    std::deque<int> tempQ;
    while(!ready_queue.empty())
    {
        int frontId = ready_queue.front();
        ready_queue.pop_front();
        if (frontId != tid)
        {
            tempQ.push_back(frontId);
        }
    }
    ready_queue = tempQ;

    threads.erase(tid);

    Thread *thread = threads[tid];

    // self termination, jump to main thread (id == 0)
    if (tid == current_thread_id)
    {
        jump_to_thread(0);
    }
    delete thread;

    // if it wasn't self termination, just return
    return 0;

}

int uthread_block(int tid)
{
    if(tid == MAIN_THREAD_ID)
    {
        fprintf(stderr, "thread library error: Can not block the main thread\n", tid);
        return -1;
    }
    if(threads.find(tid) == threads.end())
    {
        fprintf(stderr, "thread library error: Thread not found\n", tid);
        return -1;
    }
    if(threads[tid]->thread_state==READY)
    {
        //remove from queue
        //if(ready_queue.erase(find(ready_queue.begin(), ready_queue.end(), threads[tid]), ready_queue.end());
    }
    if(current_thread_id==tid)
    {
        if(threads[tid]->thread_state!=RUNNING)
        {
            //throw std::exception("running thread not in running state");
        }
        //need to do scedualing
    }
    threads[tid]->thread_state=BLOCKED;
    return 0;
}

int uthread_resume(int tid)
{
    if(threads.find(tid) == threads.end())
    {
        fprintf(stderr, "thread library error: Thread not found\n", tid);
        return -1;
    }
    if(threads[tid]->thread_state!=BLOCKED)
    {
        return 0;
    }
    //if reached here, the thred is a valid blocked thread and we need to handle the resume
    threads[tid]->thread_state=READY;
    //todo: make sure the thread isnt in the readyq, and if it is, throw exception
    ready_queue.push_back(tid);
    return 0;
}

int uthread_sleep(int num_quantums)
{
    if (num_quantums <= 0)
    {
        return -1;
    }

    if (current_thread_id == MAIN_THREAD_ID)
    {
        fprintf(stderr, "thread library error: The main thread cannot sleep.\n");
        return -1;
    }

}

int uthread_get_tid()
{
    return current_thread_id;
}

int uthread_get_total_quantums()
{
    return total_quantums;
}

int uthread_get_quantums(int tid)
{
    if(threads.find(tid) == threads.end())
    {
        fprintf(stderr, "thread library error: Thread not found\n", tid);
        return -1;
    }
    return threads[tid]->quantums_run;
}

void scheduler(int sig)
{
    /*
     * here we need to take care of reapitingly taking care of thresds.
     * handle sleeping threads - minus the sleeping time and set to ready if its 0
     * switch the running thread
     */
}

