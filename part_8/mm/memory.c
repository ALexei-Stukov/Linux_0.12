#include <linux/sched.h>

unsigned long HIGH_MEMORY = 0;                  //  定义系统最高内存地址

unsigned char mem_map [ PAGING_PAGES ] = {0,};  //  内存页面映射数组
                                                /*
                                                    这个数组存在于历代版本中，但是每个版本的实现方法都略有差别。
                                                    在这个版本中，mem_map中的元素指的是“该内存页被引用的计数”这个结论听起来很奇特，但是稍微解释一下就很容易明白。
                                                    如果内存页没有被引用，显然引用计数是0。此时这段内存页是空闲的。
                                                    如果内存页已经被引用，对应的数组元素就不是0。目前Linus把 USED状态定义为了100，暂时不清楚原因。
                                                */

void write_verify(unsigned long address) {
    unsigned long page;

    if (!( (page = *((unsigned long *) ((address>>20) & 0xffc)) )&1))
        return;
    page &= 0xfffff000;
    page += ((address>>10) & 0xffc);
    if ((3 & *(unsigned long *) page) == 1)  /* non-writeable, present */
        un_wp_page((unsigned long *) page);
    return;
}
                                                     
/*
    在进一步介绍内存分配之前，必须要明确一个原则。
    目前的Linux内核使用了二级页表，一级页表和内存页之间是物理对应的。
    比如说，0号页表对应0-4MB，1号页表对应4-8MB，以此类推。

    但是二级页表和一级页表却可以是不对应的。
    二级页表是一个指针数组，每个元素都是指针，指向某个一级页表。
    如果希望能在物理上对应，那么就要做到：
    0号元素->0号一级页表
    1号元素->1号一级页表

    但这样存在一些不便，所以Linus选择在二级页表上做了虚拟化。也就是说，二级页表中的指针可能是这样的：
    0号元素->7号一级页表
    1号元素->18号一级页表

    为什么要这么做呢？因为内存管理是一个很复杂的任务，二级页表和一级页表若同样是在物理上对应的，那么“内存管理”的复杂性就会往上传递。
    我们当然希望把复杂性留在内存管理模块中，所以一定要做虚拟化，把内存模块封装好，对外留出易于调用的接口，而不是把复杂性传递到其它模块中。
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

/*
    这个宏用于从 内存页A 复制内容到 内存页B，会复制4096Byte数据。
*/
#define copy_page(from,to) \
__asm__("cld ; rep ; movsl"::"S" (from),"D" (to),"c" (1024):)


/*
    在Linux0.11中，Linus已经加入了伙伴算法的基本框架。伙伴算法经过不断优化，沿用至今（6.x版本内核）。
    不得不说，Linus确实很NB。

    不过目前我们的代码还不包括伙伴算法，后续应该会逐步添加进去。
*/
void free_page(unsigned long addr) {
    if (addr < LOW_MEM) return;                     //尝试释放操作系统的预留地址。这是不合理的，所以不做任何操作。
    if (addr >= HIGH_MEMORY)
        printk("trying to free nonexistent page");  //尝试释放不存在或者未经管理的内存地址。（如果物理内存有100G，但操作系统只管理着其中的4G，那么超过4G的内存就属于“存在但未经管理”或者“不存在”的内存。）

    addr -= LOW_MEM;                //计算出地址对应的内存页号
    addr >>= 12;                    // 2^12 = 4096

    if (mem_map[addr]--) return;    //如果内存页已经被释放，就不做任何操作。
    mem_map[addr]=0;                //释放一个内存页
    printk("trying to free free page");
}

/*
    指定一个地址作为起点，给定需要释放的字节数，以一级页表为最小单位，释放掉若干个一级页表。
    地址必须指向一级页表的起点。
    
    注意，即使给顶了一级页表的地址，我们还是先把一级页表转换成二级页表元素，然后通过二级页表元素查到真实的物理内存，然后再释放内存的。
    这就是内存虚拟化的精髓。
    程序操作的永远是虚拟内存地址，其真实的物理地址程序是不知道的。也没必要让程序知道。
*/
int free_page_tables(unsigned long from,unsigned long size) {
    unsigned long *pg_table;
    unsigned long * dir, nr;
    
    //地址必须是4M的整数倍。
    if (from & 0x3fffff)
        printk("free_page_tables called with wrong alignment");
    
    //页表地址不能是0，因为从0开始的第一页被操作系统保留
    if (!from)
        printk("Trying to free up swapper memory space");
    
    /*
        根据要释放的字节数size，计算出要释放多少个页表。（一级页表）
        释放数量由变量仍由size存储。
        0x3fffff是 2^22-1，这个数值能辅助我们计算。
        举几例子，
        如果要释放0个字节，说明无需释放页表。0+0x3fffff >> 22 = 0
        如果要释放1个字节，即使不足1个页表（4MB）的大小，但我们仍然需要释放1个页表。0x1 + 0x3fffff >> 22 = 1
        如果要释放4299161个字节(4.1MB)，显然需要释放2个页表。 0x419999 + 0x3fffff >> 22 = 0x819998 >> 22 = 2
    */
    size = (size + 0x3fffff) >> 22;

    /*
        根据地址起点，计算出要从哪个一级页表开始操作。
        目前我们的系统分为二级页表映射。
        二级页表有4096个元素，每个元素都是指针(4Byte)，指向1个一级页表。
        一级页表有4096个元素，每个元素都是指针(4Byte)，指向1个内存页。

        现在我们需要找出：地址from对应的二级页表id是多少.
        如果获得了二级页表id，那么该元素的地址就是 dir = 4 * （id - 1）
        
        只要找到了地址，就可以使用 *dir 找到正确的一级页表。

        这就是代码逻辑，其实也不复杂。但是Linus为了提高性能，使用了难懂的位运算。接下来我会试着拆解整个过程。
    */
    dir = (unsigned long *) ((from>>20) & 0xffc);
    /*
        dir 就是地址from对应的二级页表的地址。对其取地址 *dir，就可以获得正确的一级页表指针。
        dir是这样算出来的：
        
        目前我们的地址有32位，目标是支持4GB内存。一个一级页表管理1024个页，共4MB内存。
        那么只需要执行
        id = from >> 22，即可得到地址from对应的一级页表id（从0开始计算）
        一级页表id同时也是二级页表的元素下标。

        比如说，from = 0x00800000 = 8MB,它指向第3个一级页表的起点，所以对应的一级页表id是2。
        0x00800000 >> 22 = 二进制(0x10) = 2。
        二级页表的第3个元素，其下标正好也是2。

        知道了一级页表id（即二级页表元素下标）之后，我们需要根据id获得对应的一级页表指针。也就是说：
        dir = 二级页表[id]
        那么，二级页表[id] 这个表达式究竟该怎么写呢？
        因为二级页表中都是指针，每个指针占据4个Byte，而二级页表又是从0开始，所以
        
        二级页表[id] = 0 + 4 * id
        经过化简，不难得到
        二级页表[id] = (from >> 20)。

        现在我们还剩下最后一步：数据的正确性保障。
        
        因为二级页表内都是指针，每个元素都占据4Byte，且二级页表从0开始，所以 dir肯定也指向一个 4Byte 倍数的地址。
        因为二级页表数组内最多1024个元素，所以二级页表最多占据4096个Byte（即2^12个Byte）。
        那么，出于安全考虑，我们强制让dir满足这样的条件:
            必须是4Byte的倍数
            必须在 [0 , 2^12-1] 之间。
        所以我们需要对地址做与运算，(from >> 20) & (1111 1111 1100),即 (from >> 20) & 0xffc。
        有了这一层保障，我们才能把结果赋值给dir。 
    */

    //他妈的，size-->0是一个非常逆天的写法，我感觉完全没必要。
    // for ( ; size-->0 ; dir++) {
    for ( ; size>0 ; dir++,size--) {
        /*
            如果二级页表中的元素位0，说明对应位置的内存已经空闲，无需处理。
        */
        if (!(1 & *dir))
            continue;
        /*
            获得一级页表指针。因为一级页表指针一定是从4096开始的，所以这里同样是先用与运算确保数据安全性，然后再赋值
        */
        pg_table = (unsigned long *) (0xfffff000 & *dir);
        
        /*
            尝试释放一级页表管理的每一个内存页。
        */
        for (nr=0 ; nr<1024 ; nr++) {
            if (*pg_table) {
                if (1 & *pg_table)
                    free_page(0xfffff000 & *pg_table);
                *pg_table = 0;
            }
            pg_table++;
        }
        /*
            一级页表所管理的内存页已经释放完成了。那么一级页表本身也没有再保留的必要。
            一级页表本身还占据1个内存页，可以把它也释放掉。
        */
        free_page(0xfffff000 & *dir);
        /*
            然后把二级页表对应的位置设置为0。
        */
        *dir = 0; 
    }
    invalidate();   //刷新处理器的页表缓存
    return 0;
}

/*
    指定一个地址作为起点，另一个地址作为终点，给定需要释放的字节数，以一级页表为最小单位，复制若干个一级页表的数据。
    两个地址必须指向一级页表的起点。
    
    同样的，from 和 to 是虚拟的内存地址，而不是真实的物理内存地址。
*/
int copy_page_tables(unsigned long from,unsigned long to,long size) {
    unsigned long * from_page_table;
    unsigned long * to_page_table;
    unsigned long this_page;
    unsigned long * from_dir, * to_dir;
    unsigned long nr;

    //地址正确性检查
    if ((from&0x3fffff) || (to&0x3fffff)) {
        printk("copy_page_tables called with wrong alignment");
    }

    //找到对应的二级页表地址，可以使用 *from_dir 和 *to_dir 获得对应的一级页表指针
    /* Get high 10 bits. As PDE is 4 byts, so right shift 20.*/
    from_dir = (unsigned long *) ((from>>20) & 0xffc);
    to_dir = (unsigned long *) ((to>>20) & 0xffc);

    //计算出size对应的一级页表数量。
    size = ((unsigned) (size+0x3fffff)) >> 22;

    /*
        使用for循环，一次复制1个一级页表。
    */
    // for( ; size-->0 ; from_dir++,to_dir++) {
    for( ; size>0 ; from_dir++,to_dir++,size--) {
        if (1 & *to_dir)
            printk("copy_page_tables: already exist");  //复制页表可能会造成数据覆盖。这里报一个提示。
        if (!(1 & *from_dir))
            continue;

        from_page_table = (unsigned long *) (0xfffff000 & *from_dir);

        // 找找看还有没有空闲页表。没有空闲页表的话，那显然无法完成页表复制的工作。
        if (!(to_page_table = (unsigned long *) get_free_page()))
            return -1;
        /*
            从这里可以感受到内存虚拟化的好处。因为是虚拟化，所以对一级页表的位置没有要求。
            我们从二级页表的角度来看，这几个被复制的页表是连续的。但是这些一级页表在物理上可以不连续。
            这样有助于应对内存碎片问题。
        */

        /*
            找到正确的页面之后，把二级页表项和正确的页面关联起来。
            注意这个 | 7,执行了 | 7 之后，结果尾部的二进制表示就是111。
            说明这个页表是用户态可读，可写，并且在内存中存在。
        */
        *to_dir = ((unsigned long) to_page_table) | 7;

        /*
            如果起点地址是0，那么nr = 0XA0，也就是十进制160。如果不是，那么nr = 十进制1024。
            nr是决定数据复制次数的变量。
            
            如果from是一般的一级页表，其中包含1024的页，那么显然要复制1024次。共复制4M的数据。
            但是如果from是0号一级页表，也就是二级页表的所在地， 那么只操作160个内存页。共640KB的数据。
            我猜测，这是为了避免复制敏感数据。
            确实，作者在书里说，“这是为了避免复制显存等特殊的内存段”。

        */
        nr = (from==0)?0xA0:1024;
        
        /*
            之后就是复制内存页的代码
        */
        for ( ; nr > 0 ; from_page_table++,to_page_table++,nr--) {
            this_page = *from_page_table;
            if (!this_page)
                continue;
            if (!(1 & this_page))
                continue;

            this_page &= ~2;
            *to_page_table = this_page;

            if (this_page > LOW_MEM) {
                *from_page_table = this_page;
                this_page -= LOW_MEM;
                this_page >>= 12;
                mem_map[this_page]++;
            }
        }
    }
    invalidate();   //刷新处理器的页表缓存
    return 0;
}

void un_wp_page(unsigned long * table_entry) {
    /*
        找到old_page到底是哪个页
    */
    unsigned long old_page,new_page;
    old_page = 0xfffff000 & *table_entry;

    /*
        写操作系统就是要这么小心，随时随地都要处理合法性的问题。
    */
    if (old_page >= LOW_MEM && mem_map[MAP_NR(old_page)]==1) {
        *table_entry |= 2;
        invalidate();   //刷新处理器的页表缓存
        return;
    }

    /*
        既然把 old_page 写入某个page 的操作不合法，那就获得一个 new_page，把old_page写进去就OK。
    */
    new_page=get_free_page();
    if (old_page >= LOW_MEM)
        mem_map[MAP_NR(old_page)]--;
    copy_page(old_page,new_page);
    *table_entry = new_page | 7;
    invalidate();   //刷新处理器的页表缓存
}

/*
    触发写保护错误(write protect)时的响应函数。
    调用un_wp_page进行处理。
    尝试对只读或不可写的内存区域进行写操作时会触发写保护trap，我们需要处理这个trap

    处理写时复制用到了这个响应函数
*/
void do_wp_page(unsigned long error_code, unsigned long address) {
    if (address < TASK_SIZE)
        printk("\n\rBAD! KERNEL MEMORY WP-ERR!\n\r");
    /*
        要根据地址，计算出是哪个页面需要被处理。
    */
    un_wp_page((unsigned long *)
            (((address>>10) & 0xffc) + (0xfffff000 &
                *((unsigned long *) ((address>>20) &0xffc)))));
}
