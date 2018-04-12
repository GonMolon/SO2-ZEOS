#include <libc.h>
#include <types.h>

int __attribute__ ((__section__(".text.main"))) main(void) {
    int PID = fork();
    char buff[15];
    itoa(PID, buff);
    write(1, buff, strlen(buff));

    PID = fork();
    itoa(PID, buff);
    write(1, buff, strlen(buff));

    PID = fork();
    itoa(PID, buff);
    write(1, buff, strlen(buff));

    PID = fork();
    itoa(PID, buff);
    write(1, buff, strlen(buff));
    
    while(1);
    return 0;
}
