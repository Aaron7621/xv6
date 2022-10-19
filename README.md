# Lab2



## Target

- System call implementation: trace()
- System call implementation: sysinfo()




## Explanation
### trace()

We create a new entry function trace() in user level, and the actual implementation of this system call is the function sys_trace() in kernel/sysproc.c

When trace() is called, we trap into kernel and get the "mask", which is passed by the system call's argument and can be got in the process' trapframe. Then we save this mask in the process' own struct. So we should add a new variance "mask" in the struct proc. That's all we can do when trace() is called.

To really track all the system calls that has been included in mask, we should change distributing function of all the system calls, that is the syscall() in kernel/syscall.c

Because to run a system call function, it should firstly run the distribution function. We can get the mask there and compare to the system call that is about to run. If it is included in mask, we just print an information of this system call. 

### sysinfo()

Firstly follow the same way as the former task to create a system call.

We need to implement a function: it should get the number of free pages and unused processes. Then create a struct sysinfo (kernel/sysinfo.h) with the above information. Finally we copyout this struct to the user space. The destination address is provided by the argument in system call.

So we create a function in kernel/kalloc.c, named free_mem_bytes(). We traverse the pages' freelist and sum up the bytes of them. Then create another function in kernel/proc.c, named proc_not_unused(). Traverse the processes' array and calculate all processes in "UNUSED" state.



## Enlightenment

### What does exec() do?

The exec() is a shell program as well as a system call. The way it works show us a full picture of how system call runs. Take the init program as an example:

- Pass the init's address to a0, the argv's address to a1, and the system call number of exec() to a7.
- Then ecall. Ecall comes to the distribution function syscall() of the kernel, executes the corresponding system call. This time is exec, according to the value in a7, which is what we passed in user level.
- Then the exec() function in kernel obtains the system call's arguments through the some functions in syscall.c. These function actually take the value from a0-a5 registers. 
- So we get the system call's argument from a0-a5, and the system call's number in a7. These are all we need to implement a system call after trapping into kernel mode.

