#include <linux/sched.h>
#include <linux/sys.h>
#include <asm/system.h>
#include <asm/io.h>

#include <unistd.h>

#define COUNTER 100
#define LATCH (1193180/HZ)

extern int system_call();
extern void timer_interrupt();

#define PAGE_SIZE 4096

/*
    任务联合体。这是个非常奇特的写法。
    char stack[PAGE_SIZE];是占据4096Byte的数组，而task_struct也在数组上，它和stack是重叠的，它在stack数组的低地址处。
    所以实际上，stack这个数组能使用的范围是(4096 - sizeof(struct task_struct))
    这种写法可以保证 task_union 结构一定是4KB大小。
*/
union task_union {
    struct task_struct task;
    char stack[PAGE_SIZE];
};

/*
    每个进程都对应着一个task_union，而初始进程init_task是我们自己手动在代码里构造的。
    我们专门设计一个宏，用于构造init_task。
*/
static union task_union init_task = {INIT_TASK, };

struct task_struct * current = &(init_task.task);
struct task_struct * task[NR_TASKS] = {&(init_task.task), };

/*
    这是用户态用到的栈空间
*/
long user_stack[PAGE_SIZE >> 2];

//在32位Linux平台上，long是4个Byte。所以数组user_stack一共占据  4 * 1024 个 Byte,刚好是一个内存页的大小。

struct{
    long    *a;     //在32位架构下，任何类型的指针都是4个Byte
    short   b;      //2个Byte
} stack_start = { &user_stack[PAGE_SIZE >> 2],0x10 };

//  stack_start是全局变量（具有全局可见性）。它的第一个成员a指向user_stack数组，另一个成员b是0x10


/*
    进程调度是由时钟中断来发起的。
    时钟中断会在调度模块启动时被打开，每一次时钟中断都会引发一次进程调度，不同的进程被缓入缓出，以此达到单核多任务的效果。
    时钟中断的响应函数是timer_interrup(),其中调用了do_timer(long cpl)函数。

    这里让时钟中断 = 100HZ
*/
int clock = COUNTER;

/*
    进程调度。目前Linux是一个单核多进程的操作系统，所以调度任务是不复杂的。
    执行哪个进程，就把对应的tss和ldt装载进去，被换出的进程就保存上下文。
*/
void schedule() {
    int i,next,c;
    struct task_struct ** p;

    while(1) {
        c = -1;
        next = 0;
        i = NR_TASKS;
        p = &task[NR_TASKS];

        while (--i) {
            if (!*--p)
                continue;

            if ((*p)->state == TASK_RUNNING && (*p)->counter > c)
                c = (*p)->counter, next = i;
        }

        if (c) break;
        for(p = &LAST_TASK ; p > &FIRST_TASK ; --p) {
            if (!(*p))
                continue;

            (*p)->counter = ((*p)->counter >> 1) + (*p)->priority;
        }
    }
    switch_to(next);
}

/*
    让一个进程睡眠。因为原来的进程陷入了睡眠，所以需要进行一次调度，换一个进程上来。
    连续几个进程可能都在睡眠。如果新选择的进程也在睡眠，此时要分情况讨论。
    如果是不可中断睡眠，那么只能放弃，寻找下一个。
    如果是可中断睡眠，那么就把它唤醒。
*/
static inline void __sleep_on(struct task_struct** p, int state) {
    /*
        让当前进程陷入睡眠
    */
    struct task_struct* tmp;

    if (!p)
        return;
    if (current == &(init_task.task))
        panic("task[0] trying to sleep");

    tmp = *p;
    *p = current;
    current->state = state;

    /*
        当前进程休眠了。
        尝试寻找一个新进程换上来。
    */
repeat:
    schedule();

    if (*p && *p != current) {
        (**p).state = 0;
        current->state = TASK_UNINTERRUPTIBLE;
        goto repeat;
    }

    if (!*p)
        printk("Warning: *P = NULL\n\r");
    *p = tmp;
    if (*p)
        tmp->state = 0;
}

/*
    休眠一个进程，可中断睡眠。可以理解为，这个进程可能在睡眠中就会被唤醒。
*/
void interruptible_sleep_on(struct task_struct** p) {
    __sleep_on(p, TASK_INTERRUPTIBLE);
}

/*
    休眠一个进程，不可中断睡眠。可以理解为，这个睡眠无论如何都要完整执行。
*/
void sleep_on(struct task_struct** p) {
    __sleep_on(p, TASK_UNINTERRUPTIBLE);
}

/*
    唤醒一个进程
*/
void wake_up(struct task_struct **p) {
    if (p && *p) {
        if ((**p).state == TASK_STOPPED)
            printk("wake_up: TASK_STOPPED");
        if ((**p).state == TASK_ZOMBIE)
            printk("wake_up: TASK_ZOMBIE");
        (**p).state=0;
    }
}

/**/
void do_timer(long cpl) {
    if ((--current->counter)>0) return;
    current->counter=0;
    if (!cpl) return;
    schedule();
}




/*
    调度器初始化
*/
void sched_init() {
    int i;
    
    /*
        当前进程的tss是哪个。
    */
    struct desc_struct * p;
    
    //注册初始进程的tss和ldt
    set_tss_desc(gdt + FIRST_TSS_ENTRY, &(init_task.task.tss));
    set_ldt_desc(gdt + FIRST_LDT_ENTRY, &(init_task.task.ldt));
    
    /*
        开始运行的时候只有init一个进程，所以只有task[0]是有元素的，其余的task[i]都要清空。
        同时，装载TSS LDT的描述符也一样，只有init的tss和ldt，其余的位置都要清空。
    */
    p = gdt+2+FIRST_TSS_ENTRY;
    for(i=1;i<NR_TASKS;i++) {
        task[i] = 0;
        p->a = p->b = 0;
        p++;
        p->a = p->b = 0;
        p++;
    }

    __asm__("pushfl; andl $0xffffbfff, (%esp); popfl");

    ltr(0);
    lldt(0);

    /* open the clock interruption! */
    /*
        打开时钟中断。
    */
    outb_p(0x36, 0x43);
    outb_p(LATCH & 0xff, 0x40);
    outb(LATCH >> 8, 0x40);
    set_intr_gate(0x20, &timer_interrupt);
    outb(inb_p(0x21) & ~0x01, 0x21);
    
    //打开0x80中断。
    // system_gate 和 intr_gate 的区别在于特权级。之前是没有实现用户态，所以只能使用系统特权级。
    // 现在已经有用户态了，所以需要把系统调用设置为用户态。这是非常有必要的。
    // 因为访问越界是有可能发生的，程序可能会访问到不属于自己的段。而特权级就是最后一道壁垒，访问越界可以被特权级拦下来，保护最重要的段不被篡改。
    set_system_gate(0x80, &system_call);
}


/*
    定义两个测试函数，检查进程调度的功能是否正常。
*/
void test_a(void) {
    char b;
    int n = 0;
    while ((n = read(0, &b, 1)) > 0) {
        write(1, &b, n);
    }

    __asm__("movl $0x0, %edi\n\r"
            "movw $0x1b, %ax\n\t"
            "movw %ax, %gs \n\t"
            "movb $0x0c, %ah\n\r"
            "movb $'A', %al\n\r"
            "loopa:\n\r"
            "movw %ax, %gs:(%edi)\n\r"
            "jmp loopa");
}
     
void test_b(void) {
    __asm__("movl $0x0, %edi\n\r"
            "movw $0x1b, %ax\n\t"
            "movw %ax, %gs \n\t"
            "movb $0x0f, %ah\n\r"
            "movb $'B', %al\n\r"
            "loopb:\n\r"
            "movw %ax, %gs:(%edi)\n\r"
            "jmp loopb");
}