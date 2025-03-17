.code32
.globl divide_error, debug, nmi, int3, overflow, bounds, invalid_op
.globl double_fault, coprocessor_segment_overrun
.globl invalid_TSS, segment_not_present, stack_segment
.globl general_protection, coprocessor_error, reserved
.globl alignment_check

#   这些就是中断处理代码的入口。看着复杂，其实弄懂原理之后就感觉很简单了。

#   中断分为两类：有错误码的，没有错误码的。
#   对于这两种类型的中断，处理手法都相同：先把响应函数的地址push到栈上，然后跳入“no_error_code”或者“error_code”两个不同的流程。两个流程很接近，都涉及到“上下文保存”的操作。

#   另外，在进入中断时，CPU 已自动压入 SS:ESP, EFLAGS, CS:EIP（硬件上下文）。

divide_error:
    pushl $do_divide_error
no_error_code:
                        # 先保存所有寄存器的上下文
    xchgl %eax, (%esp)  # 把栈顶的内容和eax的内容交换，这一步完成了2个操作，eax的内容被保存到了栈顶，而栈顶内容（响应函数的地址）来到了eax寄存器内。
    pushl %ebx
    pushl %ecx
    pushl %edx
    pushl %edi
    pushl %esi
    pushl %ebp
    push  %ds
    push  %es
    push  %fs

    pushl $0            # 对于没有错误码的情况，这里也压入一个值，确保有无错误码的情况下栈的结构都相同。我们选择压入立即数0。
                        # 到目前为止，一共手动把 36个Byte 的数据存入了栈中。此时栈上还有CPU自动压入的SS:ESP EFLAGS CS:EIP（硬件上下文），所以栈上数据是比44Byte 更大的。

    leal  44(%esp), %edx    # 计算%esp+44的结果。esp是栈顶，而栈在x86上向低地址增长。所以实际上，%esp+44实际上指向更低的地址。把结果存入edx中。这个结果是“发生异常时的栈顶指针”
    pushl %edx              # 把edx的值也压入栈中

    movl  $0x10, %edx   # 把内核数据段压入dx ds es fs寄存器
    movw  %dx, %ds
    movw  %dx, %es
    movw  %dx, %fs
    call  *%eax         # 执行响应函数

                        # 恢复上下文
    addl  $8, %esp      # 在调用响应函数之前，执行了2次pushl( pushl $0 和 pushl %edx )，压入了8Byte的数据。为了移除这两个数据，我们选择使用addl指令恢复栈顶。
                        # 栈是向低地址增长的。栈顶+8相当于“栈顶”后退8个Byte，也就起到了清除栈顶数据的效果。
                        # 这个写法和 leal  44(%esp), %edx  的写法是有区别的，不能混为一谈。
    pop   %fs           
    pop   %es
    pop   %ds
    popl  %ebp
    popl  %esi
    popl  %edi
    popl  %edx
    popl  %ecx
    popl  %ebx
    popl  %eax
    iret

debug:
    pushl   $do_int3
    jmp     no_error_code

nmi:
    pushl   $do_nmi
    jmp     no_error_code

int3:
    pushl   $do_int3
    jmp     no_error_code

overflow:
    pushl   $do_overflow
    jmp     no_error_code

bounds:
    pushl   $do_bounds
    jmp     no_error_code

invalid_op:
    pushl   $do_invalid_op
    jmp     no_error_code

coprocessor_segment_overrun:
    pushl   $do_coprocessor_segment_overrun
    jmp     no_error_code

reserved:
    pushl   $do_reserved
    jmp     no_error_code


double_fault:
    pushl $do_double_fault
error_code:
                            # 带错误码的汇编和不带错误码的汇编极其接近。
    xchgl %eax, 4(%esp)     # 记录错误码到eax，同时把eax的值保存到栈内
    xchgl %ebx, (%esp)      # 记录响应函数的地址到ebx，同时把ebx的值保存到栈内
    pushl %ecx
    pushl %edx
    pushl %edi
    pushl %esi
    pushl %ebp
    push  %ds
    push  %es
    push  %fs
    pushl %eax
    leal  44(%esp), %eax
    pushl %eax              # 这条语句用于压入错误码
    movl  $0x10, %eax
    movw  %ax, %ds
    movw  %ax, %es
    movw  %ax, %fs
    call  *%ebx             # 调用响应函数
    addl  $8, %esp
    pop   %fs
    pop   %es
    pop   %ds
    popl  %ebp
    popl  %esi
    popl  %edi
    popl  %edx
    popl  %ecx
    popl  %ebx
    popl  %eax
    iret

invalid_TSS:
    pushl $do_invalid_TSS
    jmp   error_code

segment_not_present:
    pushl $do_segment_not_present
    jmp   error_code

stack_segment:
    pushl $do_stack_segment
    jmp   error_code

general_protection:
    pushl $do_general_protection
    jmp   error_code

alignment_check:
    pushl $do_alignment_check
    jmp   error_code

