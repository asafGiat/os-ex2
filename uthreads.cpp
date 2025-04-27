#include "uthreads.h"
#include <iostream>
#include <queue>  // Include the queue header
#include <list>
#include <setjmp.h>

typedef enum state
{
    RUNNING, READY, BLOCKED
} state;


typedef struct thread
{
    int id;
    sigjmp_buf env;
    state state;
}thread;

std::queue<thread> threadQ;


int uthread_init(int quantum_usecs)
{
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
