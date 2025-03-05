#include <linux/sched.h>
#include <asm/system.h>

extern int system_call();

#define PAGE_SIZE 4096
long user_stack[PAGE_SIZE >> 2];

//在32位Linux平台上，long是4个Byte。所以数组user_stack一共占据  4 * 1024 个 Byte,刚好是一个内存页的大小。

struct{
    long    *a;     //在32位架构下，任何类型的指针都是4个Byte
    short   b;      //2个Byte
} stack_start = { &user_stack[PAGE_SIZE >> 2],0x10 };

//  stack_start是全局变量（具有全局可见性）。它的第一个成员a指向user_stack数组，另一个成员b是0x10


//好，我们要开始准备调度器了。
//调度器首先要做的事情是设置0x80号中断，这样一来80号中断才可以被响应。80号中断是Linux系统常用的调用。
void sched_init() {
    set_intr_gate(0x80, &system_call);
}