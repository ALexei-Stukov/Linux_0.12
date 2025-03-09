.globl page_fault

# 这是和页有关的错误

page_fault:
    xchgl   %eax, (%esp)
    pushl   %ecx
    pushl   %edx
    pushl   %ds
    pushl   %es
    pushl   %fs
    movl    $0x10, %edx
    movw    %dx, %ds
    movw    %dx, %es
    movw    %dx, %fs
    movl    %cr2, %edx
    pushl   %edx
    pushl   %eax
    testl   $1, %eax
    jne     1f      # 根据不同的情况，决定是否要执行do_wp_page
    jmp     2f      #
1:  call    do_wp_page
2:  addl    $8, %esp
    popl    %fs
    popl    %es
    popl    %ds
    popl    %edx
    popl    %ecx
    popl    %eax
    iret

