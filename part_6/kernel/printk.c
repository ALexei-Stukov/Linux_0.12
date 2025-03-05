#include <stdarg.h>
#include <stddef.h>
#include <linux/kernel.h>

static char buf[1024];

extern int vsprintf(char* buf, const char* fmt, va_list args);

int printk(const char* fmt, ...) {
    va_list args;
    int i;

    va_start(args, fmt);        //获得一个指向可变参数列表（char*）的指针args

    i = vsprintf(buf, fmt, args);   //解析可变参数列表。
                                    //vspriontf函数把参数列表做了转换，得到了  “可打印的字符串” ，同时返回了 “需要打印的字符总数” 。

    va_end(args);       //为了可移植性必须这么写。但是当Linus运行在x86平台上的时候，这个函数实际上是无效的。

    //准备好 “可打印的字符串” 和 “需要打印的字符总数” 后，调用tty_write函数，让tty_write函数执行信息打印。
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

