.code32
.text
.globl startup_32
startup_32:
    # 如果bootsect.S和setup.S的作用都基本理清，那么这个head.S确实没什么难度。照着书本走一遍就好。
    movl $0x10,   %eax
    movw %ax,   %ds
    movw %ax,   %es
    movw %ax,   %fs
    movl $0x18, %eax 
    movw %ax,   %gs
    call setup_idt
    call setup_gdt
    call set_clock_idt
    int $0x80
    sti

    movl $0x18, %eax
    movl $0x0,   %edi
    movw %ax,   %gs
    movl $0x0,   %edi

    movb $0xf,  %ah
    movb $0x42,  %al
    movw %ax,   %gs:(%edi)

loop:
    jmp loop

setup_idt:
    leal ignore_int,%edx
    movl $0x00080000,%eax
    movw %dx,%ax
    movw $0x8e00,%dx
    leal idt,%edi
    movl $256,%ecx

rp_sidt:
    movl %eax,  (%edi)
    movl %edx,  4(%edi)
    addl $8,    %edi
    decl %ecx
    jne rp_sidt
    lidt idt_descr
    ret

setup_gdt:
    lgdt gdt_descr
    ret

ignore_int:
    # 我们现在还没有print函数，通过直接向显存写入字符来模拟它
    pushl   %eax
    pushl   %ecx
    pushl   %edx
    pushw   %ds
    pushw   %es
    pushw   %fs
    movl    $0x10,  %eax
    movw    %ax,    %ds
    movw    %ax,    %es
    movw    %ax,    %fs

    movl    $0x96,  %edi
    movb    $'I',   %al
    movb    $0x0c,  %ah
    movw    %ax,   %gs:(%edi)

    popw    %fs
    popw    %es
    popw    %ds

    popl    %edx
    popl    %ecx
    popl    %eax

    iret

set_clock_idt:
    leal clock_handle,  %edx
    movl $0x00080000,   %eax
    movw %dx,   %ax
    movw $0x8e00,   %dx
    leal idt,   %edi
    addl $0x100,    %edi
    movl %eax,  (%edi)
    movl %edx,  4(%edi)
    ret

clock_handle:
    movl $0x96,%edi
    incb %gs:(%edi)

    movb $0x20,%al
    outb %al,$0x20
    iret

.align 4
.word 0

gdt_descr:
    .word 256*8-1
    .long gdt

idt_descr:
    .word 256*8-1
    .long idt


idt:
    .fill 256,8,0
.align 8
gdt:
    .quad 0x0000000000000000    #4个Byte，第一个gdt不使用
    .quad 0x00c09a0000000fff
    .quad 0x00c0920000000fff
    .quad 0x00c0f20b8000ffff
    .quad 0x0000000000000000
    .fill 251,8,0