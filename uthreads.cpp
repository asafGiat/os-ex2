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

int current_thread_id = -1;
int quantum_usecs = 0;

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
    // std::shared_ptr<char[]> stack;
    std::array<char, STACK_SIZE> stack;
};

std::unordered_map<int, Thread *> threads;
std::queue<int> threadQ;

void jump_to_thread(int tid)
{
    current_thread_id = tid;
    Thread *cur_thread = threads.at(tid);
    siglongjmp(cur_thread->env, 1);
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

    current_thread_id = MAIN_THREAD_ID;
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
    thread->thread_state = READY;

    // thread.stack = std::make_unique<char[]>(STACK_SIZE);
    thread->stack = {};
    address_t sp = (address_t) thread->stack.data() + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) entry_point;
    sigsetjmp(thread->env, 1);
    (thread->env->__jmpbuf)[JB_SP] = translate_address(sp);
    (thread->env->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&thread->env->__saved_mask);

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
            const Thread *thread = pair.second;
            if (thread->id != MAIN_THREAD_ID)
            {
                delete thread;
            }

        }

        while (!threadQ.empty())
        {
            threadQ.pop();
        }

        threads.clear();
        exit(0);
    }
    // Terminate threads whose id != 0

    // remove from queue
    std::queue<int> tempQ;
    while(!threadQ.empty())
    {
        int frontId = threadQ.front();
        threadQ.pop();
        if (frontId != tid)
        {
            tempQ.push(frontId);
        }
    }
    threadQ = tempQ;

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
