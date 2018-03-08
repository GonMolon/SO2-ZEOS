#include <libc.h>
#include <types.h>

char* buff;

int pid;

int __attribute__ ((__section__(".text.main"))) main(void) {
    buff = "Hello World";
    if(write(1, NULL, strlen(buff)) == -1) {
        buff = "Error detected:\n";
        write(1, buff, strlen(buff));
        perror();
    }
    while(1);
    return 0;
}
