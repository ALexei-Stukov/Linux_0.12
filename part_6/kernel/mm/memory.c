#include <linux/sched.h>

unsigned long HIGH_MEMORY = 0;                  //  定义系统最高内存地址

unsigned char mem_map [ PAGING_PAGES ] = {0,};  //  内存页面映射数组
                                                /*
                                                    这个数组存在于历代版本中，但是每个版本的实现方法都略有差别。
                                                    在这个版本中，mem_map中的元素指的是“该内存页被引用的计数”这个结论听起来很奇特，但是稍微解释一下就很容易明白。
                                                    如果内存页没有被引用，显然引用计数是0。此时这段内存页是空闲的。
                                                    如果内存页已经被引用，对应的数组元素就不是0。目前Linus把 USED状态定义为了100，暂时不清楚原因。
                                                */
void mem_init(long start_mem, long end_mem) {
    int i;

    HIGH_MEMORY = end_mem;              //设置最高可用内存为 end_mem（最大内存）

    for (i = 0; i < PAGING_PAGES; i++) {
        mem_map[i] = USED;
    }                                   // 初始化阶段，每个内存都设置为被使用状态
                                        
    /*
        然后计算可用内存页面范围。因为有一部分是缓冲区，这部分永远被操作系统使用，永远都是使用状态。
    */
    i = MAP_NR(start_mem);  //  计算起始页面号
    end_mem -= start_mem;   //  计算可用内存大小
    end_mem >>= 12;         //  将字节数转换为页面数，计算出可用内存有多少页。（2^12 = 4096，即一页的大小）
    while (end_mem--) {
        mem_map[i++] = 0;   //最后将可用内存范围内的页面标记为空闲（0）
    }

}

