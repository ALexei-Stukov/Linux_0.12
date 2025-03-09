#ifndef _SYSTEM_H
#define _SYSTEM_H

// 定义几个宏，这样写C语言代码的时候会更方便。
// 这些宏是针对X86架构的，不适用于其他架构。

/*
    用于切换到用户态。

    现在我需要解释一下Linux特权级的相关内容。
    在不同的特权级下，进程能访问的段是不一样的。当然，进程用到的栈也是不一样的，这就是为什么进程的tss结构里面有esp0 和 esp3，它们分别代表着内核态的栈和用户态的栈。

    如果从用户态切换到内核态，Linux的做法是使用中断。这是一个很有趣的做法。
    因为用户态到内核态的互相切换本质上是“控制流的转移”，如果从用户态到内核态使用中断，那么栈上就会保存着中断之前的上下文。
    这样一来，从内核态到用户态只需一个iret指令，从中断返回，即可继续回到原有控制流。
    
    当然，别忘了切换使用的栈。esp一定要随着特权级变化而变化。
*/
#define move_to_user_mode() \
 __asm__("movl %%esp, %%eax\n\t" \
        "pushl $0x17\n\t"       \
        "pushl %%eax\n\t"       \
        "pushfl\n\t"            \
        "pushl $0x0f\n\t"       \
        "pushl $1f\n\t"         \
        "iret \n\t"              \
        "1:\tmovl $0x17, %%eax\n\t" \
        "movw %%ax, %%ds\n\t"   \
        "movw %%ax, %%es\n\t"   \
        "movw %%ax, %%fs\n\t"   \
        "movw %%ax, %%gs\n\t"   \
        :::"ax")


/*
sti()：
汇编指令: sti
作用: 启用中断（Set Interrupt enable flag）。

cli()：
汇编指令: cli
作用: 禁用中断（Clear Interrupt enable flag）。

nop()：
汇编指令: nop
作用: 不执行任何操作，通常用于引入微小的延迟或在调试时占位。

iret()：
汇编指令: iret
作用: 从中断返回（Interrupt Return），恢复调用前的程序状态。
*/

/*
在原子操作中，经常会使用到这种手法：
{   
    cli();  // 禁用中断状态
    // 执行关键代码
    sti();  // 恢复中断状态
}
*/
#define sti()   __asm__("sti"::)
#define cli()   __asm__("cli"::)
#define nop()   __asm__("nop"::)
#define iret()  __asm__("iret"::)



/*
    在继续编码之前，必须要把中断给搞清楚。
    如果一个中断出现，首先它会被发送到中断控制器。在X86架构上，中断控制器是APIC。其中CPU内部的控制器称为“本地APIC”。
    CPU是无法预知中断何时发生的，所以CPU不得不频繁读取APIC，检查有无中断发生。

    读取到中断之后，就要考虑做出一些响应，否则系统可能无法正常运行。比如说中断会堆积在APIC之类的（APIC在设计上支持中断队列）。
*/
/*
    X86架构总共 256 个中断向量（0-255），可以分为三类中断，分别是：
        0-31:    CPU 异常和故障（处理器预定义），15和20-31保留。这部分大多是不可屏蔽中断。
        32-47:   硬件 IRQ 中断（可屏蔽中断）
        48-255:  可用于软件中断和用户自定义。其中Linux常用80中断作为系统调用。

    为了应对这三种中断，操作系统中出现了“门”这个概念。
    硬件中断   ──→ INTR_GATE    中断门
    CPU异常    ──→ TRAP_GATE    陷阱门
    系统调用   ──→ SYSTEM_GATE/TRAP_GATE    系统调用门
    本质上来说，“门”就是对中断的响应。
*/
/*
    最后再来说一个比较特殊的现象。
    对于Linux而言，中断门必须是异步的。而陷阱门，系统调用门往往是同步的。
    因为 interrupt 往往源于外部设备，CPU不可能提前预知它的产生。因此针对 interrupt 的中断门也是异步处理。
    
    和interrupt不同， trap system_call 往往是在执行程序时候产生的。
    CPU执行了一条指令，可能这条指令就会引发 trap system_call，然后CPU可以马上使用 陷阱门、系统调用门 来处理相关请求。
    所以说，trap_gate和system_gate往往是同步处理的。
*/


/*
    以下这段代码等价于这样
                        ; 将addr放入寄存器edx，将0x00080000放入寄存器eax
    movw %dx, %ax       ; 将dx寄存器（edx的低16位）的值复制到ax寄存器（eax的低16位）
                        ; 此时eax = 0x00080000 变成了 0x0008xxxx (xxxx是addr的低16位)。
                        ; eax 的高16位是 0x0008,是段选择子。低16位是中断处理程序的地址（addr）的低16位。

    movw %0, %dx        ; 将立即数(0x8000 + (dpl << 13) + (type << 8))加载到dx
                        ; 这个立即数的构成：
                        ; 0x8000: 表示段描述符存在(P=1)
                        ; dpl << 13: 特权级别，左移13位
                        ; type << 8: 门类型(如中断门0xE，陷阱门0xF)，左移8位
                        ; 例如，如果 dpl=0，type=14(中断门)：0x8000 + (0 << 13) + (14 << 8) = 0x8E00

    movl %eax, %1       ; 将eax的值存储到gate_addr指向的内存
    movl %edx, %2       ; 将edx的值存储到(gate_addr+4)指向的内存
                        ; 这两条指令在内存 gate_addr处写入了2个32位数值，一共64位，8个Byte。
                        ; 这8个Byte恰好组成了中断描述符结构。
*/
#define _set_gate(gate_addr, type, dpl, addr) \
__asm__("movw %%dx, %%ax\n\t"   \
        "movw %0, %%dx\n\t"     \
        "movl %%eax, %1\n\t"    \
        "movl %%edx, %2"        \
        :                       \
        :"i"((short)(0x8000 + (dpl << 13) + (type << 8))),  \
        "o"(*((char*)(gate_addr))),                         \
        "o"(*(4 + (char*)(gate_addr))),                     \
        "d"((char*)(addr)), "a" (0x00080000))

/*
    内核态特权级是0，用户态特权级是3。Linux只用到了两个特权级，中间的1和2没有用上。
    内核就该这么设计，过于精细的授权应该由程序自己实现，而不是什么都交给内核。
*/
#define set_trap_gate(n, addr) \
    _set_gate(&idt[n], 15, 0, addr) //中断向量从idt中获得，类型是trap，特权级是0，地址是外界提供的addr

#define set_intr_gate(n, addr) \
    _set_gate(&idt[n], 14, 0, addr) //中断向量从idt中获得，类型是intr，特权级是0，地址是外界提供的addr

#define set_system_gate(n, addr) \
    _set_gate(&idt[n], 15, 3, addr) //中断向量从idt中获得，类型是system，特权级是3，地址是外界提供的addr

/*
    这个宏把tss和ldt的地址放进描述符中，
    n代表描述符在GDT中的地址
    addr是tss或ldt的真实物理地址
    type是描述符类型，根据代码，tss的描述符类型是0x89，ldt的描述符类型是0x82
    我不太想去解析这段代码，这很消磨热情。
    还好书上写了不少内容，这里就照抄罢。

    __asm__("movw $104, %1\n\t"             \   //TSS和LDT的长度是104。起始tss是104，ldt是24,这里选择了最大的值。
        "movw %%ax, %2\n\t"             \   //把地址送入描述符的不同位置中
        "rorl $16, %%eax\n\t"           \   //
        "movb %%al, %3\n\t"             \   //
        "movb $" type ", %4\n\t"        \   //指定了描述符类型
*/
#define _set_tssldt_desc(n, addr, type) \
__asm__("movw $104, %1\n\t"             \
        "movw %%ax, %2\n\t"             \
        "rorl $16, %%eax\n\t"           \
        "movb %%al, %3\n\t"             \
        "movb $" type ", %4\n\t"        \
        "movb $0x00, %5\n\t"            \
        "movb %%ah, %6\n\t"             \
        "rorl $16, %%eax\n\t"           \
        ::"a"(addr), "m"(*(n)), "m"(*(n+2)), "m"(*(n+4)), \
        "m"(*(n+5)), "m"(*(n+6)), "m"(*(n+7)) \
        )

#define set_tss_desc(n, addr) _set_tssldt_desc(((char*)(n)), addr, "0x89")
#define set_ldt_desc(n, addr) _set_tssldt_desc(((char*)(n)), addr, "0x82")
    
#endif

