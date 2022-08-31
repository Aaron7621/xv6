## COW Lab

- 一直在mappages那里报remap。后面竟然发现是**mappage()**的问题。函数的逻辑是，如果传入>=PGSIZE的sz，会执行>1次的map循环（但是很多其他函数对它的调用都是直接传PGSIZE作为sz的，相当于加了个保护页？），因为里面的循环相当于是do while。如果只需要map一页，只传1就好

- memmove那里一直报kerneltrap：copyout打错函数名了。。调了两次cowmake，本来是一次cowcheck+一次cowmake。

- 一直sbrk fail：在进程free COW只读页的时候，虽然说只有page引用计数为1的才要free，但是引用计数还不为0的页被调kfree时，也必须减去引用计数才行。

  减引用计数的操作并不只在COW page fault发生的时候。有时候有些进程拿着很多页而直接free掉，例如exec+fork，这样就是没有发生任何page fault情况下，页引用计数减少的情况

- acquire panic：死锁。例如

  - fun2() {

    acquire(&lock);

    do sth

    release(&lock);

    }

  - fun1() {

    acquire(&lock);

    fun2();

    release(&lock);

    }

  这样就死锁了

- 内存泄漏：没有正确加锁。在给COW page fault返回实复制物理页的函数get_pa_decre_ref里。因为之前该函数放在了trap模块中，而pa的引用计数数组在kalloc中。所以分配page fault的时候要通过掉kalloc里面的函数完成页引用的修改。
  问题就是出在“获得引用计数”的这个函数调用上。如果某进程A获得了某页的引用计数n（假设此时n = 2）后，马上有B进程获得了数组的锁，该进程也读出了n = 2，所以B进程创建多了一个page，添加到虚拟内存。当B进程释放锁后，轮到A进程，此时A进程的n仍然是2。所以A进程也创建了一个新的page。则此时就多创建了一个page，本应直接转换为实页的page泄漏。

  错误加锁导致内存泄漏，控制流示意图：

![内存泄漏示意](D:\GitHubLocalRepository\xv6\note\picture\内存泄漏示意.png)

​	正确加锁控制流：

![避免内存泄漏示意](D:\GitHubLocalRepository\xv6\note\picture\避免内存泄漏示意.png)

​	所以在需要边界区域，一定要保证操作的原子性，加锁不能分开加。



## MultiThread Lab

### Uthread

- 初始化的时候，thread context的sp应该指向thread -> stack数组的尾，也就是thread -> stack + STACK_SIZE。因为RISCV的函数调用/栈使用惯例就是栈从高地址往低地址扩展。栈底在栈的最高地址



## Locks Lab

### Memory Allocator

- 在kinit调用时只有一个cpu会进来，kinit和freerange的执行不需要担心多线程的问题

### Block Cache 

- 直接在binit里面kerneltrap了。原因是本来给每个哈希桶（链表）数组的头节点都存的是头节点的指针，初始化之后指针是0，不能直接对这这个指针进行p -> next。相当于空指针异常。但是xv6不会报详细的错误，只会trap掉

- 往hashmap的头节点.next写没数据。忽略了一点：结构体整个赋值是拷贝，而不是引用

  ![image-20220829214720984](C:\Users\Aaron\AppData\Roaming\Typora\typora-user-images\image-20220829214720984.png)

  这样写的话bcache[0].next是不会有变化的

- usertests的时候显示“freeing free block"：可能是桶锁在查找LRU和驱逐之间经历了释放

  解决：后来切到实验原分支lock下跑usertests也出现了freeing free block的报错。再切了几次不同分支之后突然就正常了。。。然后切回实验8分支make clean后再跑就过了。比较迷