# Lab7



## Target

- Implement UThread: a user-level multi-threading system.
- Exploring the Unix pthread threading library, and solve the concurrent error.
- Implement barrier()



## Explanation

### UThread

This is a thread switching program in user mode, or a cooperative thread scheduling - each thread voluntarily gives up(yield) the CPU. ⭐In fact, in real world, this kind of thread has another name - coroutine.

This is a user process, although it will also be interrupted, even running on a different CPU. But in xv6, the threads of the user process in user mode are all run serially. Although now a user process has multiple user threads, they will not run parallelly. **So there is no need to consider the issue of locks**.

The multi-threading implemented now, in addition to they cannot run concurrently on multiple cores, they ARE multi-threading to some degree - the cost of thread switching is very small (no need trap into the kernel) the address space of the process is shared between all the Uthreads.

Implementation

- When initializing each thread, write the thread's address to context.ra, the stack address (the end address of the array) to context.sp, and the rest of the callee saved registers (in context array) can be initialized to zero.
- Call u_swtch in scheduler()
- Switch the context in u_swtch (switch ra, stack, callee saved registers)



### Soving the concurrent error

The composition of the hash table: A total of 5 buckets. Each bucket stores the head node of a linked list of entry struct pointers. So the hash table is an array of size 5, and each element of the array is the head node of a *entry linked list

When adding a key to the hash table, first find the bucket (linked list) where the key belongs to, and then insert it into the linked list by head insertion.
The steps of the head insertion: define a linked list node e, e -> next = head, head = e.

The reason for the lack of keys in multi-threading put: When the nodes put by two threads belong to the same bucket, the next pointer of two nodes e1 and e2 may point to the head node of the linked list at the same time, and then the head node of the linked list will eventually be one of e1 and e2. If it is e1, then e2 can never be obtained from the process of traversing the head node of the new linked list, that is, it is lost.

We can lock the entire hash table when modifying, or lock by bucket.

### Barrier

Each thread sleeps in the the same condition, until the last thread reaches. When it happens, it resets the barrier and wakes up all the threads.

This experiment is to pave the way for the sleep-wakeup mechanism in the next section. 

The core question is:

* Why does the pthread_cond_wait() primitive need to pass a lock as its parameter? 



## Enlightenment

The UThread here is actually the so-called [coroutine](https://en.wikipedia.org/wiki/Coroutine). The main different between coroutine and thread in real world is that: coroutines are **cooperative multitasked**, and they provide **concurrency but not parallelism**. 

Coroutines are run at user level and it's very fast to switch them(no need to trap into kernel). But because of that, different coroutines cannot allocated by different CPU core and realize the real parallelism. 
