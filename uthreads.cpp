#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdbool.h>
#include <map>
#include <list>
#include <algorithm>
#include <ostream>
#include <iostream>
#include "uthreads.h"
#include <memory>
#include <set>

#include <queue>
#include <string>
#include <utility>

#define MEMORY_ALLOC_ERROR "system error: memory allocation unsuccessful"
#define INVALID_INPUT_ERROR "system error: invalid input detected"
#define SIGNAL_ACTION_ERROR "system error: signal handling failure"
#define THREAD_SLOT_ERROR "system error: no available slot for new thread"
#define THREAD_SLEEP_ERROR "system error: thread 0 cannot be put to sleep"
#define THREAD_TERMINATE_ERROR "system error: thread 0 cannot be terminated"
#define THREAD_TERMINATE_SELF_ERROR "system error: thread cannot terminate itself"
#define THREAD_TERMINATE_SELF_ERROR "system error: thread cannot terminate itself"
#define ERROR -1;


void removeTid(std::queue<int> &q, int element);

class Thread {
public:
    // Constructors
    Thread(char *stk, bool blocked, int quantums, int sleep)
        : stack(stk), isBlocked(blocked), threadQuantums(quantums), sleepTime(sleep) {
    }

    // Getters and Setters
    char *getStack() const { return stack; }
    void setStack(char *stk) { stack = stk; }

    bool getIsBlocked() const { return isBlocked; }
    void setIsBlocked(bool blocked) { isBlocked = blocked; }

    int getThreadQuantums() const { return threadQuantums; }
    void setThreadQuantums(int quantums) { threadQuantums = quantums; }

    int getSleepTime() const { return sleepTime; }
    void setSleepTime(int sleep) { sleepTime = sleep; }

private:
    char *stack;
    bool isBlocked;
    int threadQuantums;
    int sleepTime;
};


class ThreadTidManager {
public:
    ThreadTidManager() {
        // Initialize with a range of possible TIDs
        for (int i = 0; i < MAX_THREAD_NUM; ++i) {
            availableTids.insert(i);
        }
    }

    int allocateTid() {
        if (availableTids.empty()) {
            return ERROR;
        }

        // Get the smallest available TID
        int tid = *availableTids.begin();
        availableTids.erase(availableTids.begin());
        return tid;
    }

    void releaseTid(int tid) {
        availableTids.insert(tid);
    }

private:
    std::set<int> availableTids;
};

class ThreadGlobals {
public:
    // ThreadGlobals(int quantum_usecs) : THREAD_QUANTUM_DURATION(quantum_usecs) {}
    int THREAD_QUANTUM_DURATION;
    std::unordered_map<unsigned int, Thread> threads;
    sigjmp_buf env[MAX_THREAD_NUM];
    ThreadTidManager tidManager;
    int threadQuantumCounter = 0;
    int tidOfCurrentThread = 0;
    std::queue<int> redayThreadQueue;
    std::list<int> sleepingThread;
    struct sigaction sa = {0};
    struct itimerval timer;
};

ThreadGlobals threadGlobals;
#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr) {
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
        "rol    $0x11,%0\n"
        : "=g" (ret)
        : "0" (addr));
    return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5


/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}


#endif

void PassToNextThread(int tid=-1, bool terminate=false) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        std::cerr << INVALID_INPUT_ERROR << std::endl;
        exit(EXIT_FAILURE);
    }

    if (sigsetjmp(threadGlobals.env[threadGlobals.tidOfCurrentThread], 1) == 0) {
        if (!terminate) {
            threadGlobals.redayThreadQueue.push(threadGlobals.tidOfCurrentThread);
        }

        threadGlobals.tidOfCurrentThread = threadGlobals.redayThreadQueue.front();
        threadGlobals.redayThreadQueue.pop();
        threadGlobals.threadQuantumCounter++;
        siglongjmp(threadGlobals.env[threadGlobals.tidOfCurrentThread], 1);
    }

    siglongjmp(threadGlobals.env[threadGlobals.tidOfCurrentThread], 1);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

void timer_handler(int sig) {
    // Exit early if the signal is not 26
    if (sig != 26) return;

    // Prepare to switch threads by saving the current thread's context
    threadGlobals.redayThreadQueue.push(threadGlobals.tidOfCurrentThread);

    // Save the current thread context and check if we just saved or restored
    if (sigsetjmp(threadGlobals.env[threadGlobals.tidOfCurrentThread], 1) == 0) {
        // If we just saved the context, switch to the next thread
        PassToNextThread();
    }
}



int uthread_init(int quantum_usecs) {
    if (quantum_usecs <= 0) {
        std::cerr << INVALID_INPUT_ERROR << std::endl;
        return ERROR;
    }
    Thread mainThread = Thread("", false, 1, 0);
    threadGlobals.THREAD_QUANTUM_DURATION = quantum_usecs;
    threadGlobals.threads[0] = mainThread;
    threadGlobals.tidManager.allocateTid();
    // Set the timer handler for SIGVTALRM
    threadGlobals.sa.sa_handler = &timer_handler;
    if (sigaction(SIGVTALRM, &threadGlobals.sa, NULL) < 0) {
        std::cerr << SIGNAL_ACTION_ERROR << std::endl;
        exit(EXIT_FAILURE);
    }

    // Configure the timer
    threadGlobals.timer.it_value.tv_sec = 0;
    threadGlobals.timer.it_value.tv_usec = threadGlobals.THREAD_QUANTUM_DURATION;

    // Start the timer and handle potential errors
    if (setitimer(ITIMER_VIRTUAL, &threadGlobals.timer, NULL) != 0) {
        std::cerr << SIGNAL_ACTION_ERROR << std::endl;
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}


int uthread_spawn(thread_entry_point entry_point) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        std::cerr << INVALID_INPUT_ERROR << std::endl;
        return ERROR;
    }
    if (entry_point == nullptr) {
        std::cerr << INVALID_INPUT_ERROR << std::endl;
        return ERROR;
    }
    int tid = threadGlobals.tidManager.allocateTid();
    if (tid == -1) {
        std::cerr << THREAD_SLOT_ERROR << std::endl;
        return ERROR;
    }
    char *stack = new char[STACK_SIZE];
    Thread newThread = Thread(stack, false, 0, 0);

    // initializes env[tid] to use the right stack, and to run from the function 'entry_point', when we'll use
    // siglongjmp to jump into the thread.
    address_t sp = (address_t) stack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) entry_point;
    sigsetjmp(threadGlobals.env[tid], 1);
    (threadGlobals.env[tid]->__jmpbuf)[JB_SP] = translate_address(sp);
    (threadGlobals.env[tid]->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&threadGlobals.env[tid]->__saved_mask);
    threadGlobals.threads[tid] = newThread;
    threadGlobals.redayThreadQueue.push(tid);
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    return EXIT_SUCCESS;
}


int uthread_terminate(int tid) {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGALRM);
    if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0) {
        std::cerr << INVALID_INPUT_ERROR << std::endl;
        return ERROR;
    }
    if (tid == 0) {
        // If the main thread is terminated, exit the process
        for (auto &thread: threadGlobals.threads) {
            delete[] thread.second.getStack();
        }
        exit(EXIT_SUCCESS);
    }
    if (threadGlobals.threads.find(tid) == threadGlobals.threads.end()) {
        std::cerr << INVALID_INPUT_ERROR << std::endl;
        return ERROR;
    }
    // if (tid == threadGlobals.tidOfCurrentThread) {
    //     PassToNextThread(tid, true);
    // }
    threadGlobals.tidManager.releaseTid(tid);
    delete[] threadGlobals.threads[tid].getStack();
    threadGlobals.threads.erase(tid);
    removeTid(threadGlobals.redayThreadQueue, tid);
    if (tid == threadGlobals.tidOfCurrentThread) {
        PassToNextThread(tid, true);
    }

    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    return EXIT_SUCCESS;
}

int uthread_get_tid() {
    return threadGlobals.tidOfCurrentThread;
}
int uthread_get_total_quantums() {
    return threadGlobals.threadQuantumCounter;
}
int uthread_get_quantums(int tid) {
    if (threadGlobals.threads.find(tid) == threadGlobals.threads.end()) {
        std::cerr << INVALID_INPUT_ERROR << std::endl;
        return ERROR;
    }
    return threadGlobals.threads[tid].getThreadQuantums();
}

void removeTid(std::queue<int> &q, int element) {
    std::queue<int> tempQueue;

    while (!q.empty()) {
        if (q.front() != element) {
            tempQueue.push(q.front());
        }
        q.pop();
    }

    q = tempQueue;
}

