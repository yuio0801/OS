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