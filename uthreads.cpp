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


typedef class  Thread
{
    int id;
    sigjmp_buf env;
    state state;
}thread;

std::queue<thread> threadQ;


int uthread_init(int quantum_usecs)
{
    if(quantum_usecs <= 0)
    {
        return -1;
    }

    // Create a queue of integers


    Thread main_thread;
    main_thread.id = MAIN_THREAD_ID;
    main_thread.state = RUNNING;


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
