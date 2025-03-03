#define __LIBRARY__

#include <linux/tty.h>
#include <linux/kernel.h>

void main(void)
{
    tty_init();     //初始化tty，使得我们可以开始处理字符。

    for (int i = 0; i < 25; i++) {
        printk("this is line %d\n\r", i);
    }

    __asm__("int $0x80 \n\r"::);
    __asm__ __volatile__(
            "loop:\n\r"
            "jmp loop"
            ::);    
}