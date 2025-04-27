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
public:
    int id;
    sigjmp_buf env;
    state state;

}Thread;

std::unordered_map<int, Thread*> threads;
std::pmr::deque<int> ready_queue;

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
