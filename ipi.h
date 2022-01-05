#include "kernel/sched/sched.h"

extern int wake_up_state(struct task_struct *p, unsigned int state);
extern void enqueue_task_fair(struct rq *rq, struct task_struct *p, int flags);
