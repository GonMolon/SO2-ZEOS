#include <libc.h>
#include <types.h>

char* buff;
char time_buff[10];

int pid;

int __attribute__ ((__section__(".text.main"))) main(void) {

    unsigned long time = gettime();
    itoa(time, time_buff);
    if(write(1, time_buff, strlen(time_buff)) == -1) {
        buff = "Error detected:\n";
        write(1, buff, strlen(buff));
        perror();
    }

    while(1);
    return 0;
}
