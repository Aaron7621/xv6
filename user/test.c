//
// Created by aaron on 2022/6/4.
//

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void func(char *x){
    while (1) {
        printf("%s", x);
    }
}

int
main()
{
    char *a0 = "abc";
    func(a0);
    a0[1] = 'c';
    exit(0);
}

