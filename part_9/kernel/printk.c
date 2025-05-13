#include <stdarg.h>
#include <stddef.h>
#include <linux/kernel.h>

static char buf[1024];

extern int vsprintf(char* buf, const char* fmt, va_list args);

int printk(const char* fmt, ...) {
    va_list args;
    int i;

    va_start(args, fmt);
    i = vsprintf(buf, fmt, args);
    va_end(args);

    __asm__("pushw %%fs\n\t"
            "pushw %%ds\n\t"
            "popw  %%fs\n\t"
            "pushl %0\n\t"
            "pushl $buf\n\t"
            "pushl $0\n\t"
            "call  tty_write\n\t"
            "addl  $8, %%esp\n\t"
            "popl  %0\n\t"
            "popw  %%fs"
            ::"r"(i):"ax", "cx", "dx");

    return i;
}

