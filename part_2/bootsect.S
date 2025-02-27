SETUPLEN    =   4                   # 暂时用不上

BOOTSEG     =   0x7c0

INITSEG     =   0x9000

SETUPSEG    =   0x9020

SYSSEG      =   0x1000              # 暂时用不上

ENDSEG      =   SYSSEG + SYSSIZE    # 暂时用不上

ROOT_DEV    =   0X000               # 暂时用不上

.code16                             

.text                               

.global _start                      

_start:                             
    jmpl    $BOOTSEG,   $start2     

start2:                                
    movw    $BOOTSEG,   %ax             
    movw    %ax,        %ds            
    movw    $INITSEG,   %ax             
    movw    %ax,        %es             

    movw    $256,       %cx             # 设置cx为256,这在为循环做准备
    subw    %si,        %si             # 清空si,即令 si = 0
    subw    %di,        %di             # 清空di,即令 di = 0
    
    rep                                 # rep指令是重复（repeat）指令。只要cx寄存器的值 > 0，后面紧邻的一条语句就会被重复执行。我们之前设置cs为256,所以后面一条语句会被重复执行256次。
    movsw                               # movsw是一条移动指令，可以把1个word（2个Byte）从 ds:si 处移到 es:di 处。且移动后寄存器会自行变化，si与di都会增加2。
                                        # 因此，在设计好寄存器后，只需rep + movsw,即可复制512Byte的内容到目标地址。

    jmpl   $INITSEG,   $go              # 整个扇区都已经复制完了，接下来跳转到目标地址，并且从 go段基 的位置开始继续执行代码。
                                        # 旧的扇区以后会被覆盖，因此接下来我们都在新扇区内工作。

go:                                     # 由于jmpl指令，此时的cs寄存器的值是0x9000，但其它段寄存器还指向旧的扇区，所以此处要进行一次更新。
    movw    %cs,        %ax                          
    movw    %ax,        %dx             # 分别设置dx es ss寄存器 为 0x9000
    movw    %ax,        %es
    movw    %ax,        %ss
    movw    $0xFF00,    %sp             # 顺便设置栈指针寄存器 = 0xFF00,这一步目前没什么意义，但是让栈指针寄存器指向一个合理的位置，是一个好习惯。

load_setup:                             # 执行完 go段 之后，紧接着就是 load_setup段。
    movw    $0X0000,    %dx             # dx寄存器的高位dh = 0 意味着 磁头号 = 0 ,低位dl = 0 意味着从软盘读取,如果dl = 7,则意味着从硬盘读取。
    movw    $0x0002,    %cx             # cx寄存器的高位ch = 0 代表柱面号,低位cl = 2，cl的0-5位代表扇号，6-7位代表磁道号的高2位。
    movw    $0x0200,    %bx             # es:bx指向缓冲区位置，在之前的代码中，es已经被设置为0x9000，此时bx设置为0x0200，那么缓冲区此时指向0x9200
    movb    $SETUPLEN,  %al             # al是要读取的扇区数量，此处是4。
    movb    $0x02,      %ah             # ah是功能号，ah=0x02指的是从磁盘扇区读到内存。

    int     $0x13                       # 调用13号中断，进行加载。如果加载成功，则不改变CF标记。如果加载失败，CF标记会被设置为1。
                                        # 加载成功之后，从0x9200开始，存储着4个扇区的数据，占用了4个512Byte，也就是4个0x200。因此数据存储的范围是：0x9200到 0x9200+ 0x800 - 1。
    
    jnc     ok_load_setup               # 在此处判断CF标记，如果为0，说明加载成功，跳转到 ok_load_setup段，以打印一条成功信息，并把接力棒交给setup段。（setup段的代码并不在这个文件内）
                                        # 如果因为各种原因读取失败，就会继续执行接下来的代码
    
    movw    $0x0000,    %dx             # 设置 dx寄存器 = 0             
    movw    $0x0000,    %ax             # 设置 ax寄存器 = 0
    int     $0x13                       # 调用13号中断。因为ax寄存器被设置为0，此时的13号中断有重置磁盘的效果。个人猜测是重置磁头位置？
    jmp     load_setup                  # 跳转回本段的开头，重新加载对应扇区。

ok_load_setup:

    movw    $msg,    %ax            # 获取字符串地址
    movw    %ax,    %bp             
    movw    $0x01301,   %ax         

    movw    $0x0c,  %bx             # 设置寄存器bx=0x0c，使得文字为红色
    movw    $38,    %cx             # 设置寄存器cx=0x12,使得字符串长度=38
    movb    $9, %dl                 
    movb    $9, %dh                 
    int $0x010                      # 调用10号中断，通知显卡刷新内容

    jmpl    $SETUPSEG,  $0          # 把接力棒交给setup段。（setup段的代码并不在这个文件内）

msg:                                
.ascii  "Setup has been loaded"

.org    508                        

root_dev:
    .word   ROOT_DEV

boot_flag:                          
    .word  0xaa55

                                    





