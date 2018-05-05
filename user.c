#include <libc.h>
#include <types.h>

int __attribute__ ((__section__(".text.main"))) main(void) {
    runjp_rank(5, 5);
    // GLOBAL_VAR = 1;
    // char stack[5][32];
    // sem_init(1, 0);
    // for(int i = 0; i < 5; ++i) {
    //     clone(&test, &stack[i][32]);
    //     sem_wait(1);
    // }
    // exit(1);
    while(1);
    return 0;
}
