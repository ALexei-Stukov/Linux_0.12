#ifndef _TTY_H
#define _TTY_H

void con_init();    //构造控制台 console
void tty_init();    //构造电传打字机    tty
/*
    可以认为
    TTY：主要用于需要简单字符输入输出的应用程序，如旧式的终端设备或一些底层系统组件。
    Terminal：广泛应用于开发、调试和日常计算机操作，支持图形用户界面（GUI）和命令行界面（CLI）。
    Console：主要用于系统的管理和维护，特别是在需要高级权限和控制的场景下。
*/

//  控制台信息打印。
//  在目前这个阶段， tty 和 console还没有分开，所以 “tty的打印” 是直接调用 “控制台的打印” 来完成的
//  tty_init() 本质上也是封装的 con_init()
void console_print(const char* buf, int nr);

#endif

