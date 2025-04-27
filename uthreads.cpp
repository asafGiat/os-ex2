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
};

std::unordered_map<int, Thread *> threads;
std::queue<int> threadQ;

sigjmp_buf env[2];

int uthread_init(int quantum_usecs)
{
    if(quantum_usecs <= 0)
    {
        return -1;
    }

    // Create a queue of integers


    Thread main_thread{};
    main_thread.id = MAIN_THREAD_ID;
    main_thread.state = RUNNING;


    return 0;
}

int uthread_spawn(thread_entry_point entry_point)
{
    if (entry_point == nullptr)
    {
        fprintf(stderr, "thread library error: The entry point should not be null.\n");
        return -1;
    }

    if (threadQ.size() + 1 > MAX_THREAD_NUM) {
        fprintf(stderr, "thread library error: The number of threads exceeds the limit.\n");
        return -1;
    }

    Thread thread;
    thread.id = threadQ.size();
    thread.state = READY;

    address_t sp = (address_t) stack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) entry_point;
    sigsetjmp(env[thread.id], 1);
    (env[thread.id]->__jmpbuf)[JB_SP] = translate_address(sp);
    (env[thread.id]->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&env[thread.id]->__saved_mask);
}
