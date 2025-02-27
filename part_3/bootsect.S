SYSSIZE = 0x3000 

SETUPLEN  = 4

BOOTSEG   = 0x7c0

INITSEG   = 0x9000

SETUPSEG  = 0x9020

SYSSEG    = 0x1000

ENDSEG    = SYSSEG + SYSSIZE

ROOT_DEV  = 0x000

.code16
.text

.global _start
_start:
  
  jmpl $BOOTSEG, $start2
  
start2:
  movw $BOOTSEG, %ax
  movw %ax, %ds
  movw $INITSEG, %ax
  movw %ax, %es
  movw $256, %cx
  subw %si, %si
  subw %di, %di

  rep
  movsw
  
  jmpl $INITSEG, $go
  
go:
	movw %cs, %ax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %ss
	movw $0xFF00, %sp

load_setup:
	movw $0x0000, %dx
	movw $0x0002, %cx
	movw $0x0200, %bx
	movb $0x02,   %ah
	movb $SETUPLEN, %al
	int  $0x13
	jnc  ok_load_setup
	movw $0x0000, %dx
	movw $0x0000, %ax
	int  $0x13
	jmp  load_setup

ok_load_setup:

	movb $0x00, %dl					# 调用13号中断读取驱动器参数，dl=0，说明读的是软盘
	movw $0x0800, %ax
	int  $0x13
									# 如果读取成功，那么
									# CH＝柱面数的低8位
									# CL的位7-6＝柱面数的高2位
									# CL的位5-0＝扇区数
									# DH＝磁头数
									# DL＝驱动器数
									# ES:DI＝磁盘驱动器参数表地址

	movb $0x00, %ch					# 清空cx的高位Byte，因为我们用不上
	movw %cx, %cs:sectors			# 一个前置工作，把扇区数放入 %cs:0 这个位置。后续只需读这个位置，就可以获得磁道上的扇区数。
	movw $INITSEG, %ax
	movw %ax, %es

	movw $SYSSEG, %ax				# bootloader运作期间，因为Bios需要占用0x10000以下的大部分地址，所以bootloader只能把系统内核搬运到0x10000开始的位置。
	movw %ax, %es					# 这里把es段寄存器设置为0x1000，已经在为复制做准备了。
									# 后续由setup 打开保护模式，那么Bios就可以离场了，此时0x10000以下的内存可以复用。setup会把系统内核从0x10000搬运到物理内存从0开始的位置。当然，这都是后话了。
	
	call read_it					# 这个函数的作用是把内核移动到从0x10000开始的位置。

	call kill_motor					# 调用kill_motor函数，关闭软盘驱动器，不要让软盘一直运作。
									
									
	movw %cs:root_dev, %ax			
	cmpw $0, %ax					
	jne  root_defined				
	movw %cs:sectors, %bx			
	movw $0x0208, %ax				
	cmpw $15, %bx					
	je   root_defined				
	movw $0x021c, %ax				
	cmpw $18, %bx					
	je   root_defined				

									
undef_root:
	jmp  undef_root					

root_defined:
	mov  %ax, %cs:root_dev

	movw %cs, %ax
	movw %ax, %ds
	movw %ax, %es
	
	movw $0x600, %ax
	movw $0x700, %bx
	movw $0x0,   %cx
	movw $0x184f,%dx
	int  $0x10
	
	movw $msg, %ax					# 打印一段红色文字："Linux is booting..."
	movw %ax, %bp
	movw $0x01301, %ax
	movw $0x0c, %bx
	movw $23, %cx
	movw $0, %dx
	int  $0x010

	jmpl $SETUPSEG, $0				# 进入setup段

sread:							# 已读取的扇区数。在加载system之前，已经读取了引导扇区+setup，一共1+4个扇区
	.word 1+SETUPLEN
head:							# 当前磁头号
	.word 0
track:							# 当前磁道号
	.word 0
								
								
read_it:						# 测试输入的段值，必须位于内存地址64KB边界处，否则进入死循环。也就是说，段的起点必须是 0x100000,作为入参的es段寄存器必须是0x1000的倍数。
	movw %es, %ax								
	testw $0x0fff, %ax

die:
	jne die						# 如果没有进入死循环，则清除bx寄存器，用于表示当前段内存放数据的开始位置。						
	xor %bx, %bx				

rp_read:
	movw %es, %ax				# 如果es段寄存器的值没有超过预设的范围，就认为复制还没完成，需要继续复制。
	cmpw $ENDSEG, %ax			
	jb   ok1_read				
	ret


	# 接下来就比较复杂了。
	# 首先，内核大小可能比较高，导致需要在读取的时候切换磁头和扇区，或者对es段寄存器进行自增。
	# 其次，对某个扇区的读取可能会失败，而且再次读取或许就能成功。总之，不可能因为1次偶然的失败就从头开始读取内核。
	# 最后，因为某些原因，Linus希望一次读取不超过64KB。
	# 为了满足上述的要求，Linus最终写了如下的代码。

ok1_read: 
	# 在一次读取不超过64KB的情况下，我们这次读取最多能读多少个扇区？
	# 这个流程是必要的，因为读取可能会失败。

	movw %cs:sectors, %ax		# 取总扇区数放入ax寄存器（由前置工作完成）
	subw sread, %ax				# 减去当前已经读取的扇区数。sread这个值会持续增加
	movw %ax, %cx				# cx = ax = 当前磁道未读扇区数

	shlw $9,  %cx				# cx = cx*512 + bx（段内当前偏移值） = 此次读操作后，段内共读入的字节数。shlw是左移指令。
	addw %bx, %cx

	jnc  ok2_read				# 如果没有超过64KB字节，就跳入ok2_read
	je   ok2_read

	xorw %ax, %ax				# 如果会超过64KB，那么就计算本次最多能读取多少字节。这个数字存储在ax寄存器内
	subw %bx, %ax
	shrw $9,  %ax
ok2_read:
	call read_track				# 进行一次读取。
	movw %ax, %cx				
	addw sread, %ax
	cmpw %cs:sectors, %ax
	jne  ok3_read
	movw $1, %ax				
	subw head, %ax

	jne  ok4_read				# 如果发现当前磁道的数据已经读完，就让磁道号+1
	incw track					
ok4_read:
	movw %ax, head
	xorw %ax, %ax
ok3_read:
	movw %ax, sread
	shlw $9,  %cx
	addw %cx, %bx
	jnc  rp_read
	movw %es, %ax

	addw $0x1000, %ax			# 当前段已经满了，所以要令es段增加0x1000，且需要重置bx寄存器。
	movw %ax, %es
	xorw %bx, %bx
	
	jmp  rp_read

read_track:					# 到这里，才开始正式进行磁道扇区读操作
	pushw %ax
	pushw %bx
	pushw %cx
	pushw %dx
	movw  track, %dx		# 磁头号+驱动器号由dx存储
	movw  sread, %cx		# 柱面号+扇区号由cx存储
	incw  %cx				# 令cx自增1
	movb  %dl, %ch			
	movw  head, %dx			
	movb  %dl, %dh			
	movb  $0, %dl
	andw  $0x0100, %dx
	movb  $2, %ah			# 功能号
	int   $0x13				# 调用1次磁盘读取,读取数据到es:bx的位置，al是传输的扇区数
	jc    bad_rt			# 检查是否读取出错
	popw  %dx
	popw  %cx
	popw  %bx
	popw  %ax
	ret
bad_rt:							# 执行启动器复位操作，再跳转到read_track处重试
	movw $0x0, %ax
	movw $0x0, %dx
	int  $0x13
	popw %dx
	popw %cx
	popw %bx
	popw %ax
	jmp read_track

kill_motor:						# 这个子历程用于关闭软驱的马达

	pushw %dx					# 入栈

	movw  $0x3f2, %dx			# 把 0x3f2 写入端口0 			
	movb  $0,  %al				
	outb  %al, %dx				
								
	popw  %dx					# 出栈
	ret							

sectors:
	.word 0						

msg:
.byte 13, 10					# 回车、换行的ASCII 码。
.ascii "Linux is booting..."
.byte 13, 10 					# 共24个ASCII字符
.org 508
root_dev:
	.word ROOT_DEV				# 这里存放根文件系统所在的设备号。后续会使用到。
boot_flag:
	.word 0xaa55
  
