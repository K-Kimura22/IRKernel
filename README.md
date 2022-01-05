# IRKernel
IRKernel operates as a kernel module. Then, a failure inside the system is detected and recovery is performed.<br>
It wants to use kernel functions and variables, but it is restricted from the kernel module and requires minimal changes.<br>
The following are implemented as the current implementation.<br>
### Detection
・Memory leak<br>
・Fork<br>
### Recovery
・Simple signal mechanism<br>
・Simple scheduling mechanism<br>
# Requirements
### OS
・Linux 4.18
###  EXPORT Function
・si_swapinfo (mm/swapfile.c)<br>
・oom_badness (mm/oom_kill.c)<br>
###  EXPORT Variable
・total_swap_pages (mm / swapfile.c)<br>
・runqueues (kernel / sched / core.c)<br>

# How to
Download IRKernel
`cd IRKernel`
`make`
`insmod system.ko`
If you want to change the detection or recovery settings, change the contents of systemcall.h.