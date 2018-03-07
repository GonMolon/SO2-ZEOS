#include <libc.h>

char* buff;

int pid;

int __attribute__ ((__section__(".text.main"))) main(void) {
    buff = "Hello World";
    write(1, buff, strlen(buff));
    while(1);
    return 0;
}
