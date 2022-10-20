# Lab7



## Target

Implement the copy-on-write fork.



## Explanation

The principle of COW fork is not difficult.

When we try to fork() a child process from a parent process, we do not allocate any pages. And we set all the PTE in parent's page table read-only, and mark them as COW page (by set a bit in PTE). Then copy a identical page table to child. 

When any processes are trying to write a certain page, it will trigger page fault. Then we allocate a new page, map a new record in page table, and make it writable.

While to implement, there are some other details have to pay attention.

- The first thing is that we have to save a counter array, to know the number of references that a physical page has. This is useful when we try to free a process, we have to know if all the pages its page table refers to can be freed or not.

- And we modify uvmcopy(). It is the function to copy user space when fork a child process. Obviously in COW mode, we don't need to allocate any pages in this function. Also, we should set all the PTEs in parent's page table only readable, and give them a mark to show that they are the COW page. This mark is useful when distinguishing a COW page or an invalid one when page fault is triggered.

  And we should add 1 to each pages' reference counter.

  Finally allocate a new page table, copy all the contents from parent's one.

- When allocate a new page in kalloc(), obviously we set its reference counter to 1.

- When page fault provokes a trap, we check if it is a COW valid page. If it is, we check the page's reference count. If the counter equals to 1, we do not need to allocate a new page. Just change the flag of its PTE. If the counter bigger than 1, we allocate a new page and copy all the content in the previous page. And decrease the previous page's counter by 1.
- When release a user space, we will finally call the kfree() function to free a page. So in COW mode, we need to decrease its counter first. If the counter is equal to 0 after decrement, we can safely give that page a real freedom by adding it into the freelist. If not, we don't need to do anything.
- Some adaptation should be done in copyout() too. If the destination address in user space is a COW page, we need to do the same as what we do when encountering a COW page fault.
- Almost there. We have to apply a lock to the reference counter array. Otherwise, when the multicore computer parallelly modify the count in this array, something incorrect may happen.
