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

/*
        将一个字节的值输出到指定的I/O端口，且没有nop停顿。
*/
#define outb(value,port) \
__asm__ ("outb %%al,%%dx"::"a" (value),"d" (port))

/*
        这是一个在 x86 架构下用于从 I/O 端口读取数据的宏定义。

        volatile 是 C 语言中的一个类型修饰符，它告诉编译器：被修饰的变量的值可能会在程序的控制流之外被改变。主要用途如下：
        防止编译器优化：
                编译器通常会对代码进行优化，但对于 volatile 变量，编译器每次都必须从内存中重新读取值，而不能使用寄存器中的缓存值。
        主要使用场景：
                硬件寄存器的访问（如：内存映射的外设寄存器）
                多线程共享的变量
                中断服务程序中修改的变量
        但volatile 不能保证原子性，不能替代同步机制，仅保证变量的读写操作不会被编译器优化掉（包括重排）。
        比如这种手法:
                
                volatile int sensor_value; // 声明一个 volatile 变量

                while (sensor_value == 0) {
                        // 这个循环不会被编译器优化掉
                        // 每次都会重新从内存读取 sensor_value 的值
                }
        加入了volatile，使得 ("inb %%dx,%%al":"=a" (_v):"d" (port)) 这条指令不会被编译器优化（包括重排）。

        inb %%dx,%%al" 是 x86 汇编指令：从端口中读取1个Byte的值，存入AL寄存器。
        端口由dx寄存器指定，dx寄存器的值由变量port提供。
        AL寄存器的值最终会被存入变量_v中。

        这个宏常用于：键盘控制器通信、读取硬件状态、与其他 I/O 设备通信。
*/
#define inb(port) ({ \
    unsigned char _v; \
    __asm__ volatile ("inb %%dx,%%al":"=a" (_v):"d" (port)); \
    _v; \
})
#define inb_p(port) ({ \
        unsigned char _v; \
        __asm__ volatile ("inb %%dx,%%al\n" \
            "\tjmp 1f\n" \
            "1:\tjmp 1f\n" \
            "1:":"=a" (_v):"d" (port)); \
        _v; \
        })
#endif

