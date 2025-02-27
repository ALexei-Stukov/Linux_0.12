GCC		:=	gcc
CCFLAG 	:=	-mcpu-i386 -I../include -nostdinc -Wall -fomit-frame-pointer -c

LDFLAG	:=	-Ttext 0x0 -s --oformat binary
OBJS	:=	head.o

system.bin:$(OBJS)
	$(LD)	$(LDFLAG) -e startup_32 -o $@ $^

head.o: head.S
	$(GCC)	-traditional -c -o $@ $^

clean:
	rm *.o
	rm system.bin