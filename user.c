#include <libc.h>
#include <types.h>

int __attribute__ ((__section__(".text.main"))) main(void) {
    fork();
    while(1);
    return 0;
}
