AR  := ar
LD  := ld
GCC := gcc
CCFLAG := -m32 -I../include -nostdinc -ffreestanding -fno-pic -Wall -fomit-frame-pointer -fno-stack-protector -c
OBJS   := string.o ctype.o file.o

lib.a : $(OBJS)
	$(AR) rcs $@ $^
	sync

string.o : string.c
	$(GCC) $(CCFLAG) -o $@ $<

ctype.o : ctype.c
	$(GCC) $(CCFLAG) -o $@ $<

file.o : file.c
	$(GCC) $(CCFLAG) -o $@ $<

clean : 
	rm *.o
	rm lib.a