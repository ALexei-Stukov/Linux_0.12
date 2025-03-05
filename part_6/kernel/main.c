#define __LIBRARY__

#include <linux/tty.h>
#include <linux/kernel.h>

extern void mem_init(long start,long end);
#define EXT_MEM_K (*(unsigned short *)0x90002)  //setup 阶段获得的内存数据总算派上用场了

static long memory_end = 0;             //内存终点
static long buffer_memory_end = 0;      //缓冲区终点
static long main_memory_start = 0;      //内存起点

void main(void)
{
    //计算内存
    memory_end = (1<<20) + (EXT_MEM_K<<10);
    memory_end &= 0xfffff000;       //把内存取整为4096的倍数
    if (memory_end > 16*1024*1024)  //总内存最大16MB
        memory_end = 16*1024*1024;
    //这里主要决定buffer_memory_end（缓冲区终点）在哪里。缓冲区是[0,n)的内存空间，用途暂时不详。
    if (memory_end > 12*1024*1024)
        buffer_memory_end = 4*1024*1024;
    else if (memory_end > 6*1024*1024)
        buffer_memory_end = 2*1024*1024;
    else
        buffer_memory_end = 1*1024*1024;

    main_memory_start = buffer_memory_end;  //处理好缓冲区，剩下的就是主内存。
    mem_init(main_memory_start, memory_end);    //内存初始化
    
    //开启“门”
    trap_init();
    sched_init();

    tty_init();     //初始化tty，使得我们可以开始处理字符。

    for (int i = 0; i < 25; i++) {
        printk("this is line %d\n\r", i);
    }
    printk("\n\rmemory start: %d, end: %d\n\r", main_memory_start, memory_end);
    
    __asm__("int $0x80 \n\r"::);
     __asm__ __volatile__(
                "int $0x7f\n\r"
                 "int $0x80\n\r"
                "movw $0x1b, %%ax\n\r"
                "movw %%ax, %%gs\n\r"
                "movl $0, %%edi\n\r"
                "movw $0x0f41, %%gs:(%%edi)\n\r"
                 "loop:\n\r"
                 "jmp loop"
                 ::);
}