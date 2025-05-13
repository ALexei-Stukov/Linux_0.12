#include <linux/sched.h>

unsigned long HIGH_MEMORY = 0;

unsigned char mem_map [ PAGING_PAGES ] = {0,};

#define copy_page(from,to) \
__asm__("cld ; rep ; movsl"::"S" (from),"D" (to),"c" (1024):)

void free_page(unsigned long addr) {
    if (addr < LOW_MEM) return;
    if (addr >= HIGH_MEMORY)
        printk("trying to free nonexistent page");

    addr -= LOW_MEM;
    addr >>= 12;
    if (mem_map[addr]--) return;
    mem_map[addr]=0;
    printk("trying to free free page");
}

int free_page_tables(unsigned long from,unsigned long size) {
    unsigned long *pg_table;
    unsigned long * dir, nr;

    if (from & 0x3fffff)
        printk("free_page_tables called with wrong alignment");
    if (!from)
        printk("Trying to free up swapper memory space");
    size = (size + 0x3fffff) >> 22;
    dir = (unsigned long *) ((from>>20) & 0xffc);

    for ( ; size-->0 ; dir++) {
        if (!(1 & *dir))
            continue;
        pg_table = (unsigned long *) (0xfffff000 & *dir);
        for (nr=0 ; nr<1024 ; nr++) {
            if (*pg_table) {
                if (1 & *pg_table)
                    free_page(0xfffff000 & *pg_table);
                *pg_table = 0;
            }
            pg_table++;
        }
        free_page(0xfffff000 & *dir);
        *dir = 0;
    }
    invalidate();
    return 0;
}

int copy_page_tables(unsigned long from,unsigned long to,long size) {
    unsigned long * from_page_table;
    unsigned long * to_page_table;
    unsigned long this_page;
    unsigned long * from_dir, * to_dir;
    unsigned long nr;

    if ((from&0x3fffff) || (to&0x3fffff)) {
        printk("copy_page_tables called with wrong alignment");
    }

    /* Get high 10 bits. As PDE is 4 byts, so right shift 20.*/
    from_dir = (unsigned long *) ((from>>20) & 0xffc);
    to_dir = (unsigned long *) ((to>>20) & 0xffc);

    size = ((unsigned) (size+0x3fffff)) >> 22;
    for( ; size-->0 ; from_dir++,to_dir++) {
        if (1 & *to_dir)
            printk("copy_page_tables: already exist");
        if (!(1 & *from_dir))
            continue;

        from_page_table = (unsigned long *) (0xfffff000 & *from_dir);
        if (!(to_page_table = (unsigned long *) get_free_page()))
            return -1;

        *to_dir = ((unsigned long) to_page_table) | 7;
        nr = (from==0)?0xA0:1024;

        for ( ; nr-- > 0 ; from_page_table++,to_page_table++) {
            this_page = *from_page_table;
            if (!this_page)
                continue;
            if (!(1 & this_page))
                continue;

            this_page &= ~2;
            *to_page_table = this_page;

            if (this_page > LOW_MEM) {
                *from_page_table = this_page;
                this_page -= LOW_MEM;
                this_page >>= 12;
                mem_map[this_page]++;
            }
        }
    }
    invalidate();
    return 0;
}

void un_wp_page(unsigned long * table_entry) {
    unsigned long old_page,new_page;
    old_page = 0xfffff000 & *table_entry;

    if (old_page >= LOW_MEM && mem_map[MAP_NR(old_page)]==1) {
        *table_entry |= 2;
        invalidate();
        return;
    }

    new_page=get_free_page();
    if (old_page >= LOW_MEM)
        mem_map[MAP_NR(old_page)]--;
    copy_page(old_page,new_page);
    *table_entry = new_page | 7;
    invalidate();
}

void do_wp_page(unsigned long error_code, unsigned long address) {
    if (address < TASK_SIZE)
        printk("\n\rBAD! KERNEL MEMORY WP-ERR!\n\r");

    un_wp_page((unsigned long *)
            (((address>>10) & 0xffc) + (0xfffff000 &
                *((unsigned long *) ((address>>20) &0xffc)))));
}

void write_verify(unsigned long address) {
	unsigned long page;

	if (!( (page = *((unsigned long *) ((address>>20) & 0xffc)) )&1))
		return;
	page &= 0xfffff000;
	page += ((address>>10) & 0xffc);
	if ((3 & *(unsigned long *) page) == 1)  /* non-writeable, present */
		un_wp_page((unsigned long *) page);
	return;
}

void mem_init(long start_mem, long end_mem) {
    int i;

    HIGH_MEMORY = end_mem;

    for (i = 0; i < PAGING_PAGES; i++) {
        mem_map[i] = USED;
    }

    i = MAP_NR(start_mem);
    end_mem -= start_mem;
    end_mem >>= 12;
    while (end_mem--) {
        mem_map[i++] = 0;
    }
}

