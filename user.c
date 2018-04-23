#include <libc.h>
#include <types.h>

int myVar;

int __attribute__ ((__section__(".text.main"))) main(void) {
    myVar = 10;
    DWord pos = (DWord) &myVar;
    char buff[15];
    itoa(pos, buff);
    write(1, buff, strlen(buff));
    while(1);
    return 0;
}
