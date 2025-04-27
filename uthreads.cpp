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


int uthread_init(int quantum_usecs)
{
    return 0;
}
