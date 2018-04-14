#include <libc.h>
#include <types.h>

int __attribute__ ((__section__(".text.main"))) main(void) {
    runjp_rank(7, 11);
    // runjp();
    while(1);
    return 0;
}
