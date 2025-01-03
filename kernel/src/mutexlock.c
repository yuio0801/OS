#include <common.h>

struct cpu cpus[16];
#define mycpu (&cpus[cpu_current()])
extern task_t * current[MAX_CPU_NUM];
// This is a ported version of mutex-lock
// from xv6-riscv to AbstractMachine:
// https://github.com/mit-pdos/xv6-riscv

void push_off();
void pop_off();
bool holding(mutexlock_t *lk);

void mutex_init(mutexlock_t *lk,const char *name){
    lk->name = name;
    lk->status = UNLOCKED;
    lk->cpu = NULL;
}

void mutex_lock(mutexlock_t *lk) {
    // Disable interrupts to avoid deadlock.
    //push_off();

    // This is a deadlock.
    // if (holding(lk)) {
    //     printf("%s",lk->name);
    //     panic("dead lock11111");
    // }

    // This our main body of mutex lock.
    int got;
    do {
        got = atomic_xchg(&lk->status, LOCKED);
    } while (got != UNLOCKED);

    lk->cpu = mycpu;
}

void mutex_unlock(mutexlock_t *lk) {
    // if (!holding(lk)) {
    //     printf("%s",lk->name);
    //     panic("dead lock22222");
    // }

    lk->cpu = NULL;
    atomic_xchg(&lk->status, UNLOCKED);

    //pop_off();
}

bool mutex_trylock(mutexlock_t *lk)
{
    //push_off();
    // if(holding(lk))
    // {
    //     printf("%s",lk->name);
    //     panic("dead lock33333");
    // }
    int got = atomic_xchg(&lk->status, LOCKED);
    if(got == UNLOCKED)
    {
        lk->cpu = mycpu;
        return true;
    }
    else
    {
        //pop_off();
        return false;
    }
}
// Check whether this cpu is holding the lock.
// Interrupts must be off.
bool holding(mutexlock_t *lk) {
    return (
        lk->status == LOCKED &&
        lk->cpu == &cpus[cpu_current()]
    );
}

// push_off/pop_off are like intr_off()/intr_on()
// except that they are matched:
// it takes two pop_off()s to undo two push_off()s.
// Also, if interrupts are initially off, then
// push_off, pop_off leaves them off.
void push_off(void) {
    int old = ienabled();
    struct cpu *c = mycpu;
    iset(false);
    if (c->noff == 0) {
        c->intena = old;
    }
    c->noff += 1;
}

void pop_off(void) {
    struct cpu *c = mycpu;

    // Never enable interrupt when holding a lock.
    // if (ienabled()) {
    //     printf("%s %u",current[cpu_current()]->name, current[cpu_current()]->status);
    //     panic("pop_off - interruptible");
    // }
    
    // if (c->noff < 1) {
    //     printf("%s %u",current[cpu_current()]->name, current[cpu_current()]->status);
    //     panic("pop_off");
    // }

    c->noff -= 1;
    if (c->noff == 0 && c->intena) {
        iset(true);
    }
}