//
// Created by aaron on 2022/6/4.
//

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


int
main(int argc, char *argv[])
{
    if(argc <= 1) {
        fprintf(2, "[lab 1] Usage: sleep file...\n");
        exit(1);
    }

    for (int i = 1; i < argc; i++) {
        sleep(atoi(argv[i]));
    }
    exit(0);
}

