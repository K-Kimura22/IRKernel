#include <linux/module.h>
#include <linux/sched/task.h>
#include "systemcall.h"

char max_process[MAX_STRING];

static int process_number(char process[]){
  struct task_struct *p;
  int i=0;
  
  p = &init_task;
  do{
    if(strcmp(process,p->comm)==0){
      i++;
    }
    p = list_entry(p->tasks.next, struct task_struct, tasks);
  }while(p != &init_task);
  
  return i;
}

static int fork_search(void){
  struct task_struct *p;
  int i,max=0;

  p = &init_task;
  do{
    i = process_number(p->comm);
    
    if(max<i){
      max=i;
      strcpy(max_process, p->comm);
    }
    p = list_entry(p->tasks.next, struct task_struct, tasks);
  }while(p != &init_task);

  return i;
}

void fork_max(void){
  int score = fork_search();
  if(score>=MAX_NUMBER){
    name_sig_kill(max_process);
  } 
}

MODULE_LICENSE("GPL");
