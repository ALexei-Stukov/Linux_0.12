AS := as
LD := ld -m elf_x86_64

LDFLAG := -Ttext 0x0 -s --oformat binary

image : linux.img

linux.img : tools/build bootsect setup kernel/system
	./tools/build bootsect setup kernel/system > $@

tools/build : tools/build.c
	gcc -o $@ $<

kernel/system : kernel/head.S kernel/*.c mm/*.c kernel/blk_drv/*.c
	cd kernel; make system; cd ..

bootsect : bootsect.o
	$(LD) $(LDFLAG) -o $@ $<

bootsect.o : bootsect.S
	$(AS) -o $@ $<

setup : setup.o
	$(LD) $(LDFLAG) -e _start_setup -o $@ $<

setup.o : setup.S
	$(AS) -o $@ $<

chr_drv/chr_drv.a: chr_drv/*.c
	cd chr_drv; make chr_drv.a; cd ..

clean:
	rm -f *.o
	rm -f bootsect
	rm -f setup
	rm -f tools/build
	rm -f linux.img
	cd kernel; make clean; cd ..

