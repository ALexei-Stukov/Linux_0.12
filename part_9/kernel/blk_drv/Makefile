AR  := ar
LD  := ld
GCC := gcc
CCFLAG := -I../../include -nostdinc -ffreestanding -Wall -fomit-frame-pointer -c -fno-stack-protector -fno-pie -fno-pic -m32
OBJS   := hd.o ll_rw_blk.o floppy.o

blk_drv.a : $(OBJS)
	$(AR) rcs $@ $^
	sync

hd.o : hd.c
	$(GCC) $(CCFLAG) -o $@ $<

floppy.o : floppy.c
	$(GCC) $(CCFLAG) -o $@ $<

ll_rw_blk.o : ll_rw_blk.c
	$(GCC) $(CCFLAG) -o $@ $<

clean : 
	rm -f *.o
	rm -f blk_drv.a

