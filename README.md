# Lab3



## Target

- print a page table
- create a per-process kernel page table
- simplify copyin/copinstr using the per-process kernel page table



## Explanation

### print a page table

We should print the page table following a specific format.

We should carefully watch how freewalk() in kernel/vm.c works to traverse a page table recursively(of course, it's also okay to traverse by loop). Then it's easy to print the whole page table.

### per-process kernel page table

We are going to add a kernel page table to every process to simply some operation later. To do this:

1. Firstly add a new variable in struct proc, to save its own kernel page table
2. We imitate the way that creating a kernel page table, write a function that returns a page table like the kernel page table, except for the process' kernel stack. Because this new kernel page table is belong to one process, there is no need for it to map all the processes' kernel stack.
3. When we allocate a new process (make it from "unused" to "runable") in kernel/proc.c/allocproc(), we also allocate its per-process page table in its struct proc. And don't forget to map the kernel stack into its kernel page table.
4.  And modify the scheduler() in kernel/proc.c. When the kernel is running in scheduler(), it is running in the kernel scheduler thread using a special stack. After swtch() the thread will be switched into the process' kernel thread. So we should change the current page table into the process' own kernel page table before running swtch().
5. When we are freeing a process in freeproc() in kernel/proc.c, we should release its own kernel page table too. To free this page table, we need to release all the sub-page-tables and . But we mustn't free that its leaves' nodes point to. Because all those pages are where the kernel locate. And of course we should free the per-process kernel table itself.
