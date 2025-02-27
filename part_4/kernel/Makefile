GCC		:=	gcc

# 还挺麻烦，不加这个-fno-pie 会导致出错：undefined reference to `_GLOBAL_OFFSET_TABLE_'
# 所谓的“位置无关代码”，实际上是靠一个全局偏移表来实现的。
# 确实，只要有偏移，代码变量数据都可以靠偏移来定位。
CCFLAG 	:=	-m32 -I../include -nostdinc -Wall -fomit-frame-pointer -c -fno-pie

LDFLAG	:=	-Ttext 0x0 -s --oformat binary -m elf_i386
INCDIR	:=	../include
OBJS	:=	head.o main.o sched.o

system.bin: $(OBJS)
	$(LD)	$(LDFLAG) -e startup_32 -o $@ $^

head.o: head.S
	$(GCC)	-m32 -traditional -c -o $@ $^

main.o: main.c
	$(GCC)	$(CCFLAG) -o $@ $^

sched.o : sched.c
	$(GCC) $(CCFLAG) -o $@ $<

clean:
	rm *.o
	rm system.bin