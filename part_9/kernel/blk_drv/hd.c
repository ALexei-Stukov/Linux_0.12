#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/hdreg.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/segment.h>

#define MAJOR_NR 3

#include "blk.h"

#define CMOS_READ(addr) ({ \
outb_p(0x80|addr,0x70); \
inb_p(0x71); \
})

#define MAX_HD          2

struct hd_i_struct {
    int head,sect,cyl,wpcom,lzone,ctl;
};

struct task_struct* waiting;

struct buffer_head* bh;

#ifdef HD_TYPE
struct hd_i_struct hd_info[] = { HD_TYPE };
#define NR_HD ((sizeof (hd_info))/(sizeof (struct hd_i_struct)))
#else
struct hd_i_struct hd_info[] = { {0,0,0,0,0,0},{0,0,0,0,0,0} };
static int NR_HD = 0;
#endif

static struct hd_struct {
    long start_sect;
    long nr_sects;
} hd[5*MAX_HD]={{0,0},};

static int hd_sizes[5*MAX_HD] = {0, };

#define port_read(port,buf,nr) \
__asm__("cld;rep;insw"::"d" (port),"D" (buf),"c" (nr):)

#define port_write(port,buf,nr) \
__asm__("cld;rep;outsw"::"d" (port),"S" (buf),"c" (nr):)

extern void hd_interrupt(void);

static void hd_out(unsigned int drive,unsigned int nsect,unsigned int sect,
        unsigned int head,unsigned int cyl,unsigned int cmd,
        void (*intr_addr)(void)) {
    register int port asm("dx");

    SET_INTR(intr_addr);
    outb_p(hd_info[drive].ctl,HD_CMD);
    port=HD_DATA;
    outb_p(hd_info[drive].wpcom>>2,++port);
    outb_p(nsect,++port);
    outb_p(sect,++port);
    outb_p(cyl,++port);
    outb_p(cyl>>8,++port);
    outb_p(0xA0|(drive<<4)|head,++port);
    outb(cmd,++port);
}

void identity_callback() {
    printk("in identity callback. \n");
    port_read(HD_DATA, bh->b_data, 256);
    wake_up(&waiting);
}

void hd_identity() {
    bh = getblk(3, 0);
    lock_buffer(bh);

    hd_out(0, 0, 0, 0, 0, WIN_IDENTIFY, identity_callback);
    sleep_on(&waiting);
    short* buf = (short*)bh->b_data;
    int sectors = ((int)buf[61] << 16) + buf[60];
    printk("HD size: %dMB\n", sectors * 512 / 1024 / 1024);
}

int sys_setup(void * BIOS) {
    int i,drive;
    unsigned char cmos_disks;
    struct partition *p;
    //struct buffer_head * bh;

#ifndef HD_TYPE
    for (drive=0 ; drive<2 ; drive++) {
        hd_info[drive].cyl = *(unsigned short *) BIOS;
        hd_info[drive].head = *(unsigned char *) (2+BIOS);
        hd_info[drive].wpcom = *(unsigned short *) (5+BIOS);
        hd_info[drive].ctl = *(unsigned char *) (8+BIOS);
        hd_info[drive].lzone = *(unsigned short *) (12+BIOS);
        hd_info[drive].sect = *(unsigned char *) (14+BIOS);
        BIOS += 16;
    }

    if (hd_info[1].cyl)
        NR_HD = 2;
    else
        NR_HD = 1;
#endif

    printk("computer has %d disks\n\r", NR_HD);

    for (i=0 ; i<NR_HD ; i++) {
        hd[i*5].start_sect = 0;
        hd[i*5].nr_sects = hd_info[i].head *
            hd_info[i].sect * hd_info[i].cyl;
        printk("disk %d has %d sects\n\r", i, hd[i*5].nr_sects);
    }

    if ((cmos_disks = CMOS_READ(0x12)) & 0xf0) {
        if (cmos_disks & 0x0f)
            NR_HD = 2;
        else
            NR_HD = 1;
    }
    else
        NR_HD = 0;

    for (i = NR_HD ; i < 2 ; i++) {
        hd[i*5].start_sect = 0;
        hd[i*5].nr_sects = 0;
    }

    
    for (i = 0; i < NR_HD; i++) {
        printk("sectors for hd %d is %d\n", i, hd[i].nr_sects);
    }

    //hd_identity();

    for (drive = 0; drive < NR_HD; drive++) {
        if (!(bh = bread(0x300 + drive*5,0))) {
            printk("Unable to read partition table of drive %d\n\r",
                    drive);
        }

        if (bh->b_data[510] != 0x55 || (unsigned char) bh->b_data[511] != 0xAA) {
            printk("Bad partition table on drive %d\n\r",drive);
        }

        p = 0x1BE + (void *)bh->b_data;
        for (i=1;i<5;i++,p++) {
            hd[i+5*drive].start_sect = p->start_sect;
            hd[i+5*drive].nr_sects = p->nr_sects;

            printk("table %d: start at %d, has %d sects\n", drive,
                hd[i+5*drive].start_sect,
                hd[i+5*drive].nr_sects);
        }
    }

    for (i=0 ; i<5*MAX_HD ; i++)
        hd_sizes[i] = hd[i].nr_sects>>1;
    blk_size[MAJOR_NR] = hd_sizes;

    add_timer(300, test_c);
    mount_root();
    return 0;
}

static int win_result(void) {
    int i = inb_p(HD_STATUS);
    if ((i & (BUSY_STAT | READY_STAT | WRERR_STAT | SEEK_STAT | ERR_STAT))
             == (READY_STAT | SEEK_STAT))
        return 0;
    if (i&1) i = inb(HD_ERROR);
    return (1);
}

void unexpected_hd_interrupt() {
    printk("Unexpected HD interrupt\n\r");
}

static void read_intr(void) {
    printk("read callback\n");
    if (win_result()) {
        printk("hd read error.\n");
        do_hd_request();
        return;
    }
    port_read(HD_DATA,CURRENT->buffer,256);
    CURRENT->errors = 0;
    CURRENT->buffer += 512;
    CURRENT->sector++;
    if (--CURRENT->nr_sectors) {
        printk("nr_sectors:%d\n", CURRENT->nr_sectors);
        SET_INTR(&read_intr);
        return;
    }
    end_request(MAJOR_NR, 1);
    do_hd_request();
}

static void write_intr() {
    if (win_result()) {
        do_hd_request();
    }

    if (--CURRENT->nr_sectors) {
        CURRENT->sector++;
        CURRENT->buffer += 512;
        SET_INTR(&write_intr);
        port_write(HD_DATA,CURRENT->buffer,256);
        return;
    }

    end_request(MAJOR_NR, 1);
    do_hd_request();
}

void do_hd_request() {
    int i,r;
    unsigned int block,dev;
    unsigned int sec,head,cyl;
    unsigned int nsect;

    INIT_REQUEST;
    dev = MINOR(CURRENT->dev);
    block = CURRENT->sector;

    if (dev >= 5*NR_HD || block+2 > hd[dev].nr_sects) {
        end_request(MAJOR_NR, 0);
        goto repeat;
    }

    block += hd[dev].start_sect;
    dev /= 5;

    __asm__("divl %4":"=a" (block),"=d" (sec):"0" (block),"1" (0),
            "r" (hd_info[dev].sect));
    __asm__("divl %4":"=a" (cyl),"=d" (head):"0" (block),"1" (0),
            "r" (hd_info[dev].head));
    sec++;
    nsect = CURRENT->nr_sectors;

    if (CURRENT->cmd == WRITE) {
        hd_out(dev,nsect,sec,head,cyl,WIN_WRITE,&write_intr);
        for(i=0 ; i<10000 && !(r=inb_p(HD_STATUS)&DRQ_STAT) ; i++)
            /*nothing*/;
        port_write(HD_DATA,CURRENT->buffer,256);
    }
    else if (CURRENT->cmd == READ) {
        hd_out(dev,nsect,sec,head,cyl,WIN_READ,&read_intr);
    }
    else
        printk("unknown hd-command");
}

void hd_init() {
    blk_dev[MAJOR_NR].request_fn = DEVICE_REQUEST;
    set_intr_gate(0x2E,&hd_interrupt);
    outb_p(inb_p(0x21)&0xfb,0x21);
    outb(inb_p(0xA1)&0xbf,0xA1);
}

