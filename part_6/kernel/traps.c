#include <asm/system.h>
#include <linux/sched.h>
#include <asm/io.h>
/*
    这些是中断门，目前只实现了0到17号中断，18到47都是预留中断reserved。
    按照分类来讲，0到47就是全部X86架构预留的硬件中断了。

        0-31:    CPU 异常和故障（处理器预定义），15和20-31保留。这部分大多是不可屏蔽中断。
        32-47:   硬件 IRQ 中断（可屏蔽中断）
        48-255:  可用于软件中断和用户自定义。其中Linux常用80中断作为系统调用。
*/

/*
    处理硬件中断时总是要处理寄存器，所以部分代码还是要用汇编来解决。
    比如divide_error()，它由汇编代码写成，位于kernel/asm.S中
    根据中断门的设置，发生除0之后，响应的函数是divide_error()。而divide_error()会调用do_divide_error()打印错误信息。
*/
void divide_error();
void debug();
void nmi();
void int3();
void overflow();
void bounds();
void invalid_op();
void double_fault();
void coprocessor_segment_overrun();
void invalid_TSS();
void segment_not_present();
void stack_segment();
void general_protection();
void reserved();
void irq13();
void alignment_check();

static void die(char* str, long esp_ptr, long nr) {
    long* esp = (long*)esp_ptr;

    printk("%s: %04x\n\r", str, 0xffff & nr);

    while (1) {
    }
}

void do_double_fault(long esp, long error_code) {
    die("double fault", esp, error_code);
}

void do_general_protection(long esp, long error_code) {
    die("general protection", esp, error_code);
}

void do_alignment_check(long esp, long error_code) {
    die("alignment check", esp, error_code);
}

void do_divide_error(long esp, long error_code) {
    die("divide error", esp, error_code);
}

void do_int3(long * esp, long error_code,
        long fs,long es,long ds,
        long ebp,long esi,long edi,
        long edx,long ecx,long ebx,long eax) {
    int tr;

    __asm__("str %%ax":"=a" (tr):"" (0));
    printk("eax\t\tebx\t\tecx\t\tedx\n\r%8x\t%8x\t%8x\t%8x\n\r",
            eax,ebx,ecx,edx);
    printk("esi\t\tedi\t\tebp\t\tesp\n\r%8x\t%8x\t%8x\t%8x\n\r",
            esi,edi,ebp,(long) esp);
    printk("\n\rds\tes\tfs\ttr\n\r%4x\t%4x\t%4x\t%4x\n\r",
            ds,es,fs,tr);
    printk("EIP: %8x   CS: %4x  EFLAGS: %8x\n\r",esp[0],esp[1],esp[2]);
}

void do_nmi(long esp, long error_code) {
    die("nmi", esp, error_code);
}

void do_debug(long esp, long error_code) {
    die("debug", esp, error_code);
}

void do_overflow(long esp, long error_code) {
    die("overflow", esp, error_code);
}

void do_bounds(long esp, long error_code) {
    die("bounds", esp, error_code);
}

void do_invalid_op(long esp, long error_code) {
    die("invalid_op", esp, error_code);
}

void do_device_not_available(long esp, long error_code) {
    die("device not available", esp, error_code);
}

void do_coprocessor_segment_overrun(long esp, long error_code) {
    die("coprocessor segment overrun", esp, error_code);
}

void do_segment_not_present(long esp, long error_code) {
    die("segment not present", esp, error_code);
}

void do_invalid_TSS(long esp, long error_code) {
    die("invalid tss", esp, error_code);
}

void do_stack_segment(long esp, long error_code) {
    die("stack segment", esp, error_code);
}

void do_reserved(long esp, long error_code) {
    die("reserved (15,17-47) error",esp,error_code);
}

/*
    设置各个函数对应的trap gate
*/
void trap_init() {
    int i;

    set_trap_gate(0, &divide_error);
    set_trap_gate(1,&debug);
    set_trap_gate(2,&nmi);
    set_system_gate(3,&int3);
    set_system_gate(4,&overflow);
    set_system_gate(5,&bounds);
    set_trap_gate(6,&invalid_op);
    set_trap_gate(8,&double_fault);
    set_trap_gate(9,&coprocessor_segment_overrun);
    set_trap_gate(10, &invalid_TSS);
    set_trap_gate(11, &segment_not_present);
    set_trap_gate(12, &stack_segment);
    set_trap_gate(13, &general_protection);
    set_trap_gate(15,&reserved);
    set_trap_gate(17,&alignment_check);

    for (i=18;i<48;i++)
        set_trap_gate(i,&reserved);
    
    /*
        这两行代码主要是在操作8259A可编程中断控制器(PIC)的中断屏蔽寄存器(IMR)

        0x21 是主8259A的数据端口
        0xfb 是 11111011 二进制，与操作 会清除 IRQ2 的屏蔽位
        目的：启用 IRQ2，这是级联从8259A的通道

        0xdf 是 11011111 二进制，与操作会清除 IRQ5 的屏蔽位
        目的：启用键盘中断（在从片上）

        IRQ的不同位置都对应着不同的中断
        主8259A (0x20-0x21)     从8259A (0xA0-0xA1)
        IRQ0 - 时钟            IRQ8  - 实时时钟
        IRQ1 - 键盘            IRQ9  - 重定向的IRQ2
        IRQ2 - 级联从片        IRQ10 - 保留
        IRQ3 - 串口2           IRQ11 - 保留
        IRQ4 - 串口1           IRQ12 - PS/2鼠标
        IRQ5 - 并口2           IRQ13 - 协处理器
        IRQ6 - 软盘            IRQ14 - 主IDE
        IRQ7 - 并口1           IRQ15 - 从IDE
    */
    outb_p(inb_p(0x21)&0xfb,0x21);
    outb(inb_p(0xA1)&0xdf,0xA1);
}

