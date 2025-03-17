#define __LIBRARY__
#include <unistd.h>
#include <stdarg.h>

_syscall3(int,read,int,fd,const char *,buf,off_t,count)
_syscall3(int,write,int,fd,const char *,buf,off_t,count)
_syscall3(int, ioctl, unsigned int, fd, unsigned int, cmd, unsigned long, arg);

static char printbuf[1024];
extern int vsprintf(char* buf, const char* fmt, va_list args);

/*
    这个函数和printk相同，都是借助了vsprintf进行打印。不过printk是直接打印到console，而printf是使用了系统调用write。
*/
int printf(const char* fmt, ...) {
    va_list args;
    int i;

    va_start(args, fmt);
    write(1, printbuf, i = vsprintf(printbuf, fmt, args));
    va_end(args);

    return i;
}

