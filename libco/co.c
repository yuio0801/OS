#include "co.h"
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>

#ifdef LOCAL_MACHINE
  #define debug(...) printf(__VA_ARGS__)
#else
  #define debug(...)
#endif
//To make the stack be aligned by 16 bytes
#define STACK_SIZE 65536
#define N 128
#define STATUS_NUM 5

enum co_status {
    CO_NEW = 1, // 新创建，还未执行过
    CO_RUNNING, // 已经执行过
    CO_WAITING, // 在 co_wait 上等待
    CO_DEAD,    // 已经结束，但还未释放资源
};

struct co {
    const char *name;
    void (*func)(void *); // co_start 指定的入口地址和参数
    void *arg;

    enum co_status status;  // 协程的状态
    struct co *    waiter;  // 是否有其他协程在等待当前协程
    struct co *    pre;
    struct co *    nxt;
    jmp_buf        context; // 寄存器现场 (setjmp.h)
    uint8_t        stack[STACK_SIZE] __attribute__((aligned(16))); // 协程的堆栈
};

int RUNNING_NUM = 0;

static inline void stack_switch_call(void *sp, void *entry, uintptr_t arg) // Only call wrapper
{
  asm volatile (
#if __x86_64__
    "\
    movq %0, %%rsp; \
    movq %2, %%rdi; \
    jmp *%1 \
    "
      : : "b"((uintptr_t)sp), "d"(entry), "a"(arg) : "memory"
#else
    "\
    movl %0, %%esp; \
    movl %2, 4(%0); \
    jmp *%1\
    "
      : : "b"((uintptr_t)sp - 8), "d"(entry), "a"(arg) : "memory"
#endif
  );
}

struct co *current;

void co_init() __attribute__((constructor));

void co_init() 
{
  //debug("co_init ");
  current = malloc(sizeof(struct co));
  current->name = "main";
  current->pre = current;
  current->nxt = current;
  current->status = CO_RUNNING; 
  current->waiter = NULL;
  current->func = NULL;
  current->arg = NULL;  //NO need, never be called by stack_switch_call()

  RUNNING_NUM = 1;
}


void wrapper() // Never return!! Only be called once by stack_switch_call
{ //push will make rsp - 8
  //debug("wrapper ");
  current->status = CO_RUNNING;
  current->func(current->arg);
  current->status = CO_DEAD;
  current->pre->nxt = current->nxt; // move out from the list
  current->nxt->pre = current->pre;
  if(current->waiter)
  {
    current->waiter->status = CO_RUNNING;

    RUNNING_NUM++;
  }

  RUNNING_NUM--;
  
  co_yield(); // Never return!!

}

struct co *co_start(const char *name, void (*func)(void *), void *arg) 
{
  //debug("co_start ");
  struct co *tmp = malloc(sizeof(struct co));
  tmp->name = name;
  tmp->func = func;
  tmp->arg = arg;
  tmp->status = CO_NEW;
  tmp->waiter = NULL;

  tmp->nxt = current->nxt;
  current->nxt->pre = tmp;
  tmp->pre = current;
  current->nxt = tmp;

  RUNNING_NUM++;

  return tmp;
}

void co_wait(struct co *co) 
{
  //debug("co_wait ");
  if(co->status != CO_DEAD)
  {
    current->status = CO_WAITING;

    co->waiter = current;
    co_yield();
  }
  assert(co->status == CO_DEAD);
  free(co);
  return;
}

void co_yield() 
{
  //debug("co_yield ");
  int val = setjmp(current->context);
  if (val == 0) 
  {
      struct co *target = current;
      srand(time(NULL));
      int step = rand()% RUNNING_NUM + (current->status == CO_WAITING || current->status == CO_DEAD);
      //int step = 1;
      while(step)
      {
        target = target->nxt;
        assert(target->status != CO_DEAD);
        if(target->status != CO_WAITING)
        {
          step--;
        }
      }
      current = target;
      if(current->status == CO_NEW)
      {
        stack_switch_call(&(current->stack[STACK_SIZE - 8]), &wrapper, (uintptr_t)current->arg);
      }
      else if(current->status == CO_RUNNING)
      {
        longjmp(current->context, 1);
      }
      else
      {
        assert(0);
      }
  } 
  else 
  {
    return ;
  }
}
