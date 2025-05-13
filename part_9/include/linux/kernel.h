#ifndef _KERNEL_H
#define _KERNEL_H

void verify_area(void * addr,int count);
int printf(const char * fmt, ...);
int printk(const char* fmt, ...);
void console_print(const char * str);

#endif

