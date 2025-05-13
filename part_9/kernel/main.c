#define __LIBRARY__

#include <unistd.h>
#include <termios.h>

inline _syscall0(int, fork);
inline _syscall1(int,setup,void *,BIOS)

int errno;

#include <asm/system.h>

#include <linux/tty.h>
#include <linux/kernel.h>
#include <linux/sched.h>

extern void mem_init(long start, long end);
extern void hd_init(void);
extern void blk_dev_init(void);
extern void floppy_init();

void init();

#define EXT_MEM_K (*(unsigned short *)0x90002)
#define DRIVE_INFO (*(struct drive_info *)0x90080)
#define ORIG_ROOT_DEV (*(unsigned short *)0x901FC)
#define ORIG_SWAP_DEV (*(unsigned short *)0x901FA)

static long memory_end = 0;
static long buffer_memory_end = 0;
static long main_memory_start = 0;

struct drive_info { char dummy[32]; } drive_info;

void main(void) {
    ROOT_DEV = ORIG_ROOT_DEV;

    drive_info = DRIVE_INFO;

    memory_end = (1<<20) + (EXT_MEM_K<<10);
    memory_end &= 0xfffff000;
    if (memory_end > 16*1024*1024)
        memory_end = 16*1024*1024;
    if (memory_end > 12*1024*1024)
        buffer_memory_end = 4*1024*1024;
    else if (memory_end > 6*1024*1024)
        buffer_memory_end = 2*1024*1024;
    else
        buffer_memory_end = 1*1024*1024;

    main_memory_start = buffer_memory_end;
    mem_init(main_memory_start, memory_end);

    trap_init();
    sched_init();

    tty_init();

    printk("\n\rmemory start: %d, end: %d\n\r", main_memory_start, memory_end);

    buffer_init(buffer_memory_end);
    blk_dev_init();
    hd_init();
    floppy_init();
    move_to_user_mode();
    printf("\x1b[31m In user mode!\n\r\x1b[0m");

    struct termios tms;
    ioctl(0, TCGETS, (unsigned long)&tms);
    tms.c_lflag &= (~ICANON);
    tms.c_lflag &= (~ECHO);
    ioctl(0, TCSETS, (unsigned long)&tms);

    if (fork() == 0) {
        init();
        if (fork() == 0) {
            test_b();
        }
        else {
            test_a();
        }
    }

    while (1) {}
}

void init() {
    setup((void *) &drive_info);
}

