.code32
.globl divide_error, debug, nmi, int3, overflow, bounds, invalid_op
.globl double_fault, coprocessor_segment_overrun
.globl invalid_TSS, segment_not_present, stack_segment
.globl general_protection, coprocessor_error, reserved
.globl alignment_check

divide_error:
    pushl $do_divide_error
no_error_code:
    xchgl %eax, (%esp)
    pushl %ebx
    pushl %ecx
    pushl %edx
    pushl %edi
    pushl %esi
    pushl %ebp
    push  %ds
    push  %es
    push  %fs
    pushl $0
    leal  44(%esp), %edx
    pushl %edx
    movl  $0x10, %edx
    movw  %dx, %ds
    movw  %dx, %es
    movw  %dx, %fs
    call  *%eax
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
    xchgl %eax, 4(%esp)
    xchgl %ebx, (%esp)
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
    pushl %eax
    movl  $0x10, %eax
    movw  %ax, %ds
    movw  %ax, %es
    movw  %ax, %fs
    call  *%ebx
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

