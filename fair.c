#include <linux/module.h>
#include <linux/compiler.h>
#include "ipi.h"

#define jump_label_key__true  STATIC_KEY_INIT_TRUE
#define jump_label_key__false STATIC_KEY_INIT_FALSE
#define SCHED_FEAT(name, enabled)		\
  jump_label_key__##enabled ,
struct static_key sched_feat_keys[__SCHED_FEAT_NR] = {
#include "kernel/sched/features.h"
};

#define UTIL_AVG_UNCHANGED 0x1
#define for_each_sched_entity(se) \
		for (; se; se = se->parent)
#define entity_is_task(se)	1

unsigned int sysctl_sched_latency			= 6000000ULL;
static unsigned int sched_nr_latency = 8;

static void update_load_add(struct load_weight *lw, unsigned long inc)
{
  lw->weight += inc;
  lw->inv_weight = 0;
}

#define WMULT_CONST	(~0U)
#define WMULT_SHIFT	32

static void __update_inv_weight(struct load_weight *lw)
{
  unsigned long w;

  if (likely(lw->inv_weight))
    return;

  w = scale_load_down(lw->weight);

  if (BITS_PER_LONG > 32 && unlikely(w >= WMULT_CONST))
    lw->inv_weight = 1;
  else if (unlikely(!w))
    lw->inv_weight = WMULT_CONST;
  else
    lw->inv_weight = WMULT_CONST / w;
}

static u64 __calc_delta(u64 delta_exec, unsigned long weight,
			struct load_weight *lw)
{
  u64 fact = scale_load_down(weight);
  int shift = WMULT_SHIFT;

  __update_inv_weight(lw);

  if (unlikely(fact >> 32)) {
    while (fact >> 32) {
      fact >>= 1;
      shift--;
    }
  }

  fact = (u64)(u32)fact * lw->inv_weight;

  while (fact >> 32) {
    fact >>= 1;
    shift--;
  }

  return mul_u64_u32_shr(delta_exec, fact, shift);
}

static struct task_struct *task_of(struct sched_entity *se)
{
  return container_of(se, struct task_struct, se);
}

static struct rq *rq_of(struct cfs_rq *cfs_rq)
{
  return container_of(cfs_rq, struct rq, cfs);
}

static struct cfs_rq *cfs_rq_of(struct sched_entity *se)
{
  return se->cfs_rq;
}

static struct sched_entity *parent_entity(struct sched_entity *se)
{
  return NULL;
}

static u64 max_vruntime(u64 max_vruntime, u64 vruntime)
{
  s64 delta = (s64)(vruntime - max_vruntime);
  if (delta > 0)
    max_vruntime = vruntime;

  return max_vruntime;
}

static int entity_before(struct sched_entity *a,
				struct sched_entity *b)
{
  return (s64)(a->vruntime - b->vruntime) < 0;
}

static void __enqueue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
  struct rb_node **link = &cfs_rq->tasks_timeline.rb_root.rb_node;
  struct rb_node *parent = NULL;
  struct sched_entity *entry;
  bool leftmost = true;

  while (*link) {
    parent = *link;
    entry = rb_entry(parent, struct sched_entity, run_node);
    if (entity_before(se, entry)) {
      link = &parent->rb_left;
    } else {
      link = &parent->rb_right;
      leftmost = false;
    }
  }
  rb_link_node(&se->run_node, parent, link);
  rb_insert_color_cached(&se->run_node,
			 &cfs_rq->tasks_timeline, leftmost);
}

static u64 calc_delta_fair(u64 delta, struct sched_entity *se)
{
  if (unlikely(se->load.weight != NICE_0_LOAD))
    delta = __calc_delta(delta, NICE_0_LOAD, &se->load);

  return delta;
}

static u64 __sched_period(unsigned long nr_running)
{
  if (unlikely(nr_running > sched_nr_latency))
    return nr_running * sysctl_sched_min_granularity;
  else
    return sysctl_sched_latency;
}

static u64 sched_slice(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
  u64 slice = __sched_period(cfs_rq->nr_running + !se->on_rq);
  
  for_each_sched_entity(se) {
    struct load_weight *load;
    struct load_weight lw;

    cfs_rq = cfs_rq_of(se);
    load = &cfs_rq->load;

    if (unlikely(!se->on_rq)) {
      lw = cfs_rq->load;

      update_load_add(&lw, se->load.weight);
      load = &lw;
    }
    slice = __calc_delta(slice, se->load.weight, load);
  }
  return slice;
}

static u64 sched_vslice(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
  return calc_delta_fair(sched_slice(cfs_rq, se), se);
}

static void update_curr(struct cfs_rq *cfs_rq)
{
  struct sched_entity *curr = cfs_rq->curr;
  u64 now = rq_clock_task(rq_of(cfs_rq));
  u64 delta_exec;

  if (unlikely(!curr))
    return;
  delta_exec = now - curr->exec_start;
  if (unlikely((s64)delta_exec <= 0))
    return;
  curr->exec_start = now;
  curr->sum_exec_runtime += delta_exec;
}

static void update_stats_wait_start(struct cfs_rq *cfs_rq,
				    struct sched_entity *se)
{
  u64 wait_start, prev_wait_start;

  if (!schedstat_enabled())
    return;

  wait_start = rq_clock(rq_of(cfs_rq));
  prev_wait_start = schedstat_val(se->statistics.wait_start);

  if (entity_is_task(se) && task_on_rq_migrating(task_of(se)) &&
      likely(wait_start > prev_wait_start))
    wait_start -= prev_wait_start;

  __schedstat_set(se->statistics.wait_start, wait_start);
}

static void update_stats_enqueue_sleeper(struct cfs_rq *cfs_rq,
					 struct sched_entity *se)
{
  struct task_struct *tsk = NULL;
  u64 sleep_start, block_start;

  if (!schedstat_enabled())
    return;

  sleep_start = schedstat_val(se->statistics.sleep_start);
  block_start = schedstat_val(se->statistics.block_start);

  if (entity_is_task(se))
    tsk = task_of(se);

  if (sleep_start) {
    u64 delta = rq_clock(rq_of(cfs_rq)) - sleep_start;

    if ((s64)delta < 0)
      delta = 0;

    if (unlikely(delta > schedstat_val(se->statistics.sleep_max)))
      __schedstat_set(se->statistics.sleep_max, delta);

    __schedstat_set(se->statistics.sleep_start, 0);
    __schedstat_add(se->statistics.sum_sleep_runtime, delta);

    if (tsk) {
      account_scheduler_latency(tsk, delta >> 10, 1);
    }
  }
  if (block_start) {
    u64 delta = rq_clock(rq_of(cfs_rq)) - block_start;

    if ((s64)delta < 0)
      delta = 0;

    if (unlikely(delta > schedstat_val(se->statistics.block_max)))
      __schedstat_set(se->statistics.block_max, delta);

    __schedstat_set(se->statistics.block_start, 0);
    __schedstat_add(se->statistics.sum_sleep_runtime, delta);
    
    if (tsk) {
      if (tsk->in_iowait) {
	__schedstat_add(se->statistics.iowait_sum, delta);
	__schedstat_inc(se->statistics.iowait_count);
      }

      account_scheduler_latency(tsk, delta >> 10, 0);
    }
  }
}

static void update_stats_enqueue(struct cfs_rq *cfs_rq,
				 struct sched_entity *se, int flags)
{
  if (!schedstat_enabled())
    return;

  if (se != cfs_rq->curr)
    update_stats_wait_start(cfs_rq, se);

  if (flags & ENQUEUE_WAKEUP)
    update_stats_enqueue_sleeper(cfs_rq, se);
}

static void account_entity_enqueue(struct cfs_rq *cfs_rq,
				   struct sched_entity *se)
{
  update_load_add(&cfs_rq->load, se->load.weight);
  if (!parent_entity(se))
    update_load_add(&rq_of(cfs_rq)->load, se->load.weight);

  cfs_rq->nr_running++;
}

static void update_cfs_group(struct sched_entity *se)
{
}

static void cfs_rq_util_change(struct cfs_rq *cfs_rq, int flags)
{
  struct rq *rq = rq_of(cfs_rq);

  if (&rq->cfs == cfs_rq || (flags & SCHED_CPUFREQ_MIGRATION)) {
    cpufreq_update_util(rq, flags);
  }
}

static unsigned long _task_util_est(struct task_struct *p)
{
  struct util_est ue = READ_ONCE(p->se.avg.util_est);

  return max(ue.ewma, ue.enqueued);
}

static void util_est_enqueue(struct cfs_rq *cfs_rq,
			     struct task_struct *p)
{
  unsigned int enqueued;

  enqueued  = cfs_rq->avg.util_est.enqueued;
  enqueued += (_task_util_est(p) | UTIL_AVG_UNCHANGED);
}

static void check_spread(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
#ifdef CONFIG_SCHED_DEBUG
  s64 d = se->vruntime - cfs_rq->min_vruntime;

  if (d < 0)
    d = -d;

  if (d > 3*sysctl_sched_latency)
    schedstat_inc(cfs_rq->nr_spread_over);
#endif
}

#define UPDATE_TG	0x0
#define DO_ATTACH	0x0

static void update_load_avg(struct cfs_rq *cfs_rq, struct sched_entity *se,
			    int not_used1)
{
  cfs_rq_util_change(cfs_rq, 0);
}

static void place_entity(struct cfs_rq *cfs_rq, struct sched_entity *se,
			 int initial)
{
  u64 vruntime = cfs_rq->min_vruntime;

  if (initial && sched_feat(START_DEBIT))
    vruntime += sched_vslice(cfs_rq, se);

  if (!initial) {
    unsigned long thresh = sysctl_sched_latency;

    if (sched_feat(GENTLE_FAIR_SLEEPERS))
      thresh >>= 1;

    vruntime -= thresh;
  }

  se->vruntime = max_vruntime(se->vruntime, vruntime);
}

static void enqueue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se,
			   int flags)
{
  bool renorm = !(flags & ENQUEUE_WAKEUP) || (flags & ENQUEUE_MIGRATED);
  bool curr = cfs_rq->curr == se;

  if (renorm && curr)
    se->vruntime += cfs_rq->min_vruntime;

  update_curr(cfs_rq);
  if (renorm && !curr)
    se->vruntime += cfs_rq->min_vruntime;
  account_entity_enqueue(cfs_rq, se);
  if (flags & ENQUEUE_WAKEUP)
    place_entity(cfs_rq, se, 0);
  update_stats_enqueue(cfs_rq, se, flags);
  check_spread(cfs_rq, se);
  if (!curr)
    __enqueue_entity(cfs_rq, se);
  se->on_rq = 1;
}

static int cfs_rq_throttled(struct cfs_rq *cfs_rq)
{
  return 0;
}

void enqueue_task_fair(struct rq *rq, struct task_struct *p, int flags)
{
  struct cfs_rq *cfs_rq;
  struct sched_entity *se = &p->se;
  
  util_est_enqueue(&rq->cfs, p);

  if (p->in_iowait)
    cpufreq_update_util(rq, SCHED_CPUFREQ_IOWAIT);
  
  for_each_sched_entity(se) {
    if (se->on_rq)
      break;
    cfs_rq = cfs_rq_of(se);

    enqueue_entity(cfs_rq, se, flags);

    if (cfs_rq_throttled(cfs_rq))
      break;

    cfs_rq->h_nr_running++;
    flags = ENQUEUE_WAKEUP;
  }
  
  for_each_sched_entity(se) {    
    cfs_rq = cfs_rq_of(se);
    cfs_rq->h_nr_running++;

    if (cfs_rq_throttled(cfs_rq))
      break;
    update_load_avg(cfs_rq, se, UPDATE_TG);
    update_cfs_group(se);
  }
 
  if (!se){
    add_nr_running(rq, 1);
  }
}

MODULE_LICENSE("GPL");
