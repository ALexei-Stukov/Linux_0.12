.code32
.text
.globl keyboard_interrupt

.align 4
keyboard_interrupt:
    pushw %ds
    pushw %es
    pushw %fs
    pushl %edx
    pushl %ecx
    pushl %ebx
    pushl %eax
    movl  $0x10, %eax
    movw  %ax, %ds
    movw  %ax, %es
    movl  $0x17, %eax
    movw  %ax, %fs
    pushl %eax  
    call  keyboard_handler
    popl  %eax
    popl  %eax
    popl  %ebx
    popl  %ecx
    popl  %edx
    popw  %fs
    popw  %es
    popw  %ds
    iret

