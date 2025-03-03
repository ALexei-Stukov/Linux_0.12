#ifndef _IO_H
#define _IO_H

/*
outb_p(value, port) 宏用于将一个字节值输出到指定的I/O端口。
(value)：将第一个操作数（AL寄存器）赋值为value。
(port)：将第二个操作数（DX寄存器）赋值为port。

使用 outb %%al, %%dx 指令，将AL寄存器中的值输出到DX寄存器指定的端口。
\tjmp 1f\n 和 1:\tjmp 1f\n 是汇编指令的一部分，用于生成无操作（NOP）指令。通过多次跳跃同一个位置，实现 NOP 的效果。
*/
#define outb_p(value, port)     \
__asm__("outb %%al, %%dx\n"     \
        "\tjmp 1f\n"            \
        "1:\tjmp 1f\n"          \
        "1:" :: "a"(value), "d"(port))

#endif

