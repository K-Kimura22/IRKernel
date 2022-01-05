#include <linux/module.h>
#include <linux/sched.h>
#include "systemcall.h"
#include "ipi.h"

DEFINE_STATIC_KEY_FALSE(sched_schedstats);

static void update_rq_clock_task(struct rq *rq, s64 delta)
{
  rq->clock_task += delta;
}

static void update_rq_clock(struct rq *rq)
{
  s64 delta;
  if (rq->clock_update_flags & RQCF_ACT_SKIP)
    return;

  delta = sched_clock_cpu(cpu_of(rq)) - rq->clock;
  if (delta < 0)
    return;
  rq->clock += delta;
  update_rq_clock_task(rq, delta);
}

void enqueue_task(struct rq *rq, struct task_struct *p,
		  int flags)
{
  enqueue_task_fair(rq, p, flags);
}

static void myactivate_task(struct rq *rq, struct task_struct *p, int flags)
{
  if (task_contributes_to_load(p))
    rq->nr_uninterruptible--;

  enqueue_task(rq, p, flags);
}

static inline void ttwu_activate(struct rq *rq, struct task_struct *p, int en_flags)
{
  myactivate_task(rq, p, en_flags);
  p->on_rq = TASK_ON_RQ_QUEUED;
}

static void ttwu_do_wakeup(struct rq *rq, struct task_struct *p,
                           int wake_flags, struct rq_flags *rf)
{
  p->state = TASK_RUNNING;
}

static void ttwu_do_activate(struct rq *rq, struct task_struct *p,
			     int wake_flags,struct rq_flags *rf)
{
  int en_flags = ENQUEUE_WAKEUP | ENQUEUE_NOCLOCK;

  ttwu_activate(rq, p, en_flags);
  ttwu_do_wakeup(rq, p, wake_flags, rf);
}

static void ttwu_queue(struct task_struct *p, int cpu,
		       int wake_flags)
{
  struct rq *rq = cpu_rq(cpu);
  struct rq_flags rf;

  rq_lock(rq, &rf);

  update_rq_clock(rq);
  ttwu_do_activate(rq, p, wake_flags, &rf);

  rq_unlock(rq, &rf);                                                         
}

static int try_to_wake_up(struct task_struct *p,
			  unsigned int state, int wake_flags)
{
  unsigned long flags;
  int cpu, success = 0;
  raw_spin_lock_irqsave(&p->pi_lock, flags);                            
  success = 1;
  cpu = task_cpu(p);
  ttwu_queue(p, cpu, wake_flags);
  raw_spin_unlock_irqrestore(&p->pi_lock, flags);
  
  return success;
}

int wake_up_state(struct task_struct *p, unsigned int state)
{
  return try_to_wake_up(p, state, 0);
}

MODULE_LICENSE("GPL");
