// uthreads.cpp
#include "uthreads.h"
#include <unordered_map>
#include <deque>
#include <csignal>
#include <sys/time.h>
#include <cstdlib>
#include <cstring>
#include <iostream>

using namespace std;

// Thread states
enum ThreadState
{
    RUNNING,
    READY,
    BLOCKED,
    SLEEPING
};

// Thread class
class Thread
{
public:
    int tid;
    ThreadState state;
    char stack[STACK_SIZE];
    thread_entry_point entry_point;
    int quantums_run = 0;
    sigjmp_buf env;
    int sleep_quantums_left = 0; // for uthread_sleep()

    Thread(int id, thread_entry_point func) : tid(id), entry_point(func) {}

    // No destructor needed yet (stack is inside the object)
};

// Global variables
unordered_map<int, Thread *> threads;
deque<int> ready_queue;
Thread *running_thread = nullptr;
int total_quantums = 0;
int quantum_usecs = 0;

// Timer
struct itimerval timer;

// Forward declarations
void scheduler(int sig);

// Helper: set timer
void set_timer()
{
    timer.it_value.tv_sec = quantum_usecs / 1000000;
    timer.it_value.tv_usec = quantum_usecs % 1000000;
    timer.it_interval.tv_sec = timer.it_value.tv_sec;
    timer.it_interval.tv_usec = timer.it_value.tv_usec;
    if (setitimer(ITIMER_VIRTUAL, &timer, nullptr) < 0)
    {
        cerr << "system error: setitimer failed" << endl;
        exit(1);
    }
}

// Helper: find smallest available tid
int find_available_tid()
{
    for (int i = 0; i < MAX_THREAD_NUM; ++i)
    {
        if (threads.find(i) == threads.end())
        {
            return i;
        }
    }
    return -1;
}

// Library API implementation

int uthread_init(int quantum_usecs_)
{
    if (quantum_usecs_ <= 0)
    {
        cerr << "thread library error: quantum_usecs must be positive" << endl;
        return -1;
    }
    quantum_usecs = quantum_usecs_;

    // Set timer signal
    struct sigaction sa = {};
    sa.sa_handler = scheduler;
    if (sigaction(SIGVTALRM, &sa, nullptr) < 0)
    {
        cerr << "system error: sigaction failed" << endl;
        exit(1);
    }

    // Create main thread (tid = 0)
    running_thread = new Thread(0, nullptr);
    running_thread->state = RUNNING;
    threads[0] = running_thread;
    total_quantums = 1;

    set_timer();
    return 0;
}

int uthread_spawn(thread_entry_point entry_point)
{
    if (entry_point == nullptr)
    {
        cerr << "thread library error: entry_point is null" << endl;
        return -1;
    }
    if ((int)threads.size() >= MAX_THREAD_NUM)
    {
        cerr << "thread library error: max thread number exceeded" << endl;
        return -1;
    }

    int tid = find_available_tid();
    if (tid == -1)
    {
        return -1;
    }

    Thread *t = new Thread(tid, entry_point);

    // Initialize context (environment)
    if (sigsetjmp(t->env, 1) == 0)
    {
        address_t sp = (address_t)t->stack + STACK_SIZE - sizeof(address_t);
        address_t pc = (address_t)entry_point;

        // Magic to set SP and PC
        // You need to define translate_address() depending on architecture
        t->env->__jmpbuf[JB_SP] = translate_address(sp);
        t->env->__jmpbuf[JB_PC] = translate_address(pc);
        sigemptyset(&t->env->__saved_mask);
    }

    t->state = READY;
    threads[tid] = t;
    ready_queue.push_back(tid);

    return tid;
}

int uthread_terminate(int tid)
{
    if (threads.find(tid) == threads.end())
    {
        cerr << "thread library error: invalid tid" << endl;
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

    // Remove from ready queue if needed
    ready_queue.erase(remove(ready_queue.begin(), ready_queue.end(), tid), ready_queue.end());

    delete threads[tid];
    threads.erase(tid);

    // If terminating yourself
    if (running_thread->tid == tid)
    {
        scheduler(0); // trigger scheduling manually
    }

    return 0;
}

int uthread_block(int tid)
{
    if (tid == 0 || threads.find(tid) == threads.end())
    {
        cerr << "thread library error: invalid tid" << endl;
        return -1;
    }

    Thread *t = threads[tid];
    if (t->state == BLOCKED)
    {
        return 0;
    }

    if (t->state == READY)
    {
        ready_queue.erase(remove(ready_queue.begin(), ready_queue.end(), tid), ready_queue.end());
    }
    t->state = BLOCKED;

    if (running_thread->tid == tid)
    {
        scheduler(0);
    }
    return 0;
}

int uthread_resume(int tid)
{
    if (threads.find(tid) == threads.end())
    {
        cerr << "thread library error: invalid tid" << endl;
        return -1;
    }

    Thread *t = threads[tid];
    if (t->state == BLOCKED)
    {
        t->state = READY;
        ready_queue.push_back(tid);
    }
    return 0;
}

int uthread_sleep(int num_quantums)
{
    if (running_thread->tid == 0)
    {
        cerr << "thread library error: main thread cannot sleep" << endl;
        return -1;
    }

    running_thread->state = SLEEPING;
    running_thread->sleep_quantums_left = num_quantums;
    scheduler(0);
    return 0;
}

int uthread_get_tid()
{
    return running_thread->tid;
}

int uthread_get_total_quantums()
{
    return total_quantums;
}

int uthread_get_quantums(int tid)
{
    if (threads.find(tid) == threads.end())
    {
        cerr << "thread library error: invalid tid" << endl;
        return -1;
    }
    return threads[tid]->quantums_run;
}

// Scheduler
void scheduler(int sig)
{
    if (sigsetjmp(running_thread->env, 1) == 1)
    {
        return;
    }

    // Sleep handling
    for (auto &pair : threads)
    {
        Thread *t = pair.second;
        if (t->state == SLEEPING && --t->sleep_quantums_left == 0)
        {
            t->state = READY;
            ready_queue.push_back(t->tid);
        }
    }

    // Pick next ready thread
    if (!ready_queue.empty())
    {
        int next_tid = ready_queue.front();
        ready_queue.pop_front();
        Thread *next_thread = threads[next_tid];

        if (running_thread->state == RUNNING)
        {
            running_thread->state = READY;
            ready_queue.push_back(running_thread->tid);
        }

        running_thread = next_thread;
        running_thread->state = RUNNING;
        running_thread->quantums_run++;
        total_quantums++;

        siglongjmp(running_thread->env, 1);
    }
}
