#include "uthreads.h"
#include <iostream>
#include <queue>  // Include the queue header
#include <memory>
#include <setjmp.h>
#include <signal.h>
#include <unordered_map>
#include <array>

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
    state state;
    // std::shared_ptr<char[]> stack;
    std::array<char, STACK_SIZE> stack;
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

    Thread *thread = new Thread();
    thread->id = (int)threadQ.size() + 1;
    thread->state = READY;

    // thread.stack = std::make_unique<char[]>(STACK_SIZE);
    thread->stack = {};
    address_t sp = (address_t) thread->stack.data() + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) entry_point;
    sigsetjmp(env[thread->id], 1);
    (env[thread->id]->__jmpbuf)[JB_SP] = translate_address(sp);
    (env[thread->id]->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&env[thread->id]->__saved_mask);

    threads[thread->id] = thread;
    threadQ.push(thread->id);
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
            Thread *thread = pair.second;

        }
        }
        exit(0);
    }


}
