#include <kernel.h>
#include <klib.h>
#include <klib-macros.h>
#include <mutexlock.h>

#define MAX_CPU_NUM 8

#ifndef __mutexLOCK_H__
#define __mutexLOCK_H__

#define UNLOCKED  0
#define LOCKED    1

struct cpu {
    int noff;
    int intena;
};
typedef struct mutexlock{
    const char *name;
    int status;
    struct cpu *cpu;
} mutexlock_t;

void mutex_lock(mutexlock_t *lk);
bool mutex_trylock(mutexlock_t *lk);
void mutex_unlock(mutexlock_t *lk);
void mutex_init(mutexlock_t *lk,const char *name);
void push_off();
void pop_off();
bool holding(mutexlock_t *lk);
#endif

#ifndef __TASK_H__
#define __TASK_H__


typedef struct semaphore sem_t;
#define STK_SZ 10240
#define MAGIC 19260817
#define RUNNABLE 0
#define RUNNING 1
#define BLOCKED 2

typedef struct task{
    union{
        struct{
            const char    *name;
            uint32_t      idle;
            void          (*entry)(void *);
            Context       *context;
            int cpu;
            struct cpu cpu_status;
            uint32_t      status;
            mutexlock_t    lk;
            sem_t *sem;
            struct task   *next;
            struct task   *pre;
            uint32_t      canary1;
            int ccc;
            char          end[0];
        };
        uint8_t stack[STK_SZ];
    };
    uint32_t      canary2;
}task_t;
// __attribute__((packed)) 

#define uaf_check(ptr) \
    panic_on(MAGIC != *(uint32_t *)(ptr), "use-after-free");

#endif

#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__
typedef struct semaphore{
    const char *name;
    int value;
    task_t *waitlist;
    mutexlock_t lk; 
}sem_t;

// typedef struct spinlock{
//     sem_t lk;
// }spinlock_t;
typedef struct spinlock{
    const char *name;
    int status;
    struct cpu *cpu;
} spinlock_t;
#endif