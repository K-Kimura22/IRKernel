# IRKernel
IRKernel operates as a kernel module. Then, a failure inside the system is detected and recovery is performed.<br>
It wants to use kernel functions and variables, but it is restricted from the kernel module and requires minimal changes.<br>
<br>
The following are implemented as the current implementation.<br>
## Detection
・ Memory leak<br>
・ Fork<br>
## Recovery
・ Simple signal mechanism<br>
・ Simple scheduling mechanism<br>
# Requirement
