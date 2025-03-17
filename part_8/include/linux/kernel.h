#ifndef _KERNEL_H
#define _KERNEL_H

//  内核级别的打印函数，支持可变参数。
int printk(const char* fmt, ...);

void verify_area(void * addr,int count);
int printf(const char * fmt, ...);
void console_print(const char * str);

#endif

