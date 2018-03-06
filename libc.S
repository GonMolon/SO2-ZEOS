#include <asm.h>

ENTRY(write)
    pushl   %ebp
    movl    %esp, %ebp
    
    // Saving protected register
    pushl   %ebx

    // Passing parameters to system call
    movl    8(%ebp), %ebx
    movl    12(%ebp), %ecx
    movl    16(%ebp), %edx

    // Choosing the system call identifier
    movl    $4, %eax

    // Generating the interrupt to make the system call
    int     $0x80

    // If result is negative, change it to -1 and update errno
    cmpl    $0, %eax
    jge     else
    movl    %eax, errno
    movl    $-1, %eax
else:

    // Restoring protected register
    popl    %ebx

    // Returning
    popl    %ebp
    ret