#include <asm.h>

#define CONCAT_AUX(A, B) A ## B
#define CONCAT(A, B) CONCAT_AUX(A, B)

#define UPDATE_ERRNO \
    cmpl    $0, %eax; \
    jge     CONCAT(else, __LINE__); \
    movl    %eax, errno; \
    movl    $-1, %eax; \
    CONCAT(else, __LINE__):
    

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
    UPDATE_ERRNO

    // Restoring protected register
    popl    %ebx

    // Returning
    popl    %ebp
    ret


ENTRY(gettime)
    pushl   %ebp
    movl    %esp, %ebp

    // Choosing the system call identifier
    movl    $10, %eax

    // Generating the interrupt to make the system call
    int     $0x80

    UPDATE_ERRNO

    // Returning
    popl    %ebp
    ret