# 分模块核心代码详解 | Core code explanation

# 简介 Introduction

本文件记录了本人对XV6各模块核心代码的解释、理解。
英文版正在路上...
This file records my comprehension and explanation to the core logics of different modules in XV6.
The English version will be coming soon...

# 启动

1. entry.S：整个内核的入口，通过kernel.ls将其连接到0x80000000的位置，也就是qemu执行首条指令的地址。随后程序为C内核设置一个最开始的栈
2. 跳转到start.c/start()，主要完成从机器模式到监管者模式的切换，初始化时钟中断
3. 跳转到main.c/main()。main()执行几乎所有模块的初始化和启动。

   多核CPU的初始化是这样的：

   1. CPU0完成大部分模块的初始化，包括第一个用户程序userinit的拉起。在初始化完成，此时内核在单CPU下也能运行，把start标志置为1.
   2. 其余所有CPU在CPU0初始化的过程中一直自旋等待，直到start为1. 则各cpu初始化一些per cpu的程序（以hart结尾的初始化程序），这些程序初始化的是每个CPU上都有的硬件。（显然在CPU0初始化的时候这些程序也被CPU0调用的）
   3. 然后所有CPU调用scheduler，进入各CPU的内核调度线程
4. 各模块初始化完毕后，main()调用userinit()初始化第一个程序。userinit()实际执行了数组initcode中的二进制指令，对应user/initcode.S中的代码，其执行了一个exec系统调用，exec了init用户程序。init通过fork+exec启动shell。
5. shell：父进程一直等待，如果子进程退出，父进程重启shell。

*疑问：在trap初始化时是把kernelvec写进stvec，作为陷入的入口。而把uservec写入stvec的操作发生在usertrapret中。在内核启动、第一次返回用户空间的时候应该会调用usertrapret才对，不然怎么调用sret返回、以及怎么写好uservec以供后面用户程序系统调用呢？*

*但是没有找到内核第一次返回用户空间时具体发生在哪。在main()执行完userinit()，初始化了第一个进程后，main()最后调用了scheduler，scheduler中调了swtch.S进行任务切换。但是swtch.S中并没有sret*

*回答：在”线程切换“部分，才能找到这个问题的真正解答*



# 内存

## riscv.h

包含页表操作时用到的几个宏

PGSIZE - 页表字节数：4096

PGSHIFT - PTE偏移量的位数：12。即每个页表的”页内地址“，因为一页表4096字节，所以用12位访问。

PXSHIFT(level)  - 给出多级页表当前需要的层数，返回获得当前级页表的PTE时，当前地址需要右移的位数。

PX(level, va) - 根据虚拟地址和当前层数，获得在PTE在当前页表的地址（偏移量），一共9位（因为一页有512 =  2^9）个PTE

## kernel/vm.c

主要是和页表操作相关的代码。

1. walk()：获取va对应的地址所在的物理页表地址，也就是第三级页表的PTE。

   i. alloc参数：一级页表（根页表）是一定存在的，存在SATP中。而如果alloc == 1，在walk的过程中二、三级页表不存在则创建二、三级页表。如果二三级页表不存在，且alloc == 0或页表创建时内存分配失败，返回0。意味着无法访问va对应pa所在的物理页号。

   最后返回三级页表中的PTE，此PTE保存了va所对应pa所在的物理页。此时的PTE只是“找到对应的”，但是并未检查合法性和其中的值，可能是空的（新分配）。

2. mappages：接收一段不一定对齐的虚拟地址与一个物理地址，及要映射的范围，把范围内所有虚拟页所对应的物理页都映射页表中，返回-1如果创建多级页表时资源分配失败。这个函数只涉及页表映射，不涉及实际资源的分配，即要**先分配了资源，获得了pa，才能调用这个函数将映射写到页表**

3. uvmalloc

   1. 参数：页表、旧的size，新的size
   2. 如果新size大于旧size而且跨页了，那么分配所有包含的新的页。如果某一新页分配失败了，调用uvmdealloc将之前分配的新的页全部回收并返回；如果某一新页分配成功但是也表映射失败了，把那一页回收，同时把之前分配成功的新的页页全部回收并返回。

4. uvmdealloc：传入oldsz和newsz，调用uvmunmap回收页。映射的页表项以及具体分配的页都会被回收。

5. uvmunmap：解除一段连续的虚拟地址范围在页表中的叶子映射并且释放掉相应的物理资源。根据va是可以获得pa的，所以可以在这个函数中完成**删映射+释放物理页**。也可以通过参数指定不释放掉相应的物理页，只删除虚拟地址范围对应的PTE。

6. freewalk：在三级页表的PTE已经全部删除之后才能使用。PTE指向真正物理内存页（而不是下一级页表）的PTE必须已经清除。该函数会将一、二、三级页表的映射以及页表本身全部清除、释放。

7. uvmfree：先调用uvmunmap删除掉一段虚拟地址范围的【叶子PTE】与【对应PTE的物理页】，再通过freewalk删掉页表叶子节点以外的所有PTE。这个函数可以把用户地址空间全部回收。



# 陷入

## 相关寄存器

stvec：内核的中断处理函数的地址。Trap之后把它写入pc
sepc：trap完之后的恢复地址，sret之后sepc写入pc
scause：trap原因描述
sscratch：在中断时可作为缓冲使用
sstatus：SIE位标志设备中断是否启用。如果内核清空SIE，推迟设备中断。SPP位指示中断发生时（前）的特权级，并控制sret返回的模式。

## kernel/trap.c

1. usertrap：
     a. 写kernelvec到stvec，让来自内核的陷入可以跳转到该处

     b. 把sepc寄存器的值存到当前进程的trapframe中。这个值来源于在ecall的时硬件自动保存当时的pc，以让ecall结束后返回用户代码。此处第二次将这值保存到内存是因为usertrap中可能会发生的进程切换，覆盖原有sepc
     c. 根据scause处理陷入。如果scause==8，即系统调用。把trapfrmae中保存的sepc加4,即用户程序ecall后的下条指令。然后调用syscall系统调用分发函数。如果是设备中断，则调devintr()处理。处理完后如果是时钟中断，则让出cpu。

     d. 系统调用返回，则调usertrapret

2. usertrapret：
     a. 把stvec设置回uservec的地址。这里大概可以推断出，来自内核的陷入应该只会发生在usertrapvec到usertrapret之间？或者说一定发生在是在用户陷入的基础之上？
     b. 设置好trapframe中的内核页表、内核栈、usertrap地址、hartid等。这些字段在uservec保存好用户寄存器后，跳转到内核陷入处理函数时需要使用.

     *疑问：trapframe中的这些字段按理来说不会被修改，这样设置是只有第一次返回用户空间的时候有用吗？*
     c. 设置好sepc寄存器为之前保存的（ecall下一行指令的位置）；
     b. 获取（还没设置）satp为用户页表。trapframe中没有保存用户页表的地址，所以需要在这里通过保存在进程结构体中的p->pagetable，作为参数传给userret。
     d. 获取userret地址，传入trapframe与用户页表，调用跳转至userret

## kernel/trampoline.S

此处保存了用户陷入时执行的第一个函数uservec以及内核返回用户空间执行的最后一个函数userret

  1. uservec

  a. 在trap.c中把uservec装进stvec，用户陷入时首先跳转到这里。

  b. 从sscratch获得当前进程的trapframe。sscratch与a0交换使a0可以暂时使用。通过a0地址保存32个当前的寄存器的值（包括存在sscratch中的a0）。然后从trapframe中加载内核栈到sp、加载usertrap地址到t0、加载内核页表到t1并写入satp，然后jr到t0。
  **注意：**uservec的注释中，在切内核页表后写道，“a0此时不再valid”，指a0寄存器的值本来存的是（用户地址空间下的）trapframe，但是内核页表中没有每一个进程的trapframe的映射，所以a0此时的值是没有意义的。

  2. userret

a.  切用户页表（为了使用trapframe的内容，需要用户页表才有映射）
b.  根据入参，此时a0是trapframe地址，a1是用户页表（文档说反了）。所以恢复32个通用寄存器的时候需要用到a0的地址，但同时又要恢复a0本身。因此过程是：先把tramframe中保存了的a0值（即ecall前的a0）写入sscratch，然后用当前a0（即作为入参、正指向trapframe的a0寄存器）从trapframe恢复除了a0的寄存器，最后当前a0与sscratch交换。所以sscratch此时同时保存了进程trapframe的地址，下次陷入时可以再使用。
c. 调用sret返回sepc的地址，恢复陷入前的特权级。



# 系统调用

## user/usys.pl  &  user/usys.S

1. usys.pl是一个生成usys.S的脚本
2. usys.S中定义了**用户层面**的系统调用接口（函数），所有系统调用都只做两件事
   1. 把当前对应的系统调用号写入a7寄存器中
   2. 调用ecall

## kernel/syscall.h & kernel/syscall.c

1. syscall.h: 定义了所有系统调用的调用号，后续内核处理不同系统调用时根据这个号进行分发
2. syscall.c：ecall之后，根据中断处理函数usertrap，属于系统调用的中断来到syscall.c/syscall()中。syscall()从p->trapframe -> a7获得系统调用号，因为在中断时用户传入a7的系统调用号最终会在中断处理后存到进程的p -> trapframe中。
3. 同理，syscall.c中的获取用户实参的函数argraw()，也是根据p -> trapframe中保存的a0、a1获得用户系统调用的传入参数。

*疑问：在uservec中提到切换内核页表后，由于没有进程trapframe的映射，存放trapframe地址的a0不再有用。那么为什么内核可以通过p -> trapframe访问trapframe呢？*
*因为a0存的是trapframe在用户空间中的虚拟地址。而p -> trapframe存的是trapframe的物理地址。内核使用恒等映射，因此可以直接通过物理地址访问trapframe*



# 中断

## 控制台输入

1. main函数最开始调用console.c/consoleinit()。
   1. consoleinit()调uartinit()，配置了uart硬件：UART之后对接收的每个字节输入生成一个接收中断，对发送完的每个字节输出一个发送完成中断。具体操作是为部分uart寄存器设了值
   2. 把consoleread和consolewrite函数指针写到了一个全局数组中，可以被read和write系统调用获取
2. 对控制台输入文件描述符的read系统调用，控制流最终走向console.c/consoleread()。consoleread()主要工作是获得cons（console.c中的一个全局的结构体对象，主要是维护了一个缓冲区，用于与中断设备交互）的锁后，对锁进行sleep，等待后续被中断唤醒。这一步就是中断处理的上半部分。
3. 当用户输入一个字符，uart发送一个中断，trap通过sscause判断当前陷入是来自中断，调用trap模块中的中断处理devintr()。devintr()通过PLIC获得当前中断设备，如果设备是uart，调用uartintr()。
4. uartintr通过uartgetc()从uart硬件读取等待输入（到程序的）字符，并交给consoleintr处理。uartgetc()具体通过LSR寄存器判断字符是否ready后，读RHR寄存器中的字符。
5. consoleintr将字符在cons.buf缓冲区累积，对部分字符对特殊处理（如backspace）。当读到换行符后，则唤醒consoleread。consoleread再将cons.buf缓冲区中的字符copyout到用户空间所在地址。3-5步就是中断处理的下半部分。



## 控制台输出

1. 对写控制台的write，最终调用到console.c/consolewirte()。该函数把write的字符串从用户空间copyin进来后，对各字符调uartputc()
2. uartputc负责将传入字符写到输出缓冲区，然后调用uartstart()就返回。uartputc()不会等待输出的完成，只会在缓冲区满的时候等待。1、2是中断的上半部分
3. uartstart()介于输出中断处理上下两部分之间，它既被上半部分的uartputc调用，也在中断发生时被调用。
   1. uartputc()每写一个字符到缓冲区都会调一次uartstart()。而uartstart的返回（退出）是不确定的。缓冲区写完了uartstart会退出（这是必然的），而THR（即接收输出字符的寄存器）还没有准备好时也会直接退出。如果THR总能在要写下一个字符的时候ready，uartputc()就把缓冲区写完为止。
   2. 当uart发送完一个字节，也会产生发送完成中断。uartintr调用uartstart，完成与上一步一样的工作。这属于中断处理的下半部分。
      之所以同样的操作在中断的上下部分都进行，是因为如果uartputc要写下一个字符的时候THR没准备好，最终缓冲区的字符就不能完整输出。所以在中断发生后（因为这是上一个字符传输完成后产生的中断，此时THR一定ready了）要继续调一次uartstart()。有了中断后半部分的调用，哪怕uartstart()每次都只能成功写一个字符到THR，也能保证整个缓冲区写完。而uartputc()对uartstart()的调用则是“启动”了写的过程。不然后面也不会产生传输完成的中断。



输入缓冲区的存在可以在即使没有进程等待输入时，控制台驱动也可以处理输入，而后续进程的读取也可以获得这些输入。输出缓冲区的存在可以让进程无需等待就可以发送输出。
缓冲区的存在使得进程与设备IO完成了解耦，提高了并发。驱动处理的“两步走”就是围绕缓冲区进行的。



# 线程切换

## 切出

### trap.c

1. 用户线程因为时钟中断（或其主动放弃cpu），触发trap，保存好用户线程的trapframe，进入内核。此时的控制流为用户程序的内核线程（因为使用的是用户内核栈）。
2. 如果是时钟中断，调用proc.c/yield()。

### proc.c

1. yield()：让出cpu的函数。首先获取当前进程的进程锁（为了保证当前正在让出cpu的操作原子性，不会再这个过程中被其他cpu调度），修改进程的状态从RUNNING到RUNNABLE。然后调用sched()
2. sched()：在线程“切出”这一步中的swtch函数调用方。在对进程做了一系列检查后，调用swtch()函数。因此此时进入swtch函数后，**RA寄存器的地址就是sched**（因为sched是swtch的调用方）。这对后面理解切入的过程很重要。
3. swtch()：swtch.S/swtch()主要将线程（**当前还是用户的内核线程**）的ra（即sched的地址）和sp（即用户的内核栈地址）、以及callee saved寄存器保存在进程的context中（p -> context），用于下次切入时使用。然后把当前cpu的context（保存了内核调度线程的sp、ra和callee saved寄存器）load到相应寄存器中
4. 随后根据cpu context load进来的ra，和sp，完成换栈（内核调度线程栈，每cpu一个）。swtch函数结束后根据ra返回到proc.c/scheduler()函数。返回的不是调用当前swtch的sched，而是本cpu上一次调用swtch的地方（也就是scheduler()）。这是线程切换最核心的地方

## 切入

### proc.c

1. 切入一个用户进程，发生在proc.c/scheduler()中。scheduler()运行在内核调度器线程中。当一个进程经历用户线程-内核线程-内核调度器线程（上文所述的过程）切出后，cpu的返回到scheduler()里的swtch()中。cpu首先释放了进程的锁，这个锁在yield获取，在scheduler释放，用来确保进程进行切出时其他cpu无法调度该进程。现在释放之后该进程就可以被其他cpu进行调度了。
2. 随后scheduler遍历所有进程，找到RUNNABLE状态的进程，并获得进程锁。将其标为RUNNING，然后调swtch**切入该进程的内核线程**
3. swtch把当前cpu内核调度线程保存在cpu->context中（所以本cpu之后在运行切入时就可以从自己的cpu对象中找到找到内核调度线程的上下文了），把要调度的进程的内核线程上下文load进响应的寄存器。
   然后用ret指令返回。因为此时的RA是p->context中的ra，也就是该进程的内核线程上次切出、在sched调用swtch时保存的ra，所以**现在ret指令后将返回到上次该进程的内核线程调用swtch切出的地方**，也就是sched()，而不是scheduler
4. 此时cpu已经切换到了用户进程的内核线程。sp也指向进程的内核栈。swtch返回到sched()后，往下走再返回调用sched()的yield()中。在yield()中释放了进程的锁（释放的是在scheduler时获得的锁，目的是保障进程在切入时不会被其他cpu看见），然后返回调用yield的usertrap中
5. 执行usertrap调用yield之后的程序，也就是调用usertrapret()。之后的过程就和用户程序执行系统调用后返回一样。从trapframe中恢复用户线程的的上下文，最后sret回用户空间

在进程切换的过程中，最重要的一点就是围绕swtch函数的调用和返回。因为每次都在线程的context中保存了该线程调用swtch的ra，所以每次调用swtch后都会返回到另一个线程上次调用swtch的地方。

*为什么不需要管pc？*
*因为pc永远都会随着代码执行的每一行而变化。在swtch中保存的pc，也仅仅是swtch指令所在的地址而已，该pc并不能使一个线程切换至另一个线程。只有ra能保证线程的切换*

*为什么只需要保存callee saved寄存器？*
*因为swtch的调用就是一个普通函数的调用。而swtch完成的工作非常简单，并不需要依赖其他的上下文。我们只需要保证swtch的调用者在swtch返回时的上下文一致即可安全完成线程的切换。所以在swtch内部只需要保存和加载callee saved寄存器即可。其他caller saved的寄存器保存在swtch调用者的栈中，在swtch返回时自动恢复。这样就确保了线程调用的swtch返回后上下文一致*

*进程的锁*
*用户进程的锁，无论是切入还是切出过程，都是在两个不同的线程中获得/释放的。切出时，在用户的内核线程执行到yield时获得，在内核调度器线程中释放。切入时在内核调度器线程中获得，在用户内核线程的yield中释放。*
*在代码中（例如yield），直观看到的是获取了锁，调用了swtch，然后释放锁。然而实际上调用swtch之后返回的并不是yield的下一行，所以这把在yield中获得的进程锁并不是直观看到的在yield中释放的*
*这样做的目的都是确保进程切换的过程的串行，进程在此过程中不会被其他cpu看到*

## 用户线程的第一次切换

现在可以回答*启动*一小节时的问题了。当时的问题是：

> 第一个用户进程是怎么进入用户空间的？在刚启动时stvec寄存器写入的是kernelvec的地址，用户trap时运行的uservec函数的地址只用在usertrapret中才写到stvec中。但是整个启动过程并没有看到usertrapret的调用甚至sret的调用。

这是因为当时并不知道线程切换和swtch的神奇作用。

1. 首先在main.c/main()中调用了proc.c/userinit()。userinit中调用的allocproc()函数（包括fork的时候需要获得空闲进程的时候也会调allocproc），从空闲的进程中获取一个进程，对他进行一系列初始化，和线程切换有关的步骤包括

   1. 往p -> context中的ra写入forkret()所在地址
   2. 往p -> context中的sp写入该进程的用户内核栈地址

   随后userinit把该第一个进程状态写为RUNNABLE

2. main()在userinit后调用scheduler。scheduler中自然会把第一个进程找到，因为此时只有它是RUNNABLE。然后调swtch切入第一个进程的内核线程。

3. swtch返回的是forkret所在地址（因为allocproc中把forkret写到了p -> context的ra中。

4. **forkret最终调用usertrapret**，此后uservec地址写入stvec，然后像正常系统调用一样返回用户空间



# 锁

## 自旋锁 spinlock.c

- acquire()：传入自旋锁对象lk，获取自旋锁。其实就是把lk-locked写为1。如果当前已经lk -> locked已经为1，则一直自旋等待。
  - 但是有一种可能：有两个线程同时读到lk -> locked为0，便能同时获得锁。因此使用C提供的从硬件层面串行化的函数__sync_lock_test_and_set()，确保判断locked是否为0并赋值的过程是串行的。
  - 另一个问题是：在一个cpu获取了一把自旋锁到其释放之前，应该**关闭中断**。假设一个线程在已经获取锁的过程中发生了中断，另一个线程也要请求这个锁。如果此时只有一个cpu的话，而cpu又不出让给已经获得了锁的线程，那就是死锁了。
- release()：释放锁的操作。核心就是把lk->locked置为0。同样需要用到C提供的串行化函数。最后打开中断。

## 睡眠锁 sleeplock.c

- 睡眠锁的作用就是：在尝试获得睡眠锁失败后，进程会进入睡眠
- 睡眠锁的实现依赖自旋锁，
  - 从自旋锁的实现中，调用了硬件层面的串行方法，本质上该方法保护的就是sleep-wakeup中的condition，即__sync_lock_test_and_set()和condition lock保护的是同一样东西：就是判断是否睡眠/自旋应当和完成睡眠/自旋的操作原子化。
    自旋锁中的sync_lock_test_and_set()确保了判断locked为0和将locked赋为1是串行的。而condition lock确保判断condition成立和进入睡眠是串行的。
  - 所以一把睡眠锁首先就应该有一把condition lock自旋锁，这个自旋锁的目的就是用来串行地判断我（睡眠锁）是否已经被占用。从sleep-wakeup的角度，这把自旋锁是condition lock，避免lost wakeup；从锁的角度，这把自旋锁的存在相当于人为帮睡眠锁执行了一个类似sync_lock_test_and_set()工作。
- acquiresleep()：
  - 首先获得acquire当前睡眠锁lk的条件锁lk -> lk，这是为了保护之后判断当前睡眠锁是否已经被占用的这个条件。
  - 然后用while判断当前睡眠锁是否已经被占用，即lk -> locked == 1。
  - 如果已经被占用则sleep。首先sleep channel是睡眠锁lk自己（因为之后释放锁的函数也需要传入睡眠锁对象，所以可以通过锁（的地址）本身作为channel完成sleep和wakeup的通信），然后condition lock则是lk -> lk。
  - 所以，condition lock保护的condition就是该睡眠锁自己的locked变量，因此condition lock放在睡眠锁的成员变量中就可以了。
- releasesleep()：获取了条件锁lk->lk后，修改locked变量（这其实就是正式释放睡眠锁了，只不过现在拿着条件锁，别的进程想获取也还得等待，所以是安全的）。然后唤醒在睡眠锁channel睡眠的进程（如果有的话），最后释放条件锁

*思考：sleep和sleeplock的关系：*
*实现上：sleeplock依赖sleep-wakeup机制实现。sleeplock本身有一个locked的标志，依赖这个表示表明sleeplock是否被占用，维护这个标志+sleep=sleeplock。为了维护这个locked，sleeplock还需要带上一个spinlock。这个spinlock从锁的角度看是为了保证locked时的串行化，类似spinlock实现时的sync_lock_test_and_set。从sleep的角度看就是sleep-wakeup机制的condition lock，所以在sleep的时候需要把这把自旋锁，也就是condition lock传进去*
***使用场景上**：sleeplock本质是**锁**，而不是sleep。因此使用sleeplock的动机和使用自旋锁一样，是独占共享资源。和自旋锁不同的只是抢不到的进程进入睡眠，以及持有睡眠锁期间中断打开（而自旋锁是关闭）。*
*sleep的本质是**等待**，使用sleep**不是为了串行化某部分流程**以及保护某部分共享区域，而是当前进程要继续执行**不得不**等待其他进程的执行，例如缓冲区空/满情况下的读/写。为了不自旋等待所以sleep。使用sleep也需要加锁，但是这个锁是保障sleep机制必须有的条件锁，和sleep的使用动机无关。这有时候在看见sleep的使用时会对sleeplock的存在以及为什么此时不使用sleeplock产生一点confuse*
*显然sleep和sleeplock的使用都是为了避免自旋浪费cpu，但**动机不同**。**使用sleep的等待是“自愿”的**，例如消费者等生产者，而使用sleeplock是利用了sleep机制避免了锁冲突时的自旋等待，**sleeplock的等待是“被迫”的**，是为了争夺共享资源产生的“无奈的等待“，而利用sleep把这种不得不进行的等待转换为睡眠*

# 文件系统

## buffer cache

buffer cache层是文件系统中唯一和硬件（驱动层）打交道的。其余关于文件系统的调用都调用cache层的接口

- buf.h：定义了一个buffer cache的内容。
  - 主要是维护了一个data[BSIZE]数组（BSIZE是block size，即块的字节数），所以**一个buf就是一个block的缓存**。
  - 因此还有blockno和dev：本buf缓存内容的设备号和所属blockno。
  - 整个buffer cache是一个双向循环链表，每个buf都是链表中的节点，所以有\*prev和\*next指针用于指向链表中的前、后两个节点。
  - 一把sleeplock：一个buf一次只能被一个进程（线程）访问
- bio.c是整个buffer cache代码：
  - bcache：这个全局静态变量保存了所有的buf对象。
    - buf[NBUF]数组是为了初始化方便，自动有NBUF个buf对象定义在里面了，初始化完成后不会再从该数组中访问buf。
    - head是所有buf组成的双向循环链表的头节点。定义双向循环链表是为了LRU的实现
    - 一把自旋锁，在调整buf链表时使用使用。锁住整个bcache（其实就是为了保护链表）
  - binit()：初始化bcache。主要是初始化了每个buf的锁、bcache锁、以及构建buf链表
  - bget()：传入设备名、blockno，返回对应block的带锁的buf。这是读磁盘中最核心的函数。
    - 首先遍历一次buf链表，如果有对应buf已经符合给定blockno和dev的，修改引用计数后返回即可。
    - 如果没有，说明该块还没有被缓存，则需要驱逐LRU的buf。此时链表从后往前遍历，只要找到第一个遇到的引用计数为0的，则把它驱逐出缓存。具体做法是修改该buf的blockno和dev。然后修改引用计数与buf的valid成员变量为0（bget调用者会处理）。返回此buf
    - 如果两次遍历都没能返回任何一个buf，说明当前所有buf都没有命中且全部为忙（即引用计数都不为0），直接panic掉
  - brelse()：要求buf使用者操作完buf后要调用此函数。主要工作是释放掉buf的睡眠锁（根据睡眠锁的实现，这也会同时唤醒正在等待本锁的进程）。
    并为buf的引用计数-1.若引用计数为0，则把当前buf移到链表头，表明这是刚使用完的。其实这是一个有效的偷懒的做法，逻辑上来说每次被释放都应该把buf放到链表头才对，但引用计数不为0时的buf永远不会被驱逐。所以大可以放心地在引用计数为0时再移当前buf
  - bread()：buffer cache层对外的读接口，传入dev和blockno，获得带锁的buf。外部读文件系统的内容都是通过此接口。
    实现：通过bget获得buf对象，如果当前对象valid == 0，说明是新缓存的对象，所以掉驱动接口virtio_disk_rw往buf里写入对应磁盘block中最新的内容。
  - bwrite()：buffer cache层对外的写接口，传入buf对象，将buf中的内容写入磁盘。调用者必须已经获得了相应buf的锁，否则会panic。然后把通过驱动接口virtio_disk_rw把缓存buf写入对应块。

## 块分配器

- fs.c/balloc()：读取位图block，找到空闲的数据block，将其对应的bit写为1，清空该block上的内容，返回该blockno
  - b：0到每一个块第一个位置中间的Bit数
  - bp：b所代表的块在位图中的位置
- fs.c/bfree()：传入要free的blockno，读取位图block，将其对应的位写为0。

## inode

inode包含存在磁盘上持久化的inode和缓存在内存中的inode cache。

### dinode

struct dinode是磁盘中的inode

fs.h/dinode:

- type：文件类型（文件、目录、设备）。0表示dinode为空闲
- nlink：引用此inode的**目录**条目数。因为每个文件或目录都一定在一个目录中，所以当nlink为0时说明inode对应的文件可以释放
- size：文件字节数
- addrs数组：此文件所有块的blockno，即块地址

### inode

struct inode是dinode的内存副本。

- file.h/inode

  - ref：引用此inode的C指针数量。当ref == 0，表明没有线程正在使用此inode。可以从内存中丢弃此inode cache

  - lock：inode的睡眠锁。和buf cache不一样的是inode cache允许多个线程同时持有inode cache的指针（iget）。而如果涉及某些需要加锁的操作则再额外调用ilock获得一个inode cache的锁

    *思考：为什么inode cache采取和buffer cache不一样的锁设计？一个进程可能会（打开后）长期持有一个文件（即其inode），在读写的时候再取锁，但对于buffer（block） cache，一般并不会长期持有一个block的内容。一般对block的使用都是获取即访问/修改。设计的差异来源于使用场景的差异*

  - valid：和buffer cache中buf->valid类似，用于标记此cache是否是新分配的

  - type、size、addrs等：和dinode中的一样，表示对应文件的inode的内容

- icache：是一个静态的全局变量，相当于buffer cache中的bcache，用来管理所有cache的inode

  - lock：icache的大锁，在修改全局inode数组的时候要获取

  - inode[NINODE]：存储所有的inode（cache）对象

- iget()：传入inum和dev，返回inode对象指针。不带锁返回，因此**允许多个线程同时持有某inode的指针**
  - 首先检查inum对应的inode是否已经cache。
  - 如果有则返回。
  - 如果没有，则找到icache->inode数组中第一个ref == 0的inode，将其inum和dev都修改为入参。并修改valid = 0.标记此inode为新
  - 如果没有已经cache且没有ref == 0的inode，说明所有inode cache都忙，直接panic
  
- ialloc()：遍历所有的inode block，找到空闲的dinode，调用iget（即会把这个新分配的inode缓存）获得其对应的inode cache对象指针。

- ilock()：读或写inode之前都必须通过ilock锁定inode。

  - ilock()主要就是获取了inode的睡眠锁
  - 通过，ilock()处理了ip->valid。因为读取和写入前必调ilock()，所以从iget中获得的inode如果valid == 0，即新分配，则ilock从磁盘（block cache）读入内容至inode cache，完成inode cache和磁盘的同步。

- iunlock()：释放inode的睡眠锁，唤醒相应进程

- iput()：其作用是释放掉一个inode的引用（名字有点confusing，为什么是put..?）

  - 当前inode的指针引用ref == 1且目录引用nlinks == 0，说明此时已经没有别的进程在持有这个inode对应的文件了。
    - 此时可以在获得inode睡眠锁后**释放掉icache的锁**。
    - 然后执行itrunc()释放inode中所有data block
    - 然后更新inode -> type = 0，并执行iupdate()同步inode cache到磁盘中。
    - 然后把ip -> valid标记为0.
    - 最后再次获得icache的锁，并为当前的inode -> ref --。
  - 如果此时nlinks仍不为0，则在持有icache的情况下对inode -> ref --即可。

  *iput的过程其用锁有很多值得思考的地方。在发现一个inode可被释放时，**为什么在获得了inode睡眠锁后要释放掉icache的锁？这样做没有问题吗**？*

  *这么做的目的应该是在磁盘上释放整个文件（inode上的所有数据block）耗时长，而icache锁获取inode等众多操作中都要被使用。如果不释放相当于一个文件在被删除时整个文件系统都不能读/写新的文件。*
  *这样做应该是没有问题的。首先nlinks == 0，表明此文件已经不在任何目录中，逻辑上确保不会有别的进程在iput释放了icache锁之后尝试iget这个in-memory inode；*
  *其次如果此时正好ialloc企图分配这个inode，也不会成功。因为ialloc首先会判断一个（磁盘上的）inode的type是否为0. 而更新inode -> type为0后，调iupdate的过程也会需要获得dinode对应块中的buf锁。而ialloc要读dinode也要获得buf锁。因此ialloc要么读到正在删除但没有完成的inode（dinode）的type，此时type不为0。当读到磁盘上的inode -> type为0时，此inode一定已经在磁盘上删除完了。*
  *严格来说其实并不是彻彻底底地”free“完。假设线程A在将type标为0并写入磁盘后，之后执行valid = 0--释放inode睡眠锁--获取icache锁--ref自减。这个过程是**没有修改inode cache的inum和dev的**。*
  *假设在type被标为0后，线程B在ialloc中，从磁盘找到了这个inode并调用iget将这个磁盘上的inode缓存进cache。如果此时线程A还没执行ref --，因为线程A中又没有把原inode cache的inum和dev清零。有一种可能是这个inode马上会被复用（因为ref > 0, 且inum和dev都相等）。*
  ***但这是没有关系的**。因为就算这个inode在”没完全free完“的时机被复用了，也是ialloc对他的复用，ialloc本来就是要返回一个全新的inode cache。而这个被复用的inode此前又确实已经完成了除inum和dev的清零。这完全符合ialloc本身的需求*
  *还有一点注意的是，假设在type = 0到iput完全结束这个窗口除了发生了ialloc把这个inode复用，还发生了外部试图读这个inode的事件。这也不会受影响。*
  *因为这个窗口只剩两件事没做：1. valid置0，2. ref --。前面讨论了ref没来得及--的影响。如果现在是valid没来得及置零，也不会产生并发错误。因为用到valid的地方就是ilock。ilock必须要获得了inode睡眠锁之后才能通过判断valid决定是否从磁盘同步inode到cache。而inode的睡眠锁在valid置0后才释放。*

- itrunc()：传入inode指针，调用者必须持有锁。通过bfree()将所有直接块和间接块free调，inode -> addrs数组所有元素清零。inode->size置0，最后调iupdate同步

- iupdate()：同步内存inode上的所有字段到磁盘inode中。

  *间接块的索引块为什么不用同步？因为bmap()的时候，间接块每分配一个，其索引块里的地址会直接写回磁盘上，所以inode cache和磁盘同步只需要关心自己的块的本身的内容就行*

- idup()：给inode->ref ++。在某些场景下临时用到inode时可能需要调用。这样可以无锁并且避免了此时的inode使用完之前被其他进程回收掉。

- bmap()：传入inode和**block在inode中的序号**（即这个block属于当前文件data block的第几个），**返回这个block的地址（blockno）**。如果对应序号的data block还没分配：
  - 先在直接块上分。从balloc()获取块的地址（blockno）并写道inode -> addrs[]中
  - 如果直接块分完了，就从间接块的索引块中分。每写一次数据块的blockno到索引块，就调log_write写入磁盘。
  - 如果这个序号比直接块+间接块总块数都要大，直接panic
- readi()：从inode中读（文件）的数据（data block）。返回读取成功的长度，失败返回-1.
  - 入参：ip--文件的inode的指针，user_dst--标志位，表示读去的地址是/不是用户空间的地址，dst--读出的地址，**off--（文件内的）偏移量（单位字节），即本次读的起点**，n--读取的长度（单位字节）
  - 内部变量：tot--应该是total的简写，表示当前已读完的字节数；m--是即将要读的字节数
  - 首先检查off是否比文件总大小ip -> size还大，如果是则说明读的起点就越界了，直接不读。
  - 如果起点off没越界，终点越界了，则只读[off, sp->size]之间范围的数据。
  - 读的过程：从off开始，每次最多读一个block（这是因为每次我们只能从一个buf（即block cache）中获得数据。每读一个buf都应获得该buf的睡眠锁。
    - 先通过bmap找到当前off所在的blockno。因为off只是文件内的偏移量，所以要通过这个偏移量算出off属于当前文件内的哪个data block，然后再通过bmap获得该block实际的地址（blockno）。然后用bread()获得该block的buf
    - 计算即将要读出的数据长度。这个长度是min(n-tot, BSIZE - off%BSIZE)。
      首先off%BSIZE意思是将当前文件内总偏移量转化为，一个块中的块内偏移量。因此BSIZE - off%BSIZE即这个块内的大小-块内偏移量，即块剩下的长度。
      n-tot是读取的总长度-当前已读长度，因此表示的是总的读取的剩余长度。
      因此当前这次读取的长度就应该是总剩余长度，与当前起点（偏移量）~当前起点所在块的末尾，之间的最小值。
    - 调用copyout把数据拷出。
    - 之后tot（当前已读长度），off（当前起点），dst（当前终点）都要+=上一轮的已读长度。

- writei()：大致和readi()相同，有几点要注意：
  - 如果off（起点）大于inode->sz，返回-1
  - 如果终点大于inode->sz，没有问题，之后对文件应当给予扩展。只要终点不要大于最大文件大小就好。
  - 扩展的方式很简单。只要不到终点，照常**读+写**buf就可以，超出inode->sz的block的读写不会有影响。因为bmap不会检查当前传入的block序号（不是blockno）是不是在inode ->size中的约束，而只会返回对应的blockno而已。
    最终再根据最后的off，更新inode->size即可
- stati()：传入一个stat指针和inode指针，将inode中的信息写到stat。主要是通过stat系统调用返回给用户

## 目录

- dirlookup()：传入一个文件的inode指针dp，查找的文件/目录名name，以及希望获得的 找到的文件/目录的目录项 在查找的目录中的偏移量poff。poff是用来传出的。返回该name对应的文件/目录在此目录中inum
  - 用readi遍历当前目录的所有data block。一次读一条目录项de。off表示遍历过程中，当前遍历到的de在dp的data block中的偏移量
  - 如果de -> inum == 0，说明该目录项未分配。继续扫
  - 如果de -> inum != 0且de -> name也和name匹配说明这就是要找的目录项，将当前off写入poff，用iget和de -> inum获得该inum对应的inode，返回

​	*dirlookup是iget不返回带锁的inode的原因。最典型的场景是如果要在dirlookup中查找当前目录，则用dp查找到的时候必然会锁定当前目录，而在iget中也尝试获得当前目录的锁的话，则会死锁。还有很多更复杂的情况。所以此时将锁inode的功能从iget中拆分出来，在某些场景（读写inode）需要加锁时调ilock加，在其他场景可以无锁获得inode时就直接iget即可*

- dirlink()：传入目录inode指针dp，准备添加到目录的目录项名字，以及其inode的inum。目的将给定文件/目录名及其inum添加到目录dp中。
  *这个类似”新建文件夹“或”创建文件“到某目录的操作就是所谓的”Link“*
  - 如果当前目录已有一个相同的文件名，则将iput欲link的文件。可以从这里推断dirlink调用前会对该文件/目录的inode->nlink++。
  - 否则从dp的所有目录项中找一个空的，把要link的文件的文件名和inum写到目录项中。

## 路径

- skipelem：传入路径名path，和一个name（用于写），将path中的下一个元素写入name，将下一个元素之后的路径返回。
  返回的路径去掉前缀”/“. 

  *注意：*如果下一个元素之后没有路径，即当前路径就是最后一个元素，则返回""。如果传入path是空字符串或'/'，则返回**0**。两者不一样

- namex()：查找一个路径，返回这个路径最终指代文件/目录的inode或者其父节点的inode

  - 入参：

    - path：查找路径
    - nameiparent：返回路径所指代节点的inode所在父节点的inode，还是节点自己的inode
    - name：传出用，将最终节点的文件名写到name中

  - 首先确定起始目录，如果是/则起始目录是root，否则是当前进程的所在目录，用ip指向其实目录的inode。

  - 调skipelem，对path进行更新，当前节点名写入name中，skipelem()的返回值即下一轮检索的path。当skipelem返回值 != 0时，即当前path仍为非空时，进入循环

    - 此时的ip一定是目录类型的。
      根据while的条件和skipelem的行为，当走到最后一个节点，也就是name是路径中最后一个节点的文件名时，ip指向最后一个节点的所在目录，新更新的path此时为""。下一轮循环再对""调skipelem时，自然会返回0从而退出循环。
      所以如果ip不是目录，则直接报错退出，说明路径上的某个中间节点不是目录
    - 如果下一轮的path已经为空而且nameiparent为真，则提前退出循环。此时退出循环后ip指向的则是最后一个节点的父节点。
    - 然后去当前目录ip中找name的inode，找不到则报错退出。找到了则将找到的文件名对应的inode返回。
    - 然后将ip更新为下一个节点的inode，继续循环。

  - 当循环退出时，如果nameiparent为1，则ip已经指向了路径末尾节点的父节点，否则ip指向末尾节点。无论如何，而name都已经写成了末尾节点的文件/目录名。因此最后返回ip即可。

    *返回之前，如果nameiparent为1时还调用了iput。说明nameiparent的方式查路径最终虽然想要拿到最终节点的父节点，但并不在乎对他的引用？也就是它可以被释放？后续看看调用者怎么用nameiparent的*

## 文件（文件描述符）层

Unix中的大多数资源都表示为文件，**包括控制台、管道、真实文件。文件描述符层是实现这种一致性的层**。此时的“文件”指的是“文件描述符”，是一个更高层的抽象概念，**file层是文件系统的最高层**，建立在文件/目录、管道、控制台等之上。

正如在第1章中看到的，Xv6为每个进程提供了自己的打开文件表或文件描述符。每个打开的文件都由一个struct file表示

- file.h/file：struct file就是对各种“文件”类型的封装，
  - type：文件的类型。枚举类型，可以是INODE（即文件/目录）、管道、设备。
  - ref：该“文件”的引用计数
  - readable/writable：标志文件是否可读/可写
  - pipe：管道指针（如果当前file是管道的话才有用）
  - inode：inode指针（如果当前file是文件/目录的话才有用）
  - off：file当前的io偏移量

- file.c/ftable：是一个全局变量。系统**所有打开的文件**都会存在ftable->file[]中。类似icache和bcache。还有一把自旋锁
- filealloc()：分配一个文件。扫描ftable->file[]，找到ref为0的文件，将其ref置1，返回file
- filedup()：增加一个file的ref
- fileclose()：减少一个file的ref。如果ref == 0；根据file的类型调用该类型的释放函数。
  *为什么inode的释放只需要调iput()？因为file的释放不一定意味着真实文件的删除。只是关掉了一个文件描述符而已，所以也有可能别的文件正指向该inode。因此要做就是调iput自减inode的ref就可以了*
- filestat()：调istat将inode的元数据拷到指定的地址。系统调用stat的底层实现
- fileread()/filewrite()：read/write系统调用的底层实现
  - 对管道读/写：（待补充）
  - 对设备读/写：调对应设备的读/写函数。
  - 对inode读/写：传入file偏移量和读/写长度作为操作偏移量，调readi和writei完成文件读写。
- sysfile.c/fdalloc()：为文件分配一个文件描述符。传入file指针，返回一个整数，这个整数就是进程文件描述符表中该file所在的下标。
  扫描进程的（已经打开了的文件的）文件描述符表p -> ofile[]，该表存的是file\*。如果扫到为0，则把传入的file\*写到该槽位中，最终返回file\*的所在数组中的下标
  **这里可以认为是文件系统架构的终点，我们可以看到面向用户的文件描述符是怎么实现的**

## 文件系统调用 sysfile.c

与文件相关的系统调用建立在前面各层的接口之上

- sys_link()：为一个已经存在的文件创建一个在另一个文件夹的链接。传入：old--原文件及其路径，new--新路径（包含文件想移动去的文件夹和文件在该文件夹的文件名）

> 例如：为/aaron/test.txt在/os文件夹下添加一个链接，链接名为mynote.txt，则old为/aaron/test.txt，new为/os/mynote.txt

 	首先通过namei找到old路径的文件的inode ip，如果该inode不是文件类型，直接报错退出。这意味着不能为目录/管道等其他类型创建链接。

​	通过nameiparent找到new路径下子节点（因为子节点就是即将要链接过去的“文件”，所以肯定是还不存在的，这就是nameiparent的用法）的父节点的inode dp，dp必然是目录类型。以及获得了new的子节点的文件名name。

​	最后调用dirlink传入dp，name，ip的inum，即往dp中添加了原文件inode的目录项。完成链接

- create()：为新inode创建一个新名称（不严格地说就是创建一个“文件”），它可以为带有O_CREATE标志的open生成一个新的普通文件，mkdir生成一个新目录，mkdev生成一个新的设备文件。
  - 传入路径名（路径名是包括了要创建的（即还不存在的）文件的文件名）、文件类型以及major、minor。返回创建文件的inode指针
  - 首先调nameiparent获取父目录inode dp，并得到子节点的文件名name
  - 在父目录dp下dirlookup一下是否有name的目录项。
  - 如果name确实存在，且create是代表open使用的（type == FILE），并且name文件本身也是一个FILE，那么视为成功，直接返回该文件的inode。否则视为失败返回0
  - name不存在的话，调ialloc分配一个新的inode。根据major和minor设置inode的属性，nlink设为1。然后iupdate一下同步到磁盘（这是核心的一步，笼统讲**创建一个文件就是创建一个inode**）
  - 如果是目录的创建，则用dirlink()为当前目录添加.和..的目录项，代表当前目录和父目录
  - 最后把当前“文件”link到父目录下
- sys_open()：用户程序系统调用open的实现。
  - 首先会接收路径，和一个**omode**参数，这个参数表明了这次open系统调用的“模式”。里面包含了读/写，是否创建等信息，用位管理。omode对open的行为至关重要
  - 如果当前的open模式有create位（有create位就说明创建的一定是一个普通的文件而不是其他任何的东西。因为创建目录应该调mkdir），则调create()获得这个用户企图open的文件的inode。
    否则直接通过namei获得这个路径对应的inode。如果这个inode是个目录且不是只读（也是通过omode中的位来判断）的，直接报错退出（因为目录只能只读）
  - 然后调filealloc和fdalloc创建file对象，并且为当前进程分配该file的文件描述符。
  - 最后根据inode的类型、以及omode的读写bit，为file的字段进行设置。最重要的是把file字段的f->ip设为前面得到的inode\*。这样文件层和底层的文件系统就彻底通过这里的调用连接在一起了
- sys_mkdir：传入路径（这个路径是包含马上要新建的文件夹的name的，具体看create的行为），调create在当前父目录中创建一个文件夹。成功则返回0
- sys_mknod：传入路径、major、minor，调create创建一个设备文件



# 管道 pipe

 管道既是一个较独立的功能，在围绕sleep-wakeup实现，最终在文件系统的文件描述符层统一成file类型的一种

### pipe的使用

系统调用接口：int pipe(int p[])：Create a pipe, put read/write file descriptors in p[0] and p[1].

用户使用pipe系统调用，通过传入一个用户地址空间中长度为2的int数组p，最终系统调用将分配一个pipe，并把pipe的读、写文件描述符写到用户数组p中

利用pipe实现进程间通信的典型样例：pipe+fork

```c
int p[2];
char *argv[2];
argv[0] = "wc";
argv[1] = 0;
pipe(p);
if(fork() == 0) {
    close(0);
    dup(p[0]);
    close(p[0]);
    close(p[1]);
    exec("/bin/wc", argv);
} else {
    close(p[0]);
    write(p[1], "hello world\n", 12);
    close(p[1]);
}
```

父进程声明了一个pipe，并获得了pipe的读写文件描述符，存在p[0]和p[1]中，然后fork之后的子进程也有了一模一样的文件描述符。底层看就是父子进程的文件描述符表中都有了该pipe的读、写file对象的指针。
fork之后父子进程可以对pipe进行读写，之后大家的file偏移量则不再是统一的了。

## pipe.c

- struct pipe：一个pipe对象占一个page的大小。其包含了：lock--自旋锁、data[]--pipe的数据区域，共512字节；nread/nwrite--pipe已读/已写的的字节数；readopen/writeopen：标志pipe是否允许读/写

- pipealloc：传入两个file **，成功返回0，否则返回-1。

  *为什么要传入两个file \*\*类型的指针？调用者希望通过pipealloc的调用获得一个指向pipe读端的file\*和一个指向pipe写端的file\*。而因为函数设计上返回值用来表达成功/失败。如果不在返回值返回file\*的情况下要让调用者获得，则调用者就要传file\*\*才行。*
  *类似于：如果通过写入参进行返回，假设是整数，那么入参就应该是该整数的地址，即int\*。*

  - 首先通过filealloc分配file，获得两个file\*指针。
  - 然后调用kalloc分配一个page，将指向page的指针转换成pipe\*类型指针pi。
  - 利用pi然后对pipe对象的成员变量做一系列初始化，此时标志pipe为可读、可写。并初始化pipe锁
  - 对该pipe的读写file对象做初始化：文件类型为FD_PIPE，读写标志一个为只可写、一个为只可读，f -> pipe为pi。
  - 成功则返回0

- piperead()：传入pipe指针pi，读出的地址addr（用户地址空间），以及读取长度。最终用户在读pipe使用read系统调用时，被fileread()调用。

  - 先获得pipe锁
  - 判断pipe是否为空（用nwrite和nread判断），为空则唤醒该pipe中的写进程，然后自己sleep。pipe锁就是保护这个判断的condition lock。如果为空则sleep，传入pipe锁。
  - 否则每次从pipe读取一个字符，将字符拷出（copyout）addr。
  - 读完后则唤醒在该pipe中睡眠的写进程
  - 返回读取长度

- pipewrite()：传入pipe指针pi，写入内容的所在地址addr（用户地址空间），以及写入长度。最终用户在写pipe使用write系统调用时，被filewrite()调用。

  - 获得pipe锁。判断pipe是否已满。已满则唤醒该pipe中的读进程，然后自己sleep。
  - 每次写入内容所在地址拷入（copyin）一个字符，并将字符添加到pipe->data[]中。
  - 最终唤醒该pipe中的读进程。返回写入成功的长度。

- pipeclose()：如果当前writable为真，则尝试唤醒所有写进程；readable为真，则唤醒所有读进程。（保证最后pipe读写完）。最终调用kfree释放pipe所使用的page

## sysfile.c

sys_pipe()：接收用户空间传入的数组fdarray，最终将pipe的读写file对应的fd写入数组中。

- 调用pipealloc分配一个pipe，并获得该pipe的读写file对象指针
- 为这两个file分配当前进程的文件描述符
- 把两个文件描述符copyout到用户空间的fdarray数组

最终，用户程序通过pipe()系统调用获得



# 日志系统 log.c

- struct logheader：log header结构，包含
  - n：已经提交但尚未转移完成的log block数量
  - int block[]：记录了第i个log block里存的是哪个blockno对应的block的数据
- struct log：日志全局管理器结构。
  - lock：管理整个struct log的大锁
  - size：日志系统块总数
  - start：日志系统起始块的blockno。和size一样都是从超级块获得
  - outstanding：目前有多少个文件操作（事务）正在进行
  - committing：日志系统是否正在commit中
  - lh：logheader对象。

- begin_op()：标志文件系统操作事务的开始。
  - 先获得lock锁，此时的lock锁充当condition lock，因为之后可能要sleep
  - 如果当前日志系统正在提交，则睡眠等待
  - 如果计算得到，当前logheader.n+正在执行的文件操作（事务）可能写入log block的最大数+当前事务可能写入log block的最大值大于log最多容纳的数量，睡眠等待。
    这是为了避免本次事务可能造成的溢出。
  - 上两条件皆pass后，则log.outstanding ++。标志着事务开始。
- log_write()：当进程要执行写文件操作时，不直接用bwrite（写缓冲区），而是用log_write()将写记录到事务中。
  - 从log.lh.block[i]中找是否当前要写的块的blockno已经存了，如果存了就不需要做任何事（log absorbtion）
    因为根据日志系统的工作流程，最终落盘到log block的操作发生在commit。在事务过程中对同一个block cache写多少次都不需要日志系统关心。日志系统只需要在事务结束后将对应block中的数据落到log block即可。
  - 如果不在，则记录下该blockno在log.lh.block[]中，然后把该block pin在buf cache中，防止其在事务结束之前被驱逐。然后log.lh.n++。此时log header的这些操作都是在内存上的，尚未落盘到log header block中，所以如果此时崩溃相当于没有写成功。不影响一致性
- end_op()：标志着事务的结束，此函数的实现蕴含了“group commit”（组提交）的思想
  - 首先log.outstanding --。意味着这个当前文件事务已经“结束”
    outstanding的作用在于给begin_op()估算能否启动一个新的事务，此时的--是没有问题的，看下面
  - 如果当前log.committing = 1，报错退出，这是不可能的。
    *因为对于一个group来说，只有group的最后一个事务来到时才可能触发真正的提交（看下面），所以：1. 一个group的非最后一个事务的到来时一定不会正在commit；2. 在最后一个事务到来并开始了commit（committing = 1）后，如果此时马上有begin_op()企图启动事务也是不可能的。因为begin_op()在committing时会sleep。*
    *这相当于告诉我们：log commit是组提交的。在一个组尚未执行提交之前，可以有不断新的begin_op()加入组（看下面），当组正在提交时，下一组事务的开始必须等待当前组事务的完全结束。*
  - 如果当前outstanding == 0，说明当前事务是当前事务组中的最后一个，则do_commit = 1（马上执行commit），以及log.committing = 1（标志当前日志系统正在提交）
  - 如果outstanding != 0，则不在这一步进行提交，但是此时可以唤醒正在睡眠的begin_op。
    *为什么一开始的log.outstanding --是没有问题？因为begin_op()是根据log.outstanding +1（当前正在执行事务加自己马上要启动的事务的事务数）\*MAXOPBLOCKS + log.lh.n（即：当前正在执行的事务与自己马上启动的事务可能占用的block的最大值，加上log block已经被占用的数量n）来估算是否启动当前事务的。*
    *因为当前事务与当前正在执行的其他事务所要用的log block是未知的，所以只能按照MAX来估算。而在该事务进入end_op()后，说明该事务已经不可能再写log block了，其对log block的占用已经记录在了log.lh.n中，所以此时可以安全地outstanding --。并且尝试唤醒正在睡眠的begin_op()线程，相当于看看是否还能让其他等待启动的事务加入这次的group*
  - 如果经过上面的流程，do_commit = 1，则执行group提交。此时调用commit()。完成后将log.commtting置0，标志着当前事务组已经全部结束。然后唤醒正在等待的begin_op()。
- commit()：
  - 如果lh.n > 0，代表有需要commit的块，则执行下面的步骤。否则退出。
  - 首先调write_log()将所有被log.block[]记录的blockno对应的cache落盘的对应的log block中。
  - 然后调write_head()将log header cache落盘到log header block中，此处是真正的commit checkpoint。这个函数执行完后，当前commit的所有内容都会被确认，之后直到事务结束的任何一个步骤crash掉，都能恢复。
  - 调install_trans()：根据log.lh，将log block中的数据转移到其对应的真正的block中
  - 将log.lh.n赋0（内存中的log.lh）
  - 再次调write_head()将已经赋0了n的log.lh写到header block中。这一步是clean。标志着此次事务的完全结束。
- write_log()：把header记录的要即将写的block从cache中落到log block中。
  - 根据log.lh，对log.lh.block[0:n]的block逐一操作：
  - bread读出该block的buf cache，from
  - bread读出要写入的log block的buf cache，to
  - memmove将from的内容写到to中
  - bwrite(to)，落盘log block
- write_head()：把header cache（log.lh）落盘header block
  - 读出header block的buf cache，buf
  - 根据log.lh对该buf写
  - 调用bwrite(buf)落盘

*为什么说write_head()是真正的提交点？因为在write_head()执行成功之前，所有的header的数据都存在于内存中。崩溃之后由于header的数据没有落盘，而恢复又是根据从header block中读出的log header进行的，所以哪怕之前把block落盘了log block，也不会执行恢复。*
*当header成功落盘后，也意味着之前的log block也成功落盘了。所以之后crash也一定能恢复。所以write_header是当前事务的commit point*

- install_trans()：将log block的数据转移到其真正属于的block中。接收一个传入参数recovering
  - 根据log.lh.n的大小，依次读出数量n个log block的buf cache，lbuf
  - 根据log.lh.block[i]，依次读出n个log block对应的block的blockno的buf cache，dbuf
  - 两buf cache进行memmove，然后dbuf落盘
  - 如果当前的install_trans不是因为crash之后的恢复所被调用（recovering = 0）的，则将dbuf unpin掉。此时dbuf可以被驱逐了。如果是recovering = 1，则不需要做这一步。因为crash完后的内存都清除了，不需要考虑buf cache的问题

### 文件系统恢复

如果crash后重启，重启首先会调用日志系统初始化：

- initlog()
  - 首先根据超级块参数初始化log.start、log.size等
  - 然后调recover_from_log()
- recover_from_log()
  - 调read_head()读head block到内存中的log.lh中
  - 调install_trans(1)：传入recovering参数为1，说明这是因崩溃后恢复调用的install_trans
  - log.lh.n赋0，然后write_head()。和commit()最后一步一样。标志着（没完成的）事务的彻底结束。在此之前如果继续crash，则会不断重复前面的transform的操作。因为是操作幂等的，所以不影响一致性
