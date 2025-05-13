GCC := gcc
USE_DEBUG :=
CCFLAG := -I../include -nostdinc -ffreestanding -Wall -fomit-frame-pointer -fno-pic -fno-stack-protector -c -m32 $(USE_DEBUG)
LDFLAG := -Ttext 0x0 -s --oformat binary -m elf_i386
INCDIR := ../include
OBJS   := head.o main.o sched.o fork.o printk.o sys_call.o vsprintf.o traps.o asm.o panic.o \
	../fs/fs.o chr_drv/chr_drv.a blk_drv/blk_drv.a ../mm/mm.o ../lib/lib.a 

system: $(OBJS)
	$(LD) $(LDFLAG) -e startup_32 -o $@ $^ -M > System.map

head.o : head.S
	$(GCC) -m32 -traditional -c -o $@ $<

sys_call.o : sys_call.S
	$(GCC) -m32 -traditional -c -o $@ $<

main.o : main.c
	$(GCC) $(CCFLAG) -o $@ $<

sched.o : sched.c
	$(GCC) $(CCFLAG) -o $@ $<

fork.o : fork.c
	$(GCC) $(CCFLAG) -o $@ $<

panic.o : panic.c
	$(GCC) $(CCFLAG) -o $@ $<

asm.o : asm.S
	$(GCC) -m32 -traditional -c -o $@ $<

printk.o : printk.c
	$(GCC) $(CCFLAG) -o $@ $<

vsprintf.o : vsprintf.c
	$(GCC) $(CCFLAG) -o $@ $<

traps.o : traps.c
	$(GCC) $(CCFLAG) -o $@ $<

chr_drv/chr_drv.a: chr_drv/*.c
	cd chr_drv; make chr_drv.a; cd ..

blk_drv/blk_drv.a: blk_drv/*.c
	cd blk_drv; make blk_drv.a; cd ..

../mm/mm.o : ../mm/*.c
	cd ../mm; make mm.o; cd ..

../fs/fs.o : ../fs/*.c
	cd ../fs; make fs.o; cd ..

../lib/lib.a : ../lib/*.c
	cd ../lib; make lib.a; cd ../kernel

clean :
	rm -f *.o
	rm -f system
	rm -f System.map
	cd chr_drv; make clean; cd ..
	cd blk_drv; make clean; cd ..
	cd ../lib; make clean; cd ../kernel
	cd ../mm; make clean; cd ../kernel
	cd ../fs; make clean; cd ../kernel

