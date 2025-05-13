#include <errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>

#include "blk.h"

struct request request[NR_REQUEST];

struct blk_dev_struct blk_dev[NR_BLK_DEV] = {
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
    { NULL, NULL },
};

int * blk_size[NR_BLK_DEV] = { NULL, NULL, };

struct task_struct * wait_for_request = NULL;

void lock_buffer(struct buffer_head * bh) {
    cli();
    while (bh->b_lock)
        sleep_on(&bh->b_wait);
    bh->b_lock=1;
    sti();
}

void unlock_buffer(struct buffer_head * bh) {
    if (!bh->b_lock)
        printk("ll_rw_block.c: buffer not locked\n\r");
    bh->b_lock = 0;
    wake_up(&bh->b_wait);
}

static void add_request(struct blk_dev_struct * dev, struct request * req) {
    struct request * tmp;
    req->next = NULL;
    cli();

    if (req->bh)
        req->bh->b_dirt = 0;

    if (!(tmp = dev->current_request)) {
        dev->current_request = req;
        sti();
        (dev->request_fn)();
        return;
    }

    for ( ; tmp->next ; tmp=tmp->next) {
        if (!req->bh) {
            if (tmp->next->bh)
                break;
            else
                continue;
        }

        if ((IN_ORDER(tmp,req) ||
                    !IN_ORDER(tmp,tmp->next)) &&
                IN_ORDER(req,tmp->next))
            break;
    }

    req->next=tmp->next;
    tmp->next=req;
    sti();
}

static void make_request(int major,int rw, struct buffer_head * bh) {
    struct request * req;
    lock_buffer(bh);
    if ((rw == WRITE && !bh->b_dirt) || (rw == READ && bh->b_uptodate)) {
        unlock_buffer(bh);
        return;
    }

repeat:
    if (rw == READ)
        req = request+NR_REQUEST;
    else
        req = request+((NR_REQUEST*2)/3);

    while (--req >= request)
        if (req->dev<0)
            break;

    if (req < request) {
        sleep_on(&wait_for_request);
        goto repeat;
    }

    req->dev = bh->b_dev;
    req->cmd = rw;
    req->errors=0;
    req->sector = bh->b_blocknr<<1;
    req->nr_sectors = 2;
    req->buffer = bh->b_data;
    req->waiting = NULL;
    req->bh = bh;
    req->next = NULL;
    add_request(major+blk_dev,req);
}

void ll_rw_block(int rw, struct buffer_head * bh) {
    unsigned int major;
    major = MAJOR(bh->b_dev);

    make_request(major,rw,bh);
}

void end_request(int dev, int uptodate) {
    struct request * req = blk_dev[dev].current_request;
    printk("end request\n");
    //DEVICE_OFF(dev);
    if (req->bh) {
        req->bh->b_uptodate = uptodate;
        unlock_buffer(req->bh);
    }
    if (!uptodate) {
        printk("I/O error\n\r");
        printk("dev %04x, block %d\n\r", dev,
                 req->bh->b_blocknr);
    }
    req->dev = -1;
    blk_dev[dev].current_request = req->next;
}

void blk_dev_init() {
    int i;

    for (i=0 ; i<NR_REQUEST ; i++) {
        request[i].dev = -1;
        request[i].next = NULL;
    }
}
