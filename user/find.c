//
// Created by aaron on 2022/6/5.
//

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

//extern int errno;
char *target;

void
find_name(char *path)
{
//    printf("enter in the func\n");
    char buf[512], *p, *q;
    int fd;
    struct dirent de;
    struct stat st;

    if((fd = open(path, 0)) < 0){
        fprintf(2, "ls: cannot open %s\n", path);
        return;
    }

    if(fstat(fd, &st) < 0){
        fprintf(2, "ls: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch(st.type){
        case T_FILE:
//            printf("in the case T_FILE, path=%s\n", path);
            for(q=path+strlen(path); q >= path && *q != '/'; q--)
                ;
            q++;
            if (strcmp(q, target) == 0){
                printf("%s\n", path);
            }
            close(fd);
            return;

        case T_DIR:
//            printf("in the case T_DIR, path=%s\n", path);
            if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
                printf("find: path too long\n");
                break;
            }
            strcpy(buf, path);
            p = buf+strlen(buf);
            *p++ = '/';
            while(read(fd, &de, sizeof(de)) == sizeof(de)){
                if(de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
                    continue;
//                printf("%s\n", de.name);
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = 0;
                if(stat(buf, &st) < 0){

                    printf("find2: cannot stat %s\n", buf);
                    continue;
                }
                find_name(buf);
            }
            close(fd);
            return;
    }
}


int
main(int argc, char *argv[])
{
    target = argv[2];
    find_name(argv[1]);
//    printf("%d %s %s\n", argc, argv[1], argv[2]);
    exit(0);
}
