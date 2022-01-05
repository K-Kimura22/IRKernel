#include <linux/module.h>
#include <linux/init.h>
#include <linux/sysfs.h>
#include <linux/kernel.h>

#include "systemcall.h"

int x=0;
static struct timer_list timer;

/*Callback function*/
static void callback(struct timer_list *t)
{
  switch(x){
  case 0:
    memory_leak();
    break;
  case 1:
    fork_max();
    break;
  default:
    printk("NO-ERROR");
  }
  
  x++;
  x = x%NUMBER;
  mod_timer(&timer, jiffies + msecs_to_jiffies(TIME));
}

/*Initial function*/
static int __init systemcall_init(void)
{
  printk("START\n");

  timer_setup(&timer, callback, 0);
  mod_timer(&timer, jiffies + msecs_to_jiffies(TIME));

  return 0;
}

/*End function*/
static void __exit systemcall_exit(void)
{
  printk("FINISH\n");
  
  del_timer(&timer);
}

module_init(systemcall_init);
module_exit(systemcall_exit);

MODULE_LICENSE("GPL");
