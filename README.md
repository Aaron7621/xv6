# Lab8



## Target

- Optimize the lock in freelist in kalloc.c
- Optimize the lock in buffer cache



## Explanation

### kalloc

- The original scenario is that multi-core CPUs share and maintain one linked list to manage free physical pages. When allocating physical pages it is single-threaded and serial. When allocation and collection operations are intensive, there will be fierce lock conflicts.
- Refinement of free physical page table locks: Divide a single free physical page table into a per-cpu linked list. Each cpu maintains its own free linked list, and each linked list has one lock. We split the big lock on a single linked list.
- When a CPU linked list is empty, it is necessary to traverse other CPU linked lists to find a free page table, and "steal" one from others' to its own linked list, so as to allocate it. Obviously, when traversing the linked list of other CPUs, we need to obtain the lock of the that linked list first.



### 😢Buffer cache

- The original scenario: there is a cache layer between the file system and memory, and bcache manages an array of buf objects. Each buf object maintains a cache (an char array, stores the date in its corresponding block), and cache-related information, such as the corresponding blockid, reference count, sleep lock (a buf cache can only be used by one thread at a time), etc.
  There is a big lock on bcache that manages all buf objects. When the buf object is requested externally or the buf object is released, the lock of the bcache must be acquired for allocation and reclamation operations.
- The purpose of this lab is to split the big lock of the bcache, and use the hash bucket strategy to map the blockid of different bufs to different hash buckets, so as to lock each bucket separately and improve the parallelism.
- The biggest difference between this experiment and the previous kalloc experiment is that kalloc's lock optimization simply splits the linked list that manages free pages, and each CPU manages a linked list. When each cpu needs to free or allocate pages, as long as its own linked list still has elements, it can return directly, and it does not need to care about the linked list of other cpus. In other words, each page is the same. There is no need to care about the pages of other CPUs.
  But the sharing of buffer cache is **real sharing**, the entire bcache object is a shared object, **can not be split into per-cpu objects**. The reason is that each buf object is unique and related to a blockid. Each cpu may have interest in the cache with any possible blockid.
- The general plan is: convert a doubly linked list maintained by bcache into a hash bucket - an array with a total of NBUCKET elements, each of which is the head node (can be a pseudo-head) of a bucket (stored in a linked list). node).
  - In initialization, put all bufs into the first bucket
  - When we need to get buf, first check whether there is a correct cache in the bucket corresponding to the blockid. If not, traverse **all bufs**, find the buf with reference count 0, and use LRU algorithm to evict it. If the bucket where the blockid of the buf is located is different from the bucket corresponding to the current blockid, delete the buf node from the original linked list and add it to the linked list of the current corresponding bucket.
  - Originally, the LRU's were always placed at the end, and the most recently used ones were placed at the header. After using the time counting method, the operation of releasing buf does not need to operate the linked list, but to update the last used time of buf and decrement the reference count.
- The tricky part of this lab is to correctly lock each bucket. Holding locks for too short can cause fatal errors. But increasing the locking window is likely to degrade the efficiency of the original large lock, or it may cause deadlock.



## Enlightenment

There are also many restrictions on the test thread in the test program of the Buffer cache task, which fully demonstrates that the fine-grained lock of this experiment is difficult to achieve without any concurrency errors. 

In fact, to me, the biggest feeling after this experiment is that the optimization is not better than the original big lock. . . Because there are only 30 buf caches totally. 

In the process of allocation and recycling, the conflict is not likely to be very large. But the original bigger lock can reduce a lot of the possibility of concurrency bugs.

Reducing the grain of a lock is sometimes very hard... like the second lab.