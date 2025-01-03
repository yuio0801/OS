#include <common.h>

#define INI_MIN 0
#define INI_MAX 9999999

void print_context(Context *ctx)
{
    printf("rax %u ",ctx->rax);
    printf("rbx %u ",ctx->rbx);
    printf("rcx %u ",ctx->rcx);
    printf("rdx %u ",ctx->rdx);
    printf("rbp %u ",ctx->rbp);
    printf("rip %u ",ctx->rip);
    printf("rsp %u ",ctx->rsp);
    printf("rsp0 %u ",ctx->rsp0);
    printf("\n");
    printf("\n");
}

extern struct cpu cpus[16];
task_t *current[MAX_CPU_NUM];
mutexlock_t tasklk[MAX_CPU_NUM];
task_t *taskhead[MAX_CPU_NUM];
int ccc[MAX_CPU_NUM];
int32_t num_cpu;

mutexlock_t printlock;

static int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg);
static void sem_init(sem_t *sem, const char *name, int value);
static void sem_wait(sem_t *sem);
static void sem_signal(sem_t *sem);
Context *kmt_context_save(Event ev, Context *context);
Context *kmt_schedule(Event ev, Context *context);
Context *print_tasklist(Event ev, Context *context);

int next_cpu(int tmp_cpu)
{
    return (tmp_cpu + 1) % num_cpu;
    //return tmp_cpu;
}

void idle_entry(void *arg)
{
    while(1)
    {
        yield();
    }
}

static void add_to_list(task_t *tmp, int cpu, uint32_t status)
{
    tmp->cpu = cpu;
    tmp->status = status;
    if(!taskhead[cpu])
    {
        tmp->pre = tmp->next = tmp;
        taskhead[cpu] = tmp;
    }
    else
    {
        tmp->next = taskhead[cpu];
        tmp->pre = taskhead[cpu]->pre;
        taskhead[cpu]->pre->next = tmp;
        taskhead[cpu]->pre = tmp;
    }
}

static void remove_from_list(task_t *tmp, int cpu, uint32_t status)
{
    tmp->status = status;
    tmp->next->pre = tmp->pre;
    tmp->pre->next = tmp->next;
    if(taskhead[cpu] == tmp)
    {
        if(tmp->next == tmp)
        {
            taskhead[cpu] = NULL;
        }
        else
        {
            taskhead[cpu] = tmp->next;
        }
    }
}
static void add_to_waitlist(task_t *tmp, sem_t *sem, uint32_t status)
{
    tmp->sem = sem;
    tmp->status = status;
    if(sem->waitlist == NULL)
    {
        tmp->next = tmp->pre = tmp;
        sem->waitlist = tmp;
    }
    else
    {
        tmp->next = sem->waitlist;
        tmp->pre = sem->waitlist->pre;
        sem->waitlist->pre->next = tmp;
        sem->waitlist->pre = tmp;
    }
}
static void remove_from_waitlist(task_t *tmp, sem_t *sem, uint32_t status)
{
    tmp->sem = NULL;
    tmp->status = status;
    tmp->next->pre = tmp->pre;
    tmp->pre->next = tmp->next;
    if(sem->waitlist == tmp)
    {
        if(tmp->next == tmp)
        {
            sem->waitlist = NULL;
        }
        else
        {
            sem->waitlist = tmp->next;
        }
    }
}

static void kmt_init()
{
    mutex_init(&printlock, "printlock");
    num_cpu = cpu_count();
    
    os->on_irq(INI_MIN, EVENT_NULL,kmt_context_save);
    //os->on_irq(INI_MAX - 2, EVENT_NULL, print_tasklist);
    os->on_irq(INI_MAX - 1, EVENT_NULL,kmt_schedule);
    //os->on_irq(INI_MAX , EVENT_NULL, print_tasklist);
    for(int i = 0; i < num_cpu; ++i)
    {
        mutex_init(&tasklk[i], "tasklistlock");
        task_t *tmp = pmm->alloc(sizeof(task_t));
        tmp->context = kcontext((Area){&tmp->end,&tmp->canary2}, idle_entry, NULL);
        tmp->name = "idle";
        tmp->idle = 1;
        tmp->cpu = i;
        mutex_init(&(tmp->lk), "idlelock");
        add_to_list(tmp, i, RUNNABLE);
        tmp->canary1 = MAGIC;
        tmp->canary2 = MAGIC;

        tmp->ccc = 10;

        ccc[i] = 10;
    }

    for(int i = 0; i < num_cpu; ++i)
    {
        task_t *tmp = pmm->alloc(sizeof(task_t));
        tmp->context = kcontext((Area){&tmp->end,&tmp->canary2}, NULL, NULL);
        current[i] = tmp;
        tmp->name = "initial";
        tmp->idle = 0;
        tmp->cpu = i;
        tmp->status = RUNNING;
        mutex_init(&(tmp->lk), "initialminitasklock");
        mutex_lock(&(tmp->lk));
        tmp->lk.cpu = &cpus[i];
        //iset(true);
        tmp->canary1 = MAGIC;
        tmp->canary2 = MAGIC;
        add_to_list(tmp, i ,RUNNING);
    }
}
int tmcpu = 0;
static int kmt_create(task_t *task, const char *name, void (*entry)(void *arg), void *arg)
{
    task->context = kcontext((Area){&task->end,&task->canary2}, entry, arg);
    task->name = name;
    task->entry = entry;
    task->canary1 = task->canary2 = MAGIC;
    task->idle = 0;
    task->ccc = 10;
    task->cpu = tmcpu;
    
    //tmcpu = next_cpu(tmcpu);
    mutex_init(&(task->lk), "createminitasklock");

    bool st = ienabled();
    iset(false);
    mutex_lock(&tasklk[task->cpu]);
    tmcpu = (tmcpu + 1) %num_cpu;
    add_to_list(task, task->cpu, RUNNABLE);
    mutex_unlock(&tasklk[task->cpu]);
    if(st) 
    {
        iset(true);
    }
    //assert(cpus[cpu_current()].noff == 0);
    assert(task->canary1 == MAGIC);
    assert(task->canary2 == MAGIC);
    return 0;
}

static void kmt_teardown(task_t *task)
{
    //assert(task->status == RUNNABLE);//?
    //assert(0);
    int cpu = cpu_current();
    iset(false);
    mutex_lock(&tasklk[cpu]);
    remove_from_list(task, cpu , RUNNABLE);
    mutex_unlock(&tasklk[cpu]);
    iset(true);
    pmm->free((void *)task);
}

static void sem_init(sem_t *sem, const char *name, int value)
{
    sem->name = name;
    sem->value = value;
    mutex_init(&(sem->lk), "semlock");
    sem->waitlist = NULL;
}

void sem_wait(sem_t *sem)
{
    int cpu = cpu_current();
    int acquired = 0;
    iset(false);
    mutex_lock(&(sem->lk));
    if(sem->value == 0)
    {
        //assert(current[cpu]->status == RUNNING);
        mutex_lock(&tasklk[cpu]);
        remove_from_list(current[cpu], cpu, BLOCKED);
        mutex_unlock(&tasklk[cpu]);
        current[cpu]->cpu = next_cpu(cpu);
        add_to_waitlist(current[cpu], sem, BLOCKED);

    }
    else
    {
        sem->value --;
        acquired = 1;
    }
    mutex_unlock(&(sem->lk));
    iset(true);
    if(!acquired) 
    {
        yield();
    }
}

void sem_signal(sem_t *sem) 
{   
    int find = 0;
    //int cpu = cpu_current();
    iset(false);
    mutex_lock(&(sem->lk));

    //remove_from_waitlist
    task_t *tmp = sem->waitlist;

    if(tmp)
    {
        while(!find)
        {
            if(mutex_trylock(&(tmp->lk)))
            {
                find = 1;
                break;
            }
            tmp=tmp->next;
        }
        int tar_cpu = tmp->cpu;
        remove_from_waitlist(tmp, sem, RUNNABLE);
        mutex_lock(&(tasklk[tar_cpu]));
        add_to_list(tmp, tar_cpu, RUNNABLE);
        mutex_unlock(&(tmp->lk));
        mutex_unlock(&(tasklk[tar_cpu]));

    }
    else
    {
        sem->value++;
    }


    mutex_unlock(&(sem->lk));
    iset(true);
}

Context *kmt_context_save(Event ev, Context *context)
{
    int cpu = cpu_current();
    current[cpu]->cpu_status = cpus[cpu];
    //iset(false);
    current[cpu]->context = context; 
    //current[cpu]->cpu_status = cpus[cpu];
    assert(current[cpu]->canary1 == MAGIC);
    assert(current[cpu]->canary2 == MAGIC);

    return NULL;
}
Context *kmt_schedule(Event ev, Context *context)
{
    int cpu = cpu_current();
    task_t *tmp = NULL;
    int find = 0;
    mutex_lock(&tasklk[cpu]);
    if(current[cpu]->status == RUNNING)
    {
        tmp = current[cpu]->next;
        while(tmp != current[cpu])
        {
            if(tmp->status == RUNNABLE && mutex_trylock(&(tmp->lk)))
            {
                find = 1;
                break;
            }
            tmp = tmp->next;
        }
    }
    else if(current[cpu]->status == BLOCKED)// || current[cpu]->status == RUNNABLE)
    {
        tmp = taskhead[cpu];
        do
        {
            if(tmp == current[cpu]) 
            {
                tmp = tmp->next;
                continue;
            }
            if(mutex_trylock(&(tmp->lk)))
            {
                find = 1;
                break;
            }
            tmp = tmp->next;
        }
        while(tmp != taskhead[cpu]);
    }
    // else 
    // {
    //     assert(0);
    // }

    if(!find)
    {
        mutex_unlock(&tasklk[cpu]);
        cpus[cpu] = tmp->cpu_status;
        return current[cpu]->context;
    }
    //assert(tmp!=NULL);
    tmp->status = RUNNING;
    //pop_off();
    if(current[cpu]->status == RUNNING)
    {
        current[cpu]->status = RUNNABLE;
        //push_off();
        
        if(current[cpu]->idle == 1)
        {
            mutex_unlock(&tasklk[cpu]);
            mutex_unlock(&(current[cpu]->lk));
        }
        else
        {
            remove_from_list(current[cpu], cpu, RUNNABLE);
            
            mutex_unlock(&tasklk[cpu]);
            //mutex_unlock(&(current[cpu]->lk));//?
            
            current[cpu]->ccc--;
            if(current[cpu]->ccc <= 0)
            {
                current[cpu]->ccc = 10;
                mutex_lock(&tasklk[next_cpu(cpu)]);
                add_to_list(current[cpu], next_cpu(cpu), RUNNABLE);
                mutex_unlock(&tasklk[next_cpu(cpu)]);
                mutex_unlock(&(current[cpu]->lk));//? !!
            }
            else
            {
                mutex_lock(&tasklk[cpu]);
                add_to_list(current[cpu], cpu, RUNNABLE);
                mutex_unlock(&tasklk[cpu]);
                mutex_unlock(&(current[cpu]->lk));//? !!
            }

        }
    }
    else if(current[cpu]->status == BLOCKED)
    {
        //push_off();
        mutex_unlock(&tasklk[cpu]);
        mutex_unlock(&(current[cpu]->lk));
    }
    else 
    {
        mutex_unlock(&(current[cpu]->lk));
        mutex_unlock(&tasklk[cpu]);
    }
    current[cpu] = tmp;
    cpus[cpu] = tmp->cpu_status;
    return tmp->context;
}

Context *print_tasklist(Event ev, Context *context)
{
    //return context;
    //int cpu = cpu_current();
    //printf("into print %d\n",tasklk[0].status);
    mutex_lock(&printlock);
    printf("stop from cpu%u: ",cpu_current());
    for(int i=0;i<num_cpu;++i)
    {
        mutex_lock(&tasklk[i]);
        
        printf("cpu%u:",i);
        task_t *tmp = taskhead[i];
        if(tmp)
        {
            do{
            
            printf("%s(%u)->",tmp->name, tmp->status);
            
            tmp=tmp->next;
            }
            while(tmp != taskhead[i]);
            
        }
        
        
        
        mutex_unlock(&tasklk[i]);
    }for(int i=0;i<cpu_count();++i)
        {
            printf(" cpu%u:%s(%u) ",i,current[i]->name, current[i]->status);
        }
        printf("\n");mutex_unlock(&printlock);
    
    //printf("out print %d",tasklk[0].status);
    return NULL;
}

// static void spin_init(spinlock_t *lk, const char *name)
// {
//     sem_init(&(lk->lk), name, 1);
// }

// static void spin_lock(spinlock_t *lk)
// {
//     sem_wait(&(lk->lk));
// }

// static void spin_unlock(spinlock_t *lk)
// {
//     sem_signal(&(lk->lk));
// }

bool sholding(spinlock_t *lk) {
    return (
        lk->status == LOCKED &&
        lk->cpu == &cpus[cpu_current()]
    );
}

void spin_init(spinlock_t *lk,const char *name){
    lk->name = name;
    lk->status = UNLOCKED;
    lk->cpu = NULL;
}

void spin_lock(spinlock_t *lk) {
    // Disable interrupts to avoid deadlock.
    iset(false);

    // This is a deadlock.
    // if (sholding(lk)) {
    //     printf("%s",lk->name);
    //     panic("dead lock11111");
    // }

    // This our main body of mutex lock.
    int got;
    do {
        got = atomic_xchg(&lk->status, LOCKED);
    } while (got != UNLOCKED);

    lk->cpu = &cpus[cpu_current()];
}

void spin_unlock(spinlock_t *lk) {
    // if (!sholding(lk)) {
    //     printf("%s",lk->name);
    //     panic("dead lock22222");
    // }

    lk->cpu = NULL;
    atomic_xchg(&lk->status, UNLOCKED);
    iset(true);
}

MODULE_DEF(kmt) = {
    .init = kmt_init,
    .create = kmt_create,
    .teardown = kmt_teardown,
    .spin_init = spin_init,
    .spin_lock = spin_lock,
    .spin_unlock = spin_unlock,
    .sem_init = sem_init,
    .sem_wait = sem_wait,
    .sem_signal = sem_signal,
};
