#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/fdreg.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#define MAJOR_NR 2
#include "blk.h"

static int recalibrate = 0;
static int reset = 0;
static int seek = 0;

extern unsigned char current_DOR;

#define immoutb_p(val,port) \
__asm__("outb %0,%1\n\tjmp 1f\n1:\tjmp 1f\n1:"::"a" ((char) (val)),"i" (port))

#define TYPE(x) ((x)>>2)
#define DRIVE(x) ((x)&0x03)

#define MAX_ERRORS 8
#define MAX_REPLIES 7
static unsigned char reply_buffer[MAX_REPLIES];
#define ST0 (reply_buffer[0])
#define ST1 (reply_buffer[1])
#define ST2 (reply_buffer[2])
#define ST3 (reply_buffer[3])

static struct floppy_struct {
    unsigned int size, sect, head, track, stretch;
    unsigned char gap,rate,spec1;
} floppy_type[] = {
    {    0, 0,0, 0,0,0x00,0x00,0x00 },
    {  720, 9,2,40,0,0x2A,0x02,0xDF },
    { 2400,15,2,80,0,0x1B,0x00,0xDF },
    {  720, 9,2,40,1,0x2A,0x02,0xDF },
    { 1440, 9,2,80,0,0x2A,0x02,0xDF },
    {  720, 9,2,40,1,0x23,0x01,0xDF },
    { 1440, 9,2,80,0,0x23,0x01,0xDF },
    { 2880,18,2,80,0,0x1B,0x00,0xCF },
};

extern void floppy_interrupt();
extern char tmp_floppy_area[1024];

static int cur_spec1 = -1;
static int cur_rate = -1;
static struct floppy_struct * floppy = floppy_type;
static unsigned char current_drive = 0;
static unsigned char sector = 0;
static unsigned char head = 0;
static unsigned char track = 0;
static unsigned char seek_track = 0;
static unsigned char current_track = 255;
static unsigned char command = 0;
unsigned char selected = 0;
struct task_struct * wait_on_floppy_select = NULL;

#define copy_buffer(from,to) \
__asm__("cld ; rep ; movsl" \
        ::"c" (BLOCK_SIZE/4),"S" ((long)(from)),"D" ((long)(to)) \
        :)

void floppy_deselect(unsigned int nr) {
    if (nr != (current_DOR & 3))
        printk("floppy_deselect: drive not selected\n\r");
    selected = 0;
    wake_up(&wait_on_floppy_select);
}

int result() {
    int i = 0, counter, status;
    if (reset)
        return -1;
    for (counter = 0 ; counter < 10000 ; counter++) {
        status = inb_p(FD_STATUS)&(STATUS_DIR|STATUS_READY|STATUS_BUSY);
        if (status == STATUS_READY)
            return i;
        if (status == (STATUS_DIR|STATUS_READY|STATUS_BUSY)) {
            if (i >= MAX_REPLIES)
                break;
            reply_buffer[i++] = inb_p(FD_DATA);
        }
    }
    reset = 1;
    printk("Getstatus times out\n\r");
    return -1;
}

void bad_flp_intr() {
    CURRENT->errors++;
    if (CURRENT->errors > MAX_ERRORS) {
        floppy_deselect(current_drive);
        end_request(MAJOR_NR, 0);
    }

    if (CURRENT->errors > MAX_ERRORS/2)
        reset = 1;
    else
        recalibrate = 1;
}

void setup_DMA() {
    long addr = (long) CURRENT->buffer;
    cli();
    if (addr >= 0x100000) {
        addr = (long) tmp_floppy_area;
        if (command == FD_WRITE)
            copy_buffer(CURRENT->buffer,tmp_floppy_area);
    }
    immoutb_p(4|2,10);
    __asm__("outb %%al,$12\n\tjmp 1f\n1:\tjmp 1f\n1:\t"
            "outb %%al,$11\n\tjmp 1f\n1:\tjmp 1f\n1:"::
            "a" ((char) ((command == FD_READ)?DMA_READ:DMA_WRITE)));
    immoutb_p(addr,4);
    addr >>= 8;
    immoutb_p(addr,4);
    addr >>= 8;
    immoutb_p(addr,0x81);
    immoutb_p(0xff,5);
    immoutb_p(3,5);
    immoutb_p(0|2,10);
    sti();
}

void output_byte(char byte) {
    int counter;
    unsigned char status;
    if (reset)
        return;
    for(counter = 0 ; counter < 10000 ; counter++) {
        status = inb_p(FD_STATUS) & (STATUS_READY | STATUS_DIR);
        if (status == STATUS_READY) {
            outb(byte,FD_DATA);
            return;
        }
    }
    reset = 1;
    printk("Unable to send byte to FDC\n\r");
}

void rw_interrupt() {
    if (result() != 7 || (ST0 & 0xf8) || (ST1 & 0xbf) || (ST2 & 0x73)) {
        if (ST1 & 0x02) {
            printk("Drive %d is write protected\n\r",current_drive);
            floppy_deselect(current_drive);
            end_request(MAJOR_NR, 0);
        }
        else {
            bad_flp_intr();
        }

        do_fd_request();
        return;
    }

    if (command == FD_READ && (unsigned long)(CURRENT->buffer) >= 0x100000)
        copy_buffer(tmp_floppy_area,CURRENT->buffer);
    floppy_deselect(current_drive);
    end_request(MAJOR_NR, 1);
    do_fd_request();
}

static inline void setup_rw_floppy() {
    setup_DMA();
    do_floppy = rw_interrupt;
    output_byte(command);
    output_byte(head<<2 | current_drive);
    output_byte(track);
    output_byte(head);
    output_byte(sector);
    output_byte(2);
    output_byte(floppy->sect);
    output_byte(floppy->gap);
    output_byte(0xFF);

    if (reset)
        do_fd_request();
}

void seek_interrupt() {
    output_byte(FD_SENSEI);
    if (result() != 2 || (ST0 & 0xF8) != 0x20 || ST1 != seek_track) {
        bad_flp_intr();
        do_fd_request();
        return;
    }
    current_track = ST1;
    setup_rw_floppy();
}

void unexpected_floppy_interrupt() {
    output_byte(FD_SENSEI);
    int res = result();
    printk("unexpected floppy interrupt %d, %d\n", res, ST0);
    if (res != 2 || (ST0 & 0xE0) == 0x60)
        reset = 1;
    else
        recalibrate = 1;
}

void transfer() {
    if (cur_spec1 != floppy->spec1) {
        cur_spec1 = floppy->spec1;
        output_byte(FD_SPECIFY);
        output_byte(cur_spec1);
        output_byte(6);
    }
    if (cur_rate != floppy->rate)
        outb_p(cur_rate = floppy->rate,FD_DCR);

    if (reset) {
        do_fd_request();
        return;
    }

    if (!seek) {
        setup_rw_floppy();
        return;
    }
    do_floppy = seek_interrupt;
    if (seek_track) {
        output_byte(FD_SEEK);
        output_byte(head<<2 | current_drive);
        output_byte(seek_track);
    }
    else {
        output_byte(FD_RECALIBRATE);
        output_byte(head<<2 | current_drive);
    }

    if (reset)
        do_fd_request();
}

void recal_interrupt() {
    output_byte(FD_SENSEI);
    if (result()!=2 || (ST0 & 0xE0) == 0x60)
        reset = 1;
    else
        recalibrate = 0;
    do_fd_request();
}

void recalibrate_floppy() {
    recalibrate = 0;
    current_track = 0;
    do_floppy = recal_interrupt;
    output_byte(FD_RECALIBRATE);
    output_byte(head<<2 | current_drive);
    if (reset)
        do_fd_request();
}

void reset_interrupt() {
    output_byte(FD_SENSEI);
    (void) result();
    output_byte(FD_SPECIFY);
    output_byte(cur_spec1);
    output_byte(6);
    do_fd_request();
}

void reset_floppy() {
    int i;
    reset = 0;
    cur_spec1 = -1;
    cur_rate = -1;
    recalibrate = 1;
    printk("Reset-floppy called\n\r");
    cli();
    do_floppy = reset_interrupt;
    outb_p(current_DOR & ~0x04,FD_DOR);
    for (i=0 ; i<100 ; i++)
        __asm__("nop");
    outb(current_DOR,FD_DOR);
    sti();
}

void floppy_on_interrupt() {
    selected = 1;
    if (current_drive != (current_DOR & 3)) {
        current_DOR &= 0xFC;
        current_DOR |= current_drive;
        outb(current_DOR,FD_DOR);
        add_timer(2,&transfer);
    }
    else {
        transfer();
    }
}

void do_fd_request() {
    printk("hinus debug: do_fd_request, reset: %d, recal: %d, current dev:%d\n",
            reset, recalibrate, CURRENT->dev);
    unsigned int block;
    seek = 0;

    if (reset) {
        reset_floppy();
        return;
    }

    if (recalibrate) {
        recalibrate_floppy();
        return;
    }

    INIT_REQUEST;
    floppy = (MINOR(CURRENT->dev)>>2) + floppy_type;

    if (current_drive != CURRENT_DEV)
        seek = 1;

    current_drive = CURRENT_DEV;
    block = CURRENT->sector;
    if (block+2 > floppy->size) {
        end_request(MAJOR_NR, 0);
        goto repeat;
    }

    sector = block % floppy->sect;
    block /= floppy->sect;
    head = block % floppy->head;
    track = block / floppy->head;
    seek_track = track << floppy->stretch;

    if (seek_track != current_track)
        seek = 1;
    sector++;
    if (CURRENT->cmd == READ)
        command = FD_READ;
    else if (CURRENT->cmd == WRITE)
        command = FD_WRITE;
    else
        printk("do_fd_request: unknown command");

    add_timer(ticks_to_floppy_on(current_drive),&floppy_on_interrupt);
}

static int floppy_sizes[] ={
    0,   0,   0,   0,
    360, 360 ,360, 360,
    1200,1200,1200,1200,
    360, 360, 360, 360,
    720, 720, 720, 720,
    360, 360, 360, 360,
    720, 720, 720, 720,
    1440,1440,1440,1440
};

void floppy_init() {
    blk_size[MAJOR_NR] = floppy_sizes;
    blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;
    set_trap_gate(0x26,&floppy_interrupt);
    outb(inb_p(0x21)&~0x40,0x21);
}

