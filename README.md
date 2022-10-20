# Lab4



## Target

- Understand RISC-V assembly
- Print the backtrace of the function called
- Set an user function "alarm". When a process occupy n ticks of CPU, the alarm function will be called.



## Explanation

### backtrace

We are going to print all the return address in each stack frame under the current stack.

Firstly we call the r_fp() function to read the frame pointer register and get the current(top) frame pointer.

To the top frame we get its RA from fp + 8. To access the previous frame, we get the previous fp from fp + 16, which store the address of previous fp. 

All of these are the convention of how RISCV's stack works when calling the function.

### Alarm

This task actually shows how a user-level trap handler works. 

In this design, the user set an trap handler for its program at first. When the process trap into kernel, the kernel will return to the handler function in user level, instead of the place where the process trapped. After running the handler function in user level, it finally call a system call "return". Then the process trap into kernel again and finally the kernel return to the place where the process trapped at the first time. It is one way to implement the user-level trap handler.

There are two times when the process trap into the kernel. We need to define another trapframe to save the trapframe at the first time, named old_tf. Because in the second trap we want our p->trapframe the same as the first-time's trapframe before we return to user level, but the current trapframe is not the same because it may be modified when running the handler function.

Also we need to save some other variable in struct proc to support the "alarm" function, like the ticks when the alarm function called and count the ticks a process has already been used, the address of the handler function, and a flag to mark if the process is currently running a trap handler function. Because we won't count the ticks or call the handler function when the handler function itself is running.

- When sys_alarm() is called, all it needs to do is "setting". It set the handler's address and the ticks in struct proc.
- When the process is interrupted by clock interruption, we sum up the ticks it uses. If it has been equal to the ticks that trigger the alarm function, we save the current trapframe by copying it to the old_tf. And we set the epc register to the address of the handler function.
- Then after the kernel return to user level, it will start running right at handler function, because of the sepc's setting before we execute sret.
- The handler function will finally call the system call sys_return(). In sys_return(), we exchange the current trapframe by old_tf, then return to user level. The process will right be at the point where it originally trapped.
