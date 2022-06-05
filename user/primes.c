//
// Created by aaron on 2022/6/4.
//

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

void
new_process(int left[], int right[])
{
    close(left[1]);
    int p;

    if (read(left[0], &p, sizeof p) == 0) {
        exit(0);
    }
    if(fork() != 0) {
        printf("prime %d\n", p);

        int n;
        while (read(left[0], &n, sizeof n) != 0) {
            if (n % p != 0) {
                write(right[1], &n, sizeof n);
            }
        }
        close(right[1]);

        wait((int *) 0);
        exit(0);
    } else {
        close(left[0]);
        close(left[1]);
        int new_right[2];
        pipe(new_right);
        new_process(right, new_right);
        exit(0);
    }
}


int
main()
{

    int sleft[2];
    pipe(sleft);
    for (int i = 2; i < 36; ++i) {
        write(sleft[1], &i, sizeof i);
    }

    int sright[2];
    pipe(sright);

    new_process(sleft, sright);
    exit(0);
}

