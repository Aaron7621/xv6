# Lab1


## Target
- Boot the XV6
- Shell program implementation: sleep()
- Shell program implementation: pingpong()
- Calculate primes by a concurrent algorithm. Implemented by using multi-process and pipe() system call.
- System call implementation: find()
- Implement the xargs


## Explanation

### sleep:

Just create sleep.c in user level and call the sleep() system call. It will evetually be navigated to sys_sleep() function in kernel level.

### pingpong:

Great application of pipe() system calls. Be aware of the fds' opening and closing.

### ⭐primes:

Very impressive algorithm

Every process is reponsible for one prime, that means it should:

- output its prime
- filter all the input numbers by divide them by its prime, then output the filtered number to the next "prime principal".

We could create several pipes and processes to do this. Carefully open and close the pipes' fds.

### find:

Recursively go through the dictionary and use strcmp() to get the files with the same name as the target's.

### xargs:

From xargs' implementation, we get the former output from the standard input fd, and the shell's arguments are passed by the argv parameter.

The key is to read the standard input **per line**. And pass each line together with xargs' own arguments to exec() system call. 



## Enlightenment

When ALL  write fds of a pipe are closed, the read() system call returns 0. The file descriptor table of each process must be kept in mind at all times. 

When pipe() is called before fork(), both parent and child processes will have read and write fd for the pipe. If the child process will check when to finish reading from the pipe by the return value of read(), **it should firstly close the pipe write fd of the child process itself**. 

Then read() from child process can realize the semantics of "waiting for the parent process to release the write descriptor". Otherwise, the child process' read() will never return 0 because of its own write descriptor.



当管道写端全部关闭，read()系统调用返回值为0。必须时刻注意各个进程的文件描述符表。

当fork()之前进行了pipe()，那么父子进程都会有该管道的读写描述符。如果子进程希望通过read()的返回值判断什么时候结束读管道，必须先把子进程自己的管道写描述符关掉。

之后read()才能实现“等待父进程释放写描述符”的语义。否则子进程的read()会因为自己的写描述符的存在永远不会返回0。
