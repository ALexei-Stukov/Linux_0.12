GCC := gcc
LD  := ld
CCFLAG := -I../include -ffreestanding -Wall -fomit-frame-pointer -c -fno-stack-protector -fno-pie -fno-pic -m32
LDFLAG := -Ttext 0x0 -s --oformat binary -m elf_i386
INCDIR := ../include
OBJS   := read_write.o ioctl.o buffer.o super.o

fs.o : $(OBJS)
	$(LD) -m elf_i386 -r -o $@ $^

read_write.o : read_write.c
	$(GCC) $(CCFLAG) -o $@ $<

ioctl.o : ioctl.c
	$(GCC) $(CCFLAG) -o $@ $<

buffer.o : buffer.c
	$(GCC) $(CCFLAG) -o $@ $<

super.o : super.c
	$(GCC) $(CCFLAG) -o $@ $<

clean : 
	rm -f *.o

