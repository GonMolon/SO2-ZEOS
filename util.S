#include <asm.h>
#include <segment.h>

ENTRY(add2)
    pushl   %ebp
    movl    %esp, %ebp
    movl    12(%ebp), %eax
    addl    8(%ebp), %eax
    popl    %ebp
    retl