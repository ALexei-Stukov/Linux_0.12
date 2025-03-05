.code32
.text
.globl system_call
int_msg:
    .asciz  "In kernel interrupt\n\r"
/*
    这部分代码暂时只有一个作用：调用printk函数，打印一行字符。
*/
system_call:
    /* we do not have function _printk now, so trick it */
    pushl %eax
    pushl %ecx
    pushl %edx
    pushw %ds
    pushw %es
    pushw %fs
    movl  $0x10, %eax
    movw  %ax, %ds
    movw  %ax, %es
    movw  %ax, %fs
    /* call  _printk */
    pushl $int_msg
    call  printk
    popl  %eax
    popw  %fs
    popw  %es
    popw  %ds
    popl  %edx
    popl  %ecx
    popl  %eax
    iret

