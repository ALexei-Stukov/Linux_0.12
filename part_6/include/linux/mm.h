#ifndef _MM_H
#define _MM_H

//可用内存空间从哪里开始
#define LOW_MEM 0x100000
extern unsigned long HIGH_MEMORY;

//可用内存为15MB，因为从起点开始的1MB要留给内核，剩下的 15MB 才是 缓冲区+可分配空间
#define PAGING_MEMORY (15*1024*1024)
//对应的可用页面号
#define PAGING_PAGES (PAGING_MEMORY>>12)

//计算地址对应的页面id的宏
#define MAP_NR(addr) (((addr)-LOW_MEM)>>12)

//已使用被定义为了100
#define USED  100


#endif

