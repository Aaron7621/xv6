//
// Created by aaron on 2022/6/4.
//


#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main()
{
    char buf[10];

    int p[2];
    pipe(p);

    int pid;
    pid = fork();

    if (pid == 0) {

        read(p[0], buf, sizeof buf);
        printf("%d: received ping\n", getpid());
        write(p[1], "b", 1);
        exit(0);
    }else{
        write(p[1], "a", 1);
        wait((int *) 0);
        read(p[0], buf, sizeof buf);
        printf("%d: received pong\n", getpid());
        exit(0);
    }

}
