# Lab3



## Target

- print a page table
- create a per-process kernel page table
- simplify copyin/copinstr using the per-process kernel page table



## Explanation

### print a page table

We should print the page table following a specific format.

We should carefully watch how freewalk() in kernel/vm.c works to traverse a page table recursively(of course, it's also okay to traverse by loop). Then it's easy to print the whole page table.
