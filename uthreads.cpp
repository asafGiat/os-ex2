#include "uthreads.h"
#include <iostream>
#include <queue>  // Include the queue header
#include <list>
#include <setjmp.h>
#include <unordered_map>

#define MAIN_THREAD_ID 0

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

int quantum_length;
std::unordered_map<int, Thread*> threads;
std::queue<int> threadQ;
Thread* running_thread = nullptr;


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

int uthread_spawn(thread_entry_point entry_point)
{
    if (entry_point == nullptr)
    {
        fprintf(stderr, "thread library error: The entry point should not be null.\n");
    }
    thread thread;

}


int uthread_get_tid()
{
    return running_thread->id;
}
