#include <linux/kernel.h>
#include <linux/sched.h>
/*
    如果发生内核错误，应该显示出报错信息，并且进入无限循环。
    这样一来，用户就不得不重启电脑了。
*/
volatile void panic(const char * s) {
    printk("Kernel panic: %s\n\r",s);
    if (current == task[0])
        printk("In swapper task - not syncing\n\r");
    for(;;);
}
