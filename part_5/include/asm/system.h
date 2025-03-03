#ifndef _SYSTEM_H
#define _SYSTEM_H

// 定义几个宏，这样写C语言代码的时候会更方便。
// 这些宏是针对X86架构的，不适用于其他架构。

/*
sti()：
汇编指令: sti
作用: 启用中断（Set Interrupt enable flag）。

cli()：
汇编指令: cli
作用: 禁用中断（Clear Interrupt enable flag）。

nop()：
汇编指令: nop
作用: 不执行任何操作，通常用于引入微小的延迟或在调试时占位。

iret()：
汇编指令: iret
作用: 从中断返回（Interrupt Return），恢复调用前的程序状态。
*/

/*
在原子操作中，经常会使用到这种手法：
{   
    cli();  // 禁用中断状态
    // 执行关键代码
    sti();  // 恢复中断状态
}
*/
#define sti()   __asm__("sti"::)
#define cli()   __asm__("cli"::)
#define nop()   __asm__("nop"::)
#define iret()  __asm__("iret"::)

#endif

