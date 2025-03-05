AR  := ar
LD  := ld
GCC := gcc
CCFLAG := -m32 -I../../include -nostdinc -ffreestanding -fno-pic -Wall -fomit-frame-pointer -fno-stack-protector -c

OBJS   := memory.o

mm.o : $(OBJS)
	$(LD) -m elf_i386 -r -o $@ $<

memory.o : memory.c
	$(GCC) $(CCFLAG) -o $@ $<

clean :
	rm -f *.o
