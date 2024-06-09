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


#include <string>
#include <utility>

#define MEMORY_ALLOC_ERROR "system error: memory allocation unsuccessful"
#define INVALID_INPUT_ERROR "system error: invalid input detected"
#define SIGNAL_ACTION_ERROR "system error: signal handling failure"
#define THREAD_SLOT_ERROR "system error: no available slot for new thread"
#define THREAD_SLEEP_ERROR "system error: thread 0 cannot be put to sleep"
#define THREAD_TERMINATE_ERROR "system error: thread 0 cannot be terminated"
#define THREAD_TERMINATE_SELF_ERROR "system error: thread cannot terminate itself"

class Thread {
public:
    // Constructors
    Thread(std::string stk, bool blocked, int quantums, int sleep)
        : stack(std::move(stk)), isBlocked(blocked), threadQuantums(quantums), sleepTime(sleep) {}

    // Getters and Setters
    std::string getStack() const { return stack; }
    void setStack(const std::string& stk) { stack = stk; }

    bool getIsBlocked() const { return isBlocked; }
    void setIsBlocked(bool blocked) { isBlocked = blocked; }

    int getThreadQuantums() const { return threadQuantums; }
    void setThreadQuantums(int quantums) { threadQuantums = quantums; }

    int getSleepTime() const { return sleepTime; }
    void setSleepTime(int sleep) { sleepTime = sleep; }

private:
    std::string stack;
    bool isBlocked;
    int threadQuantums;
    int sleepTime;
};


class ThreadGlobals {
public:
    int THREAD_QUANTUM_DURATION;
    std::unordered_map<unsigned int,Thread> threads;
    sigjmp_buf env[MAX_THREAD_NUM];
    int minAvailableThreadID[MAX_THREAD_NUM] = {0};
    int threadQuantumCounter = 0;

};

#ifdef __x86_64__
/* code for 64 bit Intel arch */

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





int uthread_init(int quantum_usecs) {
    if (quantum_usecs <= 0){
        std::cerr << INVALID_INPUT_ERROR << std::endl;
        return -1;
    }
    auto* mainThread = new Thread("", false, 1, 0);


}