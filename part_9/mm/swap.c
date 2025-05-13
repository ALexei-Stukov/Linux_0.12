#include <linux/mm.h>

unsigned long get_free_page() {
    register unsigned long __res asm("ax") = 0;

repeat:
__asm__("std ; repne ; scasb\n\t"
        "jne 1f\n\t"
        "movb $1,1(%%edi)\n\t"
        "sall $12, %%ecx\n\t"
        "addl %2, %%ecx\n\t"
        "movl %%ecx, %%edx\n\t"
        "movl $1024, %%ecx\n\t"
        "leal 4092(%%edx), %%edi\n\t"
        "xor  %%eax, %%eax\n\t"
        "rep; stosl;\n\t"
        "movl %%edx,%%eax\n"
        "1:"
        :"=a"(__res)
        :""(0), "i"(LOW_MEM), "c"(PAGING_PAGES),
        "D"(mem_map+PAGING_PAGES-1)
        :"dx");

    if (__res >= HIGH_MEMORY)
        goto repeat;
    return __res;
}

