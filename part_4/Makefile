AS	:=	as
LD	:=	ld

LDFLAG	:=	-Ttext 0x0 -s --oformat binary

image:	linux.img

linux.img:tools/build.bin bootsect.bin setup.bin kernel/system.bin
	./tools/build.bin bootsect.bin setup.bin kernel/system.bin > $@
	
tools/build.bin:tools/build.c
	gcc -o $@ $<

kernel/system.bin:
	cd kernel; make system.bin; cd ..
	
bootsect.bin:bootsect.o
	$(LD) $(LDFLAG) -o  $@ $<

bootsect.o:bootsect.S
	$(AS) -o  $@ $<

setup.bin:setup.o
	$(LD) $(LDFLAG) -e _start_setup -o  $@ $<

setup.o:setup.S
	$(AS) -o  $@ $<

clean:
	rm -f ./*.o 
	rm -f ./bootsect.bin
	rm -f ./setup.bin
	rm -f tools/build.bin
	rm -f linux.img
	cd kernel; make clean; cd ..