#ifndef _HEAD_H
#define _HEAD_H

/* 
    typedef struct desc_struct {
        unsigned long a, b;
    } desc_table[256];
 
    这个写法有点奇特。它把一个 desc_struct[256] 数组定义为了 desc_tabale类型，等价于分开写：

    struct desc_struct {
        unsigned long a, b;
    };
    typedef struct desc_struct desc_table[256]; 
*/
struct desc_struct {
    unsigned long a, b;
};
typedef struct desc_struct desc_table[256]; 

//  在启动阶段就准备好的二级页表，中断描述符表，全局描述符表，我们需要把它们以C语言的形式加入内核。这样才方便管理。
extern unsigned long pg_dir[1024];
extern desc_table idt,gdt;

#endif

