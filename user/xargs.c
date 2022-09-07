//
// Created by aaron on 2022/6/5.
//
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"


int
main(int argc, char* argv[])
{
    if(argc <= 1) {
        fprintf(2, "[lab 1] Usage: xargs file...\n");
        exit(1);
    }

    char buf[512];
    char *p = buf;
    char *q = buf;
//  exec per line and per \n
    while(read(0, buf, sizeof(buf)) > 0){
        char *exec_argv[MAXARG];
        for (int i = 0; i < argc - 1; ++i) {
            exec_argv[i] = argv[i+1];
        }
        int len = strlen(buf);
        for(;p < buf + len; p++){
//            printf("p=%c\n", *p);
            if (*p=='\n'){
                *p = 0;
//                printf("q=%s\n",q);
                exec_argv[argc-1] = q;
//                printf("in for: %s\n", exec_argv[argc-2]);
                q = p + 1;
                if (fork() == 0) {
                    exec(exec_argv[0], exec_argv);
                    printf("exec error\n");
                } else {
                    wait((int *) 0);
                }
            }
        }
    }
    exit(0);
}

