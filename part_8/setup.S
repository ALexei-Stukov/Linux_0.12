INITSEG = 0x9000                    
SYSSEG  = 0x1000
SETUPSEG = 0x9020                   

.code16                             

.text                               

.globl _start_setup

_start_setup:
    movw    %cs,    %ax
    movw    %ax,    %dx
    movw    %ax,    %es

    movw    $setup_msg,    %ax            
    movw    %ax,    %bp             
    movw    $0x01301,   %ax         

    movw    $0x0c,  %bx             
    movw    $16,    %cx             
    movb    $9,     %dl                 
    movb    $12,    %dh                 
    int $0x010                      
                                   
    
    movw    $INITSEG,  %ax
    movw    %ax,    %ds             
    movb    $0x03,  %ah             # 设置ah = 0x03
    xor     %bh,    %bh             # 使用xor清空bh，即将其设置为0
    int     $0x10                   # 调用10号中断,获取光标信息。由于ah=3,10号中断将返回光标的位置，且存放在dx寄存器内。
    movw    %dx,    (0)             # 把dx的值写入内存，写入的位置是 段基寄存器 * 2^4 + 0,也就是0x90000。由于dx是一个word，所以需要消耗2个Byte来保存数据。
                                    # 我们实际上直接用数据覆盖了bootloader，听起来很危险，但这是可行的。因为当引导程序执行到setup时，bootloader已经无需执行了。所以我们可以直接复用这部分内存。

    movb    $0x88,  %ah             # 在ah内写入0x88
    int     $0x15                   # 调用15号中断，获得扩展内存的大小。
    movw    %ax,    (2)             # 同样把结果写入0x90000附近，因为前两个Byte已经被占用，这里需要稍微修改一下偏移量。
                                    # 这里值得花篇幅说一下。
                                    # 因为翻译的问题，中文的“扩展内存”可能意味着2个意思，在很多场合其实没办法靠中文区分。
                                    # 这两个意思分别是：extended memory 和 expanded memory。
                                    # expanded memory是一个早期的技术标准，简单来说就是通过插入新硬件，使得内存容量上升。目前似乎已经被弃用了。
                                    # extended memory是和保护模式有关。
                                    # CPU刚启动时运行在实模式下，寻址能力不超过20位。即使内存巨大，CPU也只能使用到1MB的内存。
                                    # 为了使用更大的内存，我们需要打开保护模式。在打开保护模式之前，我们需要先读取真实的内存大小，看看到底有多少内存可用。
                                    # 这里多提一句，20根地址线的编号是 0 - 19。编号为20的地址线实际上是第21根。

                                    # 后续的很多代码，都是在读取硬件信息。我认为可以先不管细节，先把重点标记一下就行。反正以后写os的时候也不会再用到这个setup。
                                    # 因此，除非特别重要，否则后续的代码我只简单介绍作用。
    movb $0x0f, %ah
    int  $0x10                      # 获取显示模式信息
    movw %bx, (4)                   
    movw %ax, (6)                   
    
    movb $0x12, %ah
    movb $0x10, %bl
    int  $0x10                      # 获取显卡的相关信息
    movw %ax, (8)                   
    movw %bx, (10)                  
    movw %cx, (12)                  
    movw $0x5019, (14)

    movw $0x0000, %ax               
    movw %ax, %ds                   
    ldsw (4 * 0x41), %si            # 把特殊地址 4 * 0x41 存入 si 寄存器。ldsw指令和movw指令的效果是相同的，只不过这条指令更加明确地指出这个值是一个地址。
                                    # 4 * 0x41 是一个比较特殊的位置。这个位置是被bios所管理的，在整个启动过程中，这个位置首先属于中断向量，后来被用于存储硬盘的相关信息。
                                    # 第一块硬盘（disk0）的位置是 4 * 0x41，如果有第二个硬盘（被称为disk1），它的位置会被放在 4 * 0x46的位置。
    
    movw $INITSEG, %ax              
    movw %ax, %es                   
    movw $0x0080, %di               
    movw $0x10, %cx                 
    rep
    movsb                           # 复制0号硬盘的参数
                                    
                                    

    /* get hd1 data */              # 同理，用相同的手法复制1号硬盘的参数。    
    movw $0x0000, %ax
    movw %ax, %ds
    ldsw (4 * 0x46), %si
    movw $INITSEG, %ax
    movw %ax, %es
    movw $0x0090, %di
    movw $0x10, %cx
    rep
    movsb
                                    # 由于一台机器可能有多个硬盘，所以此处要通过中断来判断。
    movw $0x1500, %ax
    movb $0x81, %dl
    int  $0x13                      # 这是0x13号中断，和磁盘有关。
    jc   no_disk1                   # 如果没有第二块磁盘，就执行 no_disk1
    cmpb $3, %ah
    je   is_disk1                   # 如果存在第二块硬盘，就可以直接进入 is_disk1,也就是正式进入保护模式的流程。
                                    # je指令的运作需要依赖于 CF进位。所以此时，进位寄存器的值是1。

no_disk1:                           # no_disk1实际上只是在清空disk1这块区域的磁盘信息而已。
    movw $INITSEG, %ax
    movw %ax, %es
    movw $0x0090, %di
    movw $0x10, %cx
    movw $0x00, %ax
    rep
    stosb
is_disk1:
    /* prepare for protection mode */
    cli                             # 关闭中断，避免接下来的关键代码被中断给影响。
    
    movw $0x0000, %ax               # 设置ax寄存器 = 0x0000
    cld                             # 清除CF进位。这样一来，其它寄存器才能按需运作。（比如 di si 这两个地址指针）
                                    # 如果不这么做，之后就无法搬移操作系统内核。

do_move:                            # 开始搬移操作系统内核

    movw %ax, %es
    addw $0x1000, %ax           
    cmpw $0x9000, %ax
    jz   end_move                   # 直到复制完成，才会进入end_move中
    movw %ax, %ds
    subw %di, %di
    subw %si, %si
    movw $0x8000, %cx               # cx被设置为8000，后续的movsw指令会被执行8000次。
    rep
    movsw
    jmp  do_move                    # 回到 do_move的起点，继续复制。
                                    # 因为每次复制之前都会执行 addw $0x1000, %ax，所以这个函数实际上会复制一段很大的地址。
                                    # 函数一共执行8次，每次复制0x10000个字节。
end_move:

    movw $0xb800, %ax               # 我们希望在屏幕上打印一个字母A。由于中断已经被关闭，已经不能使用int 10的手法了，此处只能使用“直接写入显存”的方法
    movw %ax, %gs
    movb $0xf, %ah                  # 影响打印位置
    movb $0x41, %al                 # 影响打印位置
    movl $0x100, %edi
    movw %ax, %gs:(%di)             # 这些设置有点繁琐，因为打印方式不是中断，而是写入显存。

                                    
    movw $SETUPSEG, %ax             
    movw %ax, %ds    
                                    # gdtr是全局描述符表寄存器(gdt register)，它可以被存储在内存中的任何位置。它的作用是存储gdt，CPU通过查询gdtr寄存器找到GDT的起始地址。可以把gdtr理解为一个小型数据库。
    lgdt gdt_48                     # 在这一步，使用lgdt指令，把GDT存入gdtr内，使得全局描述符（GDT）可以被查询到。

    call empty_8042                 # 这部分代码是在打开A20地址线。为了完成这个任务，需要多次调用empty_8042这个函数。
    movb $0xD1, %al                 
    outb %al,  $0x64                
    call empty_8042                 
    movb $0xDF, %al                 
    outb %al,  $0x60                
    call empty_8042                 

                                    # 无聊的中断
    movb $0x11, %al
    outb %al,  $0x20

    .word 0x00eb, 0x00eb
    outb %al,  $0xA0

    .word 0x00eb, 0x00eb
    movb $0x20, %al
    outb %al, $0x21

    .word 0x00eb, 0x00eb
    movb $0x28, %al
    outb %al,   $0xA1
    
    .word 0x00eb, 0x00eb
    movb $0x04, %al
    outb %al, $0x21

    .word 0x00eb, 0x00eb
    movb $0x02, %al
    outb %al, $0xA1

    .word 0x00eb, 0x00eb
    movb $0x01, %al
    outb %al, $0x21

    .word 0x00eb, 0x00eb
    outb %al, $0xA1

    .word 0x00eb, 0x00eb
    movb $0xff, %al
    outb %al, $0x21

    .word 0x00eb, 0x00eb
    outb %al, $0xA1

                                    # 打开关于时间的中断，后续会用到。
    movb $0xfe, %al
    outb %al, $0x21

    # movb $0xff, %al               # 我们换一个方式打开时间中断，此处需要注销。
    outb %al, $0xA1


    movl %cr0, %eax                 # 进入保护模式是通过设置cr0寄存器的PE位来实现的。(设置一个Bit)
    xorb $1, %al                    # 把cr0的值传给eax，通过xorb设置PE位
    movl %eax, %cr0                 # 然后把结果移回cr0，就进入了保护模式

                                    # cs寄存器没有搞定
    .byte 0x66, 0xea                # 此处又用了一些特殊写法。0xea指的是长跳转，0x66是扩展指令（特殊写法）的标志。
    
    .long 0x0000                    # 占用4个字节，存储跳转地址偏移
    .word 0x8                       # 占用2个字节，存储段选择子。这个选择子实际上是某种形式的下标，0x8写成16位的二进制是0000000000001000，它可以被拆解为
                                    # 0000000000001 0 000,第一部分是下标，说明是第1个描述符，第二个数字0代表该描述符是全局的，要去gdt里面找，第三部分是特权级，这里是0，说明是最高特权级。
                                    # 最终，跳转地址就由 段:偏移 来决定，偏移=0x0，段从gdt里取。
                                    # 之所以选择0xea，而不是选择jmp之类的助记符，似乎是因为一些小小的特性。我认为这个特性无关紧要，目前我们只需照做即可。
                                    

empty_8042:                 # 这部分代码和A20地址线有关。
    .word 0x00eb, 0x00eb
    inb $0x64, %al
    testb $2, %al
    jnz empty_8042
    ret

gdt:                        # 这是gdt表（全局描述符表），里面存着许多描述符，一条描述符能描述1个段，占用8个Byte。
    .word 0,0,0,0           # 第1个描述符全是0,这个描述符是被禁止使用的。

                            # 描述符这玩意儿很复杂，关键位置一大堆。

    #0x08指向这个段          # 内核代码段。
    .word 0x07ff
    .word 0x0000            # base address = 0
    .word 0x9a00            # 代码段为只读，可执行
    .word 0x00c0            # 颗粒度为4096,32位模式
                            
    #0x10指向这个段          # 内核数据段。     
    .word 0x07ff
    .word 0x0000
    .word 0x9200
    .word 0x00c0

    .word 0xffff
    .word 0x8000
    .word 0x920b
    .word 0x00c0


gdt_48:                     
    .word 0x800             
    .word 512+gdt, 0x9      

    
check:
.ascii "begin to close int"

setup_msg:                                
.ascii  "Setup is running"
                                    





