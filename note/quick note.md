为什么醒了要马上拿锁：
因为可能有多个线程都是sleep同一个事件。但只有一个线程能拿到

为什么会lost wakeup：



为什么lost wakeup之后会hang住？为什么后面手动往控制台打了几个字之后又短暂地“活了”？

1. 因为uartwrite在写到所有THR寄存器（假设输出寄存器只有一个）都忙时就会sleep。在THR处理完毕后，发出中断返回。假设此时发生了lost wakeup，即在线程成功sleep的前一刻，wakeup执行完毕，把所有在等待chan的状态为sleep的进程标为了RUNNABLE，然而此时本应在sleep的进程还没有sleep，而是在wakeup执行完后才sleep。所以此时在不触发LHR中断的情况下，在uartwrite中sleep的进程将永远不会被唤醒。
2. 而此时如果手动往控制台打几个字，将会触发UART的中断，从而把本应”醒着的“进程唤醒。但是后面在uartwrite的过程中一样有可能发生1一样的事情。没hang只是巧合



为什么要进程和uart两把锁：

1. uart的锁充当了condition lock的作用。更大的原因感觉在于在中断发生的时候，中断处理函数是没有办法获得等待中断时的进程的上下文的