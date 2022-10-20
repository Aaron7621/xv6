# Lab5



## Target

Implement lazy allocation for sbrk() system call. Pass all the test cases.



## Explanation

The way to implement lazy allocation:

- When sbrk() is called, we just modify the process' sz variable, instead of allocating the pages for it immediately.
- When trap happen, we can recognize page fault by reading scause register. Then validate if the visual address of the page fault, by checking if the va is larger than sz or smaller than user stack.
- If the visual address is Okay, we allocate a new physical page and map the pa with va in user's pagetable.

Other adaptations in xv6 we have to do to support the lazy allocation

-  uvmunmap(): Beforehand we traverse the every va in a specific range in the page table. If there are any va that cannot be accessed, or its corresponding pte is not valid, the xv6 panic because there must be something wrong. 
  But now those cases are Okay. Because if we do lazy allocation, it's not a problem that encounter some va without any mapping in the page table.
-  The same as the uvmcopy(). When we copy a certain user space by its page table, it's okay that there are some va in the correct range but without any mappings. Just ignore it. This function will be used in fork().
- When sbrk() is passed by a negative number, it means we have to decrease the address space. We can safely call uvmdealloc(). Because it will finally use uvmunmap() to release the page table and the corresponding pages. If those pages have not been allocated, uvmunmap() will ignore it and not cause any problems.
