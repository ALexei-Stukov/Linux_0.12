GCC := gcc
LD  := ld
CCFLAG := -I../include -ffreestanding -fno-pic -Wall -fomit-frame-pointer -fno-stack-protector -c -m32
LDFLAG := -Ttext 0x0 -s --oformat binary -m elf_i386
INCDIR := ../include
OBJS   := memory.o swap.o page.o

mm.o : $(OBJS)
	$(LD) -m elf_i386 -r -o $@ $^

swap.o : swap.c
	$(GCC) $(CCFLAG) -o $@ $<

memory.o : memory.c
	$(GCC) $(CCFLAG) -o $@ $<

page.o : page.S
	$(GCC) -m32 -traditional -c -o $@ $<

clean : 
	rm *.o

