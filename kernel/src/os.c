#include <common.h>
#include <devices.h>
//#include<unistd.h>
//#include<assert.h>

#define IRQ_NUM 128

struct irq_handler{
    int seq;
    int event;
    handler_t handler;
};
typedef struct irq_handler irq_t;
irq_t handlers[IRQ_NUM];
uintptr_t htot = 0;

// static void os_init() 
// {
//     pmm->init();
//     kmt->init();
// }
static void os_init();
static void os_run();

static Context *os_trap(Event ev, Context *context) {
    Context *next = NULL;
    for (int i = 0; i < htot; ++i)
    {
        irq_t h = handlers[i];
        if (h.event == EVENT_NULL || h.event == ev.event) 
        {
            Context *r = h.handler(ev, context);
            panic_on(r && next, "return to multiple contexts");
            if (r) next = r;
        }
    }
    panic_on(!next, "return to NULL context");
    //panic_on(sane_context(next), "return to invalid context");
    return next;
}

static void os_on_irq(int seq, int event, handler_t handler)
{
    htot++;
    int i = htot - 1;
    for(; i >= 1; --i)
    {
        if(handlers[i - 1].seq < seq)
        {
            break;
        }
        handlers[i] = handlers[i - 1];
    }
    handlers[i] = (irq_t){seq, event, handler};
    return ;
}

MODULE_DEF(os) = {
    .init = os_init,
    .run  = os_run,
    .trap = os_trap,
    .on_irq = os_on_irq
};

#ifdef TEST1
#define N 2
#define NPROD 3
#define NCONS 2
extern task_t *current[MAX_CPU_NUM];
sem_t empty,fill;
//void T_produce(void *arg) { while (1) { kmt->sem_wait(&empty); printf("produce cpu%u",cpu_current());putch('(');putch('\n'); kmt->sem_signal(&fill);  } }
//void T_consume(void *arg) { while (1) { kmt->sem_wait(&fill);  printf("consume cpu%u",cpu_current());putch(')');putch('\n'); kmt->sem_signal(&empty); } }
void T_produce(void *arg) { while (1) { kmt->sem_wait(&empty); printf("%s from cpu%u",current[cpu_current()]->name, cpu_current()); putch('('); putch('\n');kmt->sem_signal(&fill);  } }
void T_consume(void *arg) { while (1) { kmt->sem_wait(&fill);  printf("%s from cpu%u",current[cpu_current()]->name, cpu_current()); putch(')'); putch('\n'); kmt->sem_signal(&empty); } }
//void T_produce(void *arg) { while (1) { kmt->sem_wait(&empty); putch('(');kmt->sem_signal(&fill);  } }
//void T_consume(void *arg) { while (1) { kmt->sem_wait(&fill);  putch(')');kmt->sem_signal(&empty); } }

static inline task_t *task_alloc() {
    return pmm->alloc(sizeof(task_t));
}
char pname[10][10] = {"p1", "p2", "p3", "p4", "p5", "p6"};
char cname[10][10] = {"c1", "c2", "c3", "c4", "c5", "c6"};
static void run_test1() {
    kmt->sem_init(&empty, "empty", N);
    kmt->sem_init(&fill,  "fill",  0);
    for (int i = 0; i < NPROD; i++) {
        printf("pro\n");
        kmt->create(task_alloc(), pname[i], T_produce, NULL);
    }
    for (int i = 0; i < NCONS; i++) {
        printf("con\n");
        kmt->create(task_alloc(), cname[i], T_consume, NULL);
    }

    
    //yield();
    //iset(true);
}

static void os_init() 
{
    pmm->init();
    kmt->init(); // 模块先初始化
    run_test1();
}
static void os_run() 
{
    iset(true);
    //yield();
    
    while (1) ;
}
#elif defined(TEST2)
static inline task_t *task_alloc() {
    return pmm->alloc(sizeof(task_t));
}

static void tty_reader(void *arg) {
    device_t *tty = dev->lookup(arg);
    char cmd[128], resp[128], ps[16];
    snprintf(ps, 16, "(%s) $ ", arg);
    while (1) {
        tty->ops->write(tty, 0, ps, strlen(ps));
        int nread = tty->ops->read(tty, 0, cmd, sizeof(cmd) - 1);
        cmd[nread] = '\0';
        sprintf(resp, "tty reader task: got %d character(s).\n", strlen(cmd));
        tty->ops->write(tty, 0, resp, strlen(resp));
    }
}

static void os_init() 
{    
    pmm->init();
    kmt->init();
    dev->init();
    kmt->create(task_alloc(), "tty_reader", tty_reader, "tty1");
    kmt->create(task_alloc(), "tty_reader", tty_reader, "tty2");
}
static void os_run() 
{
    iset(true);
    while (1) yield();
}
#else

static void os_init() 
{
    pmm->init();
    kmt->init(); // 模块先初始化
}

static void os_run() 
{
    iset(true);
    while (1) yield();
}
#endif

