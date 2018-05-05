#include <libc.h>
#include <types.h>

int __attribute__ ((__section__(".text.main"))) main(void) {
    runjp_rank(17, 17);
    while(1);
    return 0;
}
