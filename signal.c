#include <linux/module.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include "systemcall.h"
#include "ipi.h"

static void mysigaddset(sigset_t *set, int _sig)
{
  unsigned long sig = _sig - 1;
  if (_NSIG_WORDS == 1)
    set->sig[0] |= 1UL << sig;
  else
    set->sig[sig / _NSIG_BPW] |= 1UL << (sig % _NSIG_BPW);
}

static void myset_bit(int nr, volatile unsigned long *addr)
{
  unsigned long mask = BIT_MASK(nr);
  unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

  *p  |= mask;
}

static inline void myset_ti_thread_flag(struct thread_info *ti, int flag)
{
  myset_bit(flag, (unsigned long *)&ti->flags);
}

static struct thread_info *mytask_thread_info(struct task_struct *task)
{
  return &task->thread_info;
}

static void myset_tsk_thread_flag(struct task_struct *tsk, int flag)
{
  myset_ti_thread_flag(mytask_thread_info(tsk), flag);
}

static void mysignal_wake_up_state(struct task_struct *t, unsigned int state)
{
  myset_tsk_thread_flag(t, TIF_SIGPENDING);
}

static void mysignal_wake_up(struct task_struct *t, bool resume)
{
  mysignal_wake_up_state(t, resume ? TASK_WAKEKILL : 0);
}

static void mysignal_wake_up_process_state(struct task_struct *t,
					   unsigned int state)
{
  if (!wake_up_state(t, state | TASK_INTERRUPTIBLE)){
    kick_process(t);
  }
}

static void mysignal_wake_up_process(struct task_struct *t, bool resume)
{
  mysignal_wake_up_process_state(t, resume ? TASK_WAKEKILL : 0);
}

static void check_process(struct task_struct *t)
{
  struct task_struct *p = &init_task;
  pid_t pid = t->pid;

  do{
    if(pid == p->pid)
      mysignal_wake_up_process(p, 1);
    p = list_entry(p->tasks.next, struct task_struct, tasks);
  }while(p != &init_task);
}

static void check_process_name(char name[])
{
  struct task_struct *p = &init_task;
  do{
    if(strcmp(p->comm, name)==0){
      mysignal_wake_up_process(p, 1);
    }
    p = list_entry(p->tasks.next, struct task_struct, tasks);
  }while(p != &init_task);
}

void sig_kill(struct task_struct *p){
  mysigaddset(&p->pending.signal, SIGNAL);
  mysignal_wake_up(p, 1);
  mdelay(10);
  check_process(p);
}

void name_sig_kill(char name[]){
  struct task_struct *p = &init_task;
  do{
    if(strcmp(p->comm, name)==0){
      mysigaddset(&p->pending.signal, SIGNAL);
      mysignal_wake_up(p, 1);
    }
    p = list_entry(p->tasks.next, struct task_struct, tasks);
  }while(p != &init_task);
  mdelay(10);
  check_process_name(name);
}

MODULE_LICENSE("GPL");
