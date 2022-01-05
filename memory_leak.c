#include <linux/module.h>
#include <linux/mm.h>
#include <linux/sched/task.h>
#include <linux/nsproxy.h>
#include "systemcall.h"

static int proc_oom_score(struct pid_namespace *ns, struct task_struct *task)
{
  unsigned long totalpages = totalram_pages + *total_swap_pages;
  unsigned long points = 0;

  points = oom_badness(task, NULL, NULL, totalpages) * 1000 / totalpages;
  return points;
}

struct task_struct *memory_leak_process(void){
  struct task_struct *p = &init_task;
  struct task_struct *badness_task = &init_task;
  int badness_score = 0;
  do{
    int score = proc_oom_score(p->nsproxy->pid_ns_for_children, p);
    if(score > badness_score){
      badness_task = p;
      badness_score = score;
    }
    p = list_entry(p->tasks.next, struct task_struct, tasks);
  }while(p != &init_task);

  return badness_task;
}

static int memory_leak_search(void){
  struct sysinfo system_info;
  unsigned long total, free;
  int score;

  si_meminfo(&system_info);
  si_swapinfo(&system_info);
  total = system_info.totalram + system_info.totalswap;
  free = system_info.freeram + system_info.freeswap;
  score =  (total - free) * 100 / total;

  return score;
}

/**/
void memory_leak(void){
  struct task_struct *p = &init_task;
  int score = memory_leak_search();
  
  if(score > Memory_Leak){
    p = memory_leak_process();    
    sig_kill(p);
  }
}

MODULE_LICENSE("GPL");
