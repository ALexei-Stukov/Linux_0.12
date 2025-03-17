#ifndef _MM_H
#define _MM_H

#include <linux/kernel.h>

#define PAGE_SIZE 4096

extern unsigned long get_free_page();
extern void free_page(unsigned long addr);

/*
    这个宏定义 invalidate() 是用来刷新（使无效）处理器的 TLB（Translation Lookaside Buffer，页表缓存）的。
    具体作用是：
        通过重新加载 CR3 寄存器来强制刷新 TLB
        TLB 中的所有条目都会被标记为无效
        强制系统在下次内存访问时重新进行页表查询
    
    当操作系统对页表进行了一些处理之后，就需要重置CPU的缓存。
*/
#define invalidate() \
__asm__("movl %%eax,%%cr3"::"a" (0))


//可用内存空间从哪里开始
#define LOW_MEM 0x100000
extern unsigned long HIGH_MEMORY;

//可用内存为15MB，因为从起点开始的1MB要留给内核，剩下的 15MB 才是 缓冲区可分配空间
#define PAGING_MEMORY (15*1024*1024)
//对应的可用页面号
#define PAGING_PAGES (PAGING_MEMORY>>12)

//计算地址对应的页面id的宏
#define MAP_NR(addr) (((addr)-LOW_MEM)>>12)

//已使用被定义为了100
#define USED  100

extern unsigned char mem_map [ PAGING_PAGES ];

#endif

