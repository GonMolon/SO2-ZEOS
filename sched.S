#include <asm.h>

ENTRY(task_switch)
    pushl   %ebp
    movl    %esp, %ebp

    pushl   %esi
    pushl   %edi
    pushl   %ebx

    pushl   8(%ebp)
    call    inner_task_switch
    addl    $4, %esp

    popl    %ebx
    popl    %edi
    popl    %esi    

    popl    %ebp
    ret

ENTRY(get_ebp)
    movl    %ebp, %eax
    ret

ENTRY(change_context)
    movl    4(%esp), %esp
    popl    %ebp
    ret