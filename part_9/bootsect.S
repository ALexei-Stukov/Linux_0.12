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
  movb $SETUPLEN, %al
  movb $0x02,   %ah
  int  $0x13
  jnc  ok_load_setup
  movw $0x0000, %dx
  movw $0x0000, %ax
  int  $0x13
  jmp  load_setup

ok_load_setup:
	movb $0x00, %dl
	movw $0x0800, %ax
	int  $0x13
	movb $0x00, %ch
	movw %cx, %cs:sectors
	movw $INITSEG, %ax
	movw %ax, %es

	movw $SYSSEG, %ax
	movw %ax, %es
	call read_it
	call kill_motor

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

	movw $msg, %ax
	movw %ax, %bp
	movw $0x01301, %ax
	movw $0x0c, %bx
	movw $23, %cx
	movw $0, %dx
	int  $0x010

	jmpl $SETUPSEG, $0

sread:
	.word 1+SETUPLEN
head:
	.word 0
track:
	.word 0
read_it:
	movw %es, %ax
	testw $0x0fff, %ax
die:
	jne die
	xor %bx, %bx
rp_read:
	movw %es, %ax
	cmpw $ENDSEG, %ax
	jb   ok1_read
	ret
ok1_read:
	movw %cs:sectors, %ax
	subw sread, %ax
	movw %ax, %cx
	shlw $9,  %cx
	addw %bx, %cx
	jnc  ok2_read
	je   ok2_read
	xorw %ax, %ax
	subw %bx, %ax
	shrw $9,  %ax
ok2_read:
	call read_track
	movw %ax, %cx
	addw sread, %ax
	cmpw %cs:sectors, %ax
	jne  ok3_read
	movw $1, %ax
	subw head, %ax
	jne  ok4_read
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
	addw $0x1000, %ax
	movw %ax, %es
	xorw %bx, %bx
	jmp  rp_read

read_track:
	pushw %ax
	pushw %bx
	pushw %cx
	pushw %dx
	movw  track, %dx
	movw  sread, %cx
	incw  %cx
	movb  %dl, %ch
	movw  head, %dx
	movb  %dl, %dh
	movb  $0, %dl
	andw  $0x0100, %dx
	movb  $2, %ah
	int   $0x13
	jc    bad_rt
	popw  %dx
	popw  %cx
	popw  %bx
	popw  %ax
	ret
bad_rt:
	movw $0x0, %ax
	movw $0x0, %dx
	int  $0x13
	popw %dx
	popw %cx
	popw %bx
	popw %ax
	jmp read_track

kill_motor:
	pushw %dx
	movw  $0x3f2, %dx
	movb  $0,  %al
	outb  %al, %dx
	popw  %dx
	ret

sectors:
	.word 0

msg:
.byte 13, 10
.ascii "Linux is booting..."
.byte 13, 10
.org 508
root_dev:
	.word ROOT_DEV
boot_flag:
	.word 0xaa55

