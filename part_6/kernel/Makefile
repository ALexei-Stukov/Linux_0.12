GCC		:=	gcc

# 还挺麻烦，不加这个-fno-pie 会导致出错：undefined reference to `_GLOBAL_OFFSET_TABLE_'
# 所谓的“位置无关代码”，实际上是靠一个全局偏移表来实现的。
# 确实，只要有偏移，代码变量数据都可以靠偏移来定位。

# 不加 -fno-stack-protector 也会出问题。我们在写内核代码，在部分情况下确实会做出对“堆栈保护”不利的事情。但这是必须的。
# 所以必须去除掉“堆栈保护”选项，这样我们能做更复杂的操作。
CCFLAG 	:=	-m32 -I../include -nostdinc -Wall -fomit-frame-pointer -c -fno-pie -fno-stack-protector

LDFLAG	:=	-Ttext 0x0 -s --oformat binary -m elf_i386
INCDIR	:=	../include
OBJS   := head.o main.o sched.o printk.o sys_call.o vsprintf.o traps.o asm.o chr_drv/chr_drv.a mm/mm.o ../lib/lib.a 

system.bin: $(OBJS)
	$(LD)	$(LDFLAG) -e startup_32 -o $@ $^	-M > System.map

sys_call.o : sys_call.S
	$(GCC) -m32 -traditional -c -o $@ $<

asm.o : asm.S
	$(GCC) -m32 -traditional -c -o $@ $<
	
traps.o : traps.c
	$(GCC) $(CCFLAG) -o $@ $<

head.o: head.S
	$(GCC)	-m32 -traditional -c -o $@ $^

main.o: main.c
	$(GCC)	$(CCFLAG) -o $@ $^

sched.o : sched.c
	$(GCC) $(CCFLAG) -o $@ $<

# 这部分代码和字符设备、打印字符串有关。
printk.o : printk.c
	$(GCC) $(CCFLAG) -o $@ $<

vsprintf.o : vsprintf.c
	$(GCC) $(CCFLAG) -o $@ $<

mm/mm.o : mm/*.c
	cd mm; make mm.o; cd ..

chr_drv/chr_drv.a: chr_drv/*.c
	cd chr_drv; make chr_drv.a; cd ..

../lib/lib.a : ../lib/*.c
	cd ../lib; make lib.a; cd ../kernel

clean :
	rm -f *.o
	rm -f system.bin
	rm -f System.map
	cd chr_drv; make clean; cd ..
	cd ../lib; make clean; cd ../kernel
	cd mm; make clean; cd ..