#include <errno.h>
#include <string.h>
 
#include <asm/system.h>
#include <linux/sched.h>
#include <linux/kernel.h>

extern void write_verify(unsigned long address);

/*
    这是系统调用fork的实现。

    一个进程对应的tss 和 ldt会占据 1个内存页。
    而fork的作用是复制进程，所以会对内存页进行复制，主要是复制原有进程的tss和ldt，然后做一些其它的改动。

    init进程的tss和ldt是我们手动设计的。而其它所有进程的tss和ldt都是复制init进程的tss和ldt。
    当然，复制完tss和ldt之后还得做一下改动。
*/

long last_pid = 0;



void verify_area(void * addr,int size) {
    unsigned long start;

    start = (unsigned long) addr;
    size += start & 0xfff;
    start &= 0xfffff000;
    start += get_base(current->ldt[2]);
    while (size>0) {
        size -= 4096;
        write_verify(start);
        start += 4096;
    }
}

/*
    复制进程内存。
    fork后的一瞬间，父子进程在内存上完全一致。
    但是从fork函数开始的控制流不同，最终导致父子进程走向不同的逻辑。

    i386的虚拟内存机制可以让每个进程都拥有独立的4GB虚拟内存，但是Linus当时认为每个进程不需要4GB空间，而是只需要64MB就够了。
    所以Linux只支持64个进程，每个进程可以分到64MB内存，大家一起共享4GB的内存。

    这里其实涉及到了写时复制技术。
    首先，copy_mem只复制了页表，而没有复制物理页。
    也就是说，两个进程的页表，实际上指向相同的物理页。

    然后,fork进程把这些页表设置为只读，这对父子进程都有效。
    如果父子进程只是读取页表内容，那么就不会有影响。
    但如果其中一个进程视图对这些页表执行写操作，那么此时会报写异常，操作系统就得以介入。
    操作系统会分配一个新的内存页给进程，并且把页表映射到这个新的页。

    这样一来，写入就将顺利进行。这就是大名鼎鼎的写时复制技术。
*/
int copy_mem(int nr, struct task_struct* p) {
    unsigned long old_data_base,new_data_base,data_limit;
    unsigned long old_code_base,new_code_base,code_limit;

    code_limit = get_limit(0x0f);
    data_limit = get_limit(0x17);
    old_code_base = get_base(current->ldt[1]);
    old_data_base = get_base(current->ldt[2]);
    if (old_data_base != old_code_base)
        panic("We don't support separate I&D");
    if (data_limit < code_limit)
        panic("Bad data_limit");
    
    new_data_base = new_code_base = nr * TASK_SIZE; //每个进程都拥有64MB的虚拟内存。data_base和code_base相同，都是虚拟内存的首地址。
    set_base(p->ldt[1],new_code_base);
    set_base(p->ldt[2],new_data_base);
    if (copy_page_tables(old_data_base,new_data_base,data_limit)) {
        free_page_tables(new_data_base,data_limit);
        return -ENOMEM;
    }

    return 0;
}

/*
    复制进程
*/
int copy_process(int nr,long ebp,long edi,long esi,long gs,long none,
        long ebx,long ecx,long edx, long orig_eax,
        long fs,long es,long ds,
        long eip,long cs,long eflags,long esp,long ss) {
    struct task_struct *p;

    p = (struct task_struct *) get_free_page();
    if (!p)
        return -EAGAIN;

    task[nr] = p;
    memcpy(p, current, sizeof(struct task_struct));

    p->pid = last_pid;
    p->p_pptr = current;

    p->tss.back_link = 0;
    p->tss.esp0 = PAGE_SIZE + (long)p;
    p->tss.ss0 = 0x10;
    p->tss.cr3 = current->tss.cr3;
    p->tss.eip = eip;               // 我们使用中断int来调用fork，而执行完fork之后，eip指向int的下一条指令。对于子进程来说，由fork产生的子进程一定要和父进程的eip相等。
    p->tss.eflags = eflags;
    p->tss.eax = 0;
    p->tss.ecx = ecx;
    p->tss.edx = edx;
    p->tss.ebx = ebx;
    p->tss.esp = esp;
    p->tss.ebp = ebp;
    p->tss.esi = esi;
    p->tss.edi = edi;
    p->tss.es  = es & 0xffff;
    p->tss.cs  = cs & 0xffff;
    p->tss.ss  = ss & 0xffff;
    p->tss.ds  = ds & 0xffff;
    p->tss.fs  = fs & 0xffff;
    p->tss.gs  = gs & 0xffff;
    p->tss.ldt = _LDT(nr);
    p->tss.trace_bitmap = 0x80000000;
    
    if (copy_mem(nr, p)) {
        task[nr] = NULL;
        free_page((long)p);
        return -EAGAIN;
    }

    set_tss_desc(gdt+(nr<<1)+FIRST_TSS_ENTRY,&(p->tss));
    set_ldt_desc(gdt+(nr<<1)+FIRST_LDT_ENTRY,&(p->ldt));

    return last_pid;
}

int find_empty_process() {
    int i;
repeat:
    if ((++last_pid)<0) last_pid=1;

    for(i=0 ; i<NR_TASKS ; i++) {
        if (task[i] && (task[i]->pid == last_pid))
            goto repeat;
    }

    for (i = 1; i < NR_TASKS; i++) {
        if (!task[i])
            return i;
    }

    return -EAGAIN;
}

