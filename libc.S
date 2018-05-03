#include <asm.h>

#define CONCAT_AUX(A, B) A ## B
#define CONCAT(A, B) CONCAT_AUX(A, B)

#define UPDATE_ERRNO \
    cmpl    $0, %eax; \
    jge     CONCAT(else, __LINE__); \
    imul    $-1, %eax; \
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
    // Choosing the system call identifier
    movl    $10, %eax

    // Generating the interrupt to make the system call
    int     $0x80

    UPDATE_ERRNO

    // Returning
    ret


ENTRY(fork)
    movl    $2, %eax
    int     $0x80
    UPDATE_ERRNO
    ret


ENTRY(getpid)
    movl    $20, %eax
    int     $0x80
    UPDATE_ERRNO
    ret


ENTRY(exit)
    movl    $1, %eax
    int     $0x80
    UPDATE_ERRNO
    ret


ENTRY(get_stats)
    pushl   %ebp
    movl    %esp, %ebp
    
    pushl   %ebx
    movl    8(%ebp), %ebx
    movl    12(%ebp), %ecx

    movl    $35, %eax
    int     $0x80

    UPDATE_ERRNO

    popl    %ebx
    popl    %ebp
    ret


ENTRY(clone)
    pushl   %ebp
    movl    %esp, %ebp

    pushl   %ebx
    movl    8(%ebp), %ebx
    movl    12(%ebp), %ecx

    movl    $19, %eax
    int     $0x80

    UPDATE_ERRNO
    
    popl    %ebx
    popl    %ebp
    ret
