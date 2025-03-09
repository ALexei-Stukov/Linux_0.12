#ifndef _SCHED_H
#define _SCHED_H

//进程切换的频率
#define HZ 100

//最大进程数量
#define NR_TASKS 64

//一个进程占据的内存大小，目前只有4MB。
#define TASK_SIZE       0x04000000

//编译期检查，确认一个任务占据的内存大小是不是4MB的倍数，不是则不允许通过。
#if (TASK_SIZE & 0x3fffff)
#error "TASK_SIZE must be multiple of 4M"
#endif

//我们的目标是设计一个32位系统，共支持4GB内存。假设不考虑什么超大的内存申请，进程的所有内存需求都在属于自己的内存中得到解决，那么必须有 进程数量*进程内存大小 = 4GB
#if (((TASK_SIZE>>16)*NR_TASKS) != 0x10000)
#error "TASK_SIZE*NR_TASKS must be 4GB"
#endif

//定义两个宏，可以让我们简便地找到第一个和最后一个进程。FIRST_TASK不可能是一个空对象，但是LAST_TASK可能是一个空对象（进程数量不足64个）。
#define FIRST_TASK task[0]
#define LAST_TASK task[NR_TASKS-1]

#include <linux/head.h>
#include <linux/mm.h>
 
//定义各种进程状态
#define TASK_RUNNING            0  //运行中
#define TASK_INTERRUPTIBLE      1  //可中断（暂停）
#define TASK_UNINTERRUPTIBLE    2  //不可中断（暂停）
#define TASK_ZOMBIE             3  //僵尸进程
#define TASK_STOPPED            4  //进程已停止

//定义NULL这个宏。从现在开始，如果有必要，可以让函数返回NULL了。
#ifndef NULL
#define NULL ((void *) 0)
#endif

/*
    引入内存页管理的函数
*/
extern int copy_page_tables(unsigned long from, unsigned long to, long size);
extern int free_page_tables(unsigned long from, unsigned long size);

extern void trap_init();

/*
    把sched_init()移动到另一个位置，可以认为这是为了代码规范性。
    sched_init()函数只会在内核启动时被执行1次，此后应该再也不执行。这种不能多次使用的关键代码，不应该写在对外的头文件内。
*/
//void sched_init();
extern void sched_init();
extern void schedule();     //这个函数负责具体的进程调度

extern void panic(const char* s);   // 引入内核错误处理函数。万一进程调度出现大问题，直接报错，让操作系统陷入无限循环。

void test_a();
void test_b();

/*
    即使C语言没有直接支持面向对象，但是同样可以使用面向对象的手法。
    无论是什么进程，都被视为一个task。
    在Linux0.12,NR_TASKS的值是64，也就是说，同时最多存在64个进程。
*/
extern struct task_struct *task[NR_TASKS];
extern struct task_struct *current;

/*
    预定义 int() 类型的指针来描述函数。
    这种操作和封装任务队列的手法是非常像的。
*/
typedef int (*fn_ptr)();

/*
    在操作系统理论中，进程是资源分配的最小单位，且进程由PCB（procrss control block 进程控制块）表述。Linux沿用了这个设计，不过名为 task_struct。
    就目前这个版本来说，Linux的task_struct只是用了代码段和数据段。
    ldt就是“本地(local)描述符表”，和gdt性质相同，只是作用范围不同而已。由于ldt数组的0号元素默认不使用，所以ldt中有3个元素： 0号预留 1号2号作为代码段和数据段。
    tss是任务状态栈(task state stack)，这个东西很重要。随着实践进入深水区，这个栈的作用会被逐一介绍。

    tss_struct是为CPU准备的，当发生进程切换，CPU会自行把数据（比如寄存器状态）保存到tss_struct内，无需操作系统手动保存。
    所以说，这个结构是为CPU专门定制的。
    只要操作系统发出了进程切换信号，CPU就会开始保存进程的tss。当前进程的tss也有一个专门的寄存器来负责，那就是tr寄存器。加载tss也有一条专门的指令：ltr。
    tr寄存器中也是一个选择子，要根据选择子从GDT中找到描述符，才能得知tss的真实物理位置。
*/
struct tss_struct {
    long back_link;
    long esp0;
    long ss0;
    long esp1;
    long ss1;
    long esp2;
    long ss2;
    long cr3;
    long eip;
    long eflags;
    long eax, ecx, edx, ebx;
    long esp;
    long ebp;
    long esi;
    long edi;
    long es;
    long cs;
    long ss;
    long ds;
    long fs;
    long gs;
    long ldt;
    long trace_bitmap;
};

struct task_struct {
    long state;             
    long counter;           
    long priority;          
    long pid;               
    struct task_struct* p_pptr;
    struct desc_struct  ldt[3];
    struct tss_struct tss;
};


/*
    准备一些宏，开始加载初始进程的tss和ldt
    head.S中的GDT一共有5个元素，0号元素默认不使用，填充为0，1 2 3 号分别是内核代码段，内核数据段，显存段，4号默认不使用，填充为0。
    从第5项开始，就是每个进程各自的TSS和LDT描述符了。
    init进程的id为0，剩余进程的id从1开始。每个进程都占据2个描述符，如果一个进程的id为n(从0开始），那么它对应的tss就是第 5+2n个描述符，对应的ldt就是第 6+2n个描述符。
    这些宏都是用来计算进程n的 tss ldt 位置的。

    需要注意的是，
*/
#define FIRST_TSS_ENTRY 5
#define FIRST_LDT_ENTRY (FIRST_TSS_ENTRY + 1)
#define _TSS(n) ((((unsigned long)n) << 4) + (FIRST_TSS_ENTRY << 3))    // 这个宏可以写成 (n * 16) + (5 * 8)，也就是 (2 * n * 8) + (5 * 8) = (5+2n) * 8。一个描述符8个Byte。起始也就是5 +2n 个描述符的位置。
#define _LDT(n) ((((unsigned long)n) << 4) + (FIRST_LDT_ENTRY << 3))    // 这个就是第6+2个描述符的位置。
#define ltr(n) __asm__("ltr %%ax"::"a"(_TSS(n)))        // 注册n号进程的tss
#define lldt(n) __asm__("lldt %%ax"::"a"(_LDT(n)))      // 注册n号进程的ldt

/*
    这个宏和进程调度有关。
    进程调度需要修改cpu当前的tss和ldt，所以需要用到特殊的汇编指令。
    这个宏在schedule()函数的末尾被调用。
*/
#define switch_to(n) {\
    struct {long a,b;} __tmp; \
    __asm__("cmpl %%ecx,current\n\t" \
            "je 1f\n\t" \
            "movw %%dx,%1\n\t" \
            "xchgl %%ecx,current\n\t" \
            "ljmp *%0\n\t" \
            "1:" \
            ::"m" (*&__tmp.a),"m" (*&__tmp.b), \
            "d" (_TSS(n)),"c" ((long) task[n])); \
}

/*
    这几个宏 _set_base _set_limit _get_base get_limit 和复制内存有关。
    详见 fork 函数的 int copy_mem(int nr, struct task_struct* p)
*/
#define _set_base(addr,base) \
__asm__("movw %%dx,%0\n\t" \
        "rorl $16,%%edx\n\t" \
        "movb %%dl,%1\n\t" \
        "movb %%dh,%2" \
        ::"m" (*((addr)+2)), \
        "m" (*((addr)+4)), \
        "m" (*((addr)+7)), \
        "d" (base) \
        :)

#define _set_limit(addr,limit) \
__asm__("movw %%dx,%0\n\t" \
        "rorl $16,%%edx\n\t" \
        "movb %1,%%dh\n\t" \
        "andb $0xf0,%%dh\n\t" \
        "orb %%dh,%%dl\n\t" \
        "movb %%dl,%1" \
        ::"m" (*(addr)), \
        "m" (*((addr)6)), \
        "d" (limit) \
        :"dx")

#define set_base(ldt,base) _set_base( ((char *)&(ldt)) , base )
#define set_limit(ldt,limit) _set_limit( ((char *)&(ldt)) , (limit-1)>>12 )

#define _get_base(addr) ({\
unsigned long __base; \
__asm__("movb %3,%%dh\n\t" \
    "movb %2,%%dl\n\t" \
    "shll $16,%%edx\n\t" \
    "movw %1,%%dx" \
    :"=d" (__base) \
    :"m" (*((addr)+2)), \
    "m" (*((addr)+4)), \
    "m" (*((addr)+7))); \
__base;})

#define get_base(ldt) _get_base( ((char *)&(ldt)) )

#define get_limit(segment) ({ \
unsigned long __limit; \
__asm__("lsll %1,%0\n\tincl %0":"=r" (__limit):"r" (segment)); \
__limit;})

/*
    第一个进程很特殊，它叫init进程。只有这个进程，是我们在代码中手动构造的。就用下面这个宏构造。
    其余所有进程都由init进程产生。

    {0, PAGE_SIZE + (long)&init_task, 0x10, 0, 0, 0, 0, (long)&pg_dir, \    // tss 的esp0 指向栈顶，而PAGE_SIZE + (long)&init_task刚好就是 联合体init_task的尾地址，刚好是stack数组的尾地址，也就是栈顶。
*/
#define INIT_TASK \
{                   \
    0,              \
    15,             \
    15,             \
    0,              \
    &init_task.task,\
    {               \
        {0, 0},     \
        {0xfff, 0xc0fa00},   \
        {0xfff, 0xc0f200},   \
    },              \
    {0, PAGE_SIZE + (long)&init_task, 0x10, 0, 0, 0, 0, (long)&pg_dir, \
        0, 0, 0, 0, 0, 0, 0, 0, \
        0, 0, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,   \
        _LDT(0), 0x80000000,    \
    },              \
}

#endif

