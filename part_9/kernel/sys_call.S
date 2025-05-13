.code32
.text
.globl system_call, timer_interrupt, sys_fork
.globl hd_interrupt, floppy_interrupt

EAX        = 0x00
EBX        = 0x04
ECX        = 0x08
EDX        = 0x0C
ORIG_EAX   = 0x10
FS         = 0x14
ES         = 0x18
DS         = 0x1c
EIP        = 0x20
CS         = 0x24
EFLAGS     = 0x28
OLDESP     = 0x2c
OLDSS      = 0x30

int_msg:
    .asciz  "In kernel interrupt\n\r"

system_call:
    pushl %ds
    pushl %es
    pushl %fs
    pushl %eax
    pushl %edx
    pushl %ecx
    pushl %ebx
    movl  $0x10, %edx
    movw  %dx, %ds
    movw  %dx, %es
    movl  $0x17, %edx
    movw  %dx, %fs

    call sys_call_table(, %eax, 4)
    pushl %eax

ret_from_sys_call:
    popl %eax
    popl %ebx
    popl %ecx
    popl %edx
    addl $4, %esp
    popl %fs
    popl %es
    popl %ds
    iret

.align 4
timer_interrupt:
    pushl %ds
    pushl %es
    pushl %fs
    pushl $-1
    pushl %edx
    pushl %ecx
    pushl %ebx
    pushl %eax
    movl  $0x10, %eax
    movw  %ax, %ds
    movw  %ax, %es
    movl  $0x17, %eax
    movw  %ax, %fs
    movb  $0x20, %al
    outb  %al, $0x20
    movl  CS(%esp), %eax
    andl  $3, %eax
    pushl %eax
    call  do_timer
    addl  $4, %esp
    jmp   ret_from_sys_call

.align 4
sys_fork:
    call  find_empty_process
    testl %eax, %eax
    js    1f
    pushl %gs
    pushl %esi
    pushl %edi
    pushl %ebp
    pushl %eax
    call  copy_process
    addl  $20, %esp
1:  ret

hd_interrupt:
    pushl   %eax
    pushl   %ecx
    pushl   %edx
    pushl   %ds
    pushl   %es
    pushl   %fs
    movl    $0x10, %eax
    movw    %ax, %ds
    movw    %ax, %es
    movl    $0x17, %eax
    movw    %ax, %fs
    movb    $0x20, %al
    outb    %al, $0xA0
    jmp     1f
1:  jmp     1f
1:  xorl    %edx, %edx
    movl    %edx, hd_timeout
    xchgl   do_hd, %edx
    testl   %edx, %edx
    jne     1f
    movl    $unexpected_hd_interrupt, %edx
1:  call    *%edx
    movb    $0x20, %al
    outb    %al, $0x20
    popl    %fs
    popl    %es
    popl    %ds
    popl    %edx
    popl    %ecx
    popl    %eax
    iret

floppy_interrupt:
    pushl   %eax
    pushl   %ecx
    pushl   %edx
    pushl   %ds
    pushl   %es
    pushl   %fs
    movl    $0x10, %eax
    movw    %ax, %ds
    movw    %ax, %es
    movl    $0x17, %eax
    movw    %ax, %fs
    xorl    %edx, %edx
    xchgl   do_floppy, %edx
    testl   %edx, %edx
    jne     1f
    movl    $unexpected_floppy_interrupt, %edx
1:  call    *%edx
    movb    $0x20, %al
    outb    %al, $0x20
    popl    %fs
    popl    %es
    popl    %ds
    popl    %edx
    popl    %ecx
    popl    %eax
    iret

