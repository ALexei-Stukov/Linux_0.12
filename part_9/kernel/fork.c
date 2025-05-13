#include <errno.h>
#include <string.h>

#include <asm/system.h>
#include <linux/sched.h>
#include <linux/kernel.h>

extern void write_verify(unsigned long address);

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

    new_data_base = new_code_base = nr * TASK_SIZE;
    set_base(p->ldt[1],new_code_base);
    set_base(p->ldt[2],new_data_base);
    if (copy_page_tables(old_data_base,new_data_base,data_limit)) {
        free_page_tables(new_data_base,data_limit);
        return -ENOMEM;
    }

    return 0;
}

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
    p->tss.eip = eip;
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

