#include "uthreads.h"
#include <iostream>
#include <queue>  // Include the queue header
#include <list>
#include <setjmp.h>
#include <signal.h>
#include <unordered_map>

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


class  Thread
{
public:
    int id;
    sigjmp_buf env;
    state state;
    thread_entry_point entry_point;
};


/*======global variables=====*/
int quantum_length;
std::unordered_map<int, Thread*> threads;
std::queue<int> readyQ;
Thread* running_thread = nullptr;

sigjmp_buf env[2];



int uthread_init(int quantum_usecs)
{
    if(quantum_usecs <= 0)
    {
        return -1;
    }
    quantum_length = quantum_usecs;
    Thread* main_thread = new Thread();
    main_thread->id = MAIN_THREAD_ID;
    main_thread->state = RUNNING;
    threads[main_thread->id] = main_thread;

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

    if (readyQ.size() + 1 > MAX_THREAD_NUM) {
        fprintf(stderr, "thread library error: The number of threads exceeds the limit.\n");
        return -1;
    }

    Thread* thread = new Thread;
    thread->id = find_available_id();
    thread->state = READY;

    address_t sp = (address_t) stack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) entry_point;
    sigsetjmp(env[thread->id], 1);
    (env[thread->id]->__jmpbuf)[JB_SP] = translate_address(sp);
    (env[thread->id]->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&env[thread->id]->__saved_mask);

    readyQ.push(thread->id);
}

int uthread_terminate(int tid)
{
    if (threads.find(tid) == threads.end())
    {
        return -1;
    }
    if (tid == 0)
    {
        // Terminate process
        for (auto &pair : threads)
        {
            delete pair.second;
        }
        threads.clear();
        exit(0);
    }
    delete threads[tid];
    threads.erase(tid);

    return 0;
}

int uthread_get_tid()
{
    return running_thread->id;
}


int uthread_block(int tid)
{
    if (threads.find(tid) == threads.end())
    {
        return -1;
    }

}
