#include <libc.h>
#include <types.h>

void test() {
    write(1, "child\n", 6);
    // exit(1);
    return;
}

int __attribute__ ((__section__(".text.main"))) main(void) {
    int stack[32];
    clone(&test, &stack[32]);
    write(1, "father\n", 7);
    while(1);
    return 0;
}
