#ifndef _BLK_H
#define _BLK_H

#define NR_BLK_DEV      7

#define NR_REQUEST      32

struct request {
    int dev;
    int cmd;
    int errors;
    unsigned long sector;
    unsigned long nr_sectors;
    char * buffer;
    struct task_struct * waiting;
    struct buffer_head * bh;
    struct request * next;
};

#define IN_ORDER(s1,s2) \
((s1)->cmd<(s2)->cmd || ((s1)->cmd==(s2)->cmd && \
((s1)->dev < (s2)->dev || ((s1)->dev == (s2)->dev && \
(s1)->sector < (s2)->sector))))

struct blk_dev_struct {
    void (*request_fn)(void);
    struct request * current_request;
};

extern struct blk_dev_struct blk_dev[NR_BLK_DEV];
extern struct request request[NR_REQUEST];
extern struct task_struct * wait_for_request;

extern int * blk_size[NR_BLK_DEV];

#ifdef MAJOR_NR

#if (MAJOR_NR == 2)
#define DEVICE_NAME "floppy"
#define DEVICE_INTR do_floppy
#define DEVICE_REQUEST do_fd_request
#define DEVICE_NR(device) ((device) & 3)
#define DEVICE_ON(device) floppy_on(DEVICE_NR(device))
#define DEVICE_OFF(device) floppy_off(DEVICE_NR(device))

#elif (MAJOR_NR == 3)
#define DEVICE_NAME "harddisk"
#define DEVICE_INTR do_hd
#define DEVICE_TIMEOUT hd_timeout
#define DEVICE_REQUEST do_hd_request
#define DEVICE_NR(device) (MINOR(device)/5)
#define DEVICE_ON(device)
#define DEVICE_OFF(device)
#endif

#define CURRENT (blk_dev[MAJOR_NR].current_request)
#define CURRENT_DEV DEVICE_NR(CURRENT->dev)

#ifdef DEVICE_INTR
void (*DEVICE_INTR)(void) = NULL;
#endif

extern void unlock_buffer(struct buffer_head * bh);
extern void lock_buffer(struct buffer_head * bh);
extern void end_request(int dev, int uptodate);

#ifdef DEVICE_TIMEOUT
int DEVICE_TIMEOUT = 0;
#define SET_INTR(x) (DEVICE_INTR = (x),DEVICE_TIMEOUT = 200)
#else
#define SET_INTR(x) (DEVICE_INTR = (x))
#endif

void (DEVICE_REQUEST)(void);

#ifdef DEVICE_TIMEOUT
#define CLEAR_DEVICE_TIMEOUT DEVICE_TIMEOUT = 0;
#else
#define CLEAR_DEVICE_TIMEOUT
#endif

#ifdef DEVICE_INTR
#define CLEAR_DEVICE_INTR DEVICE_INTR = 0;
#else
#define CLEAR_DEVICE_INTR
#endif

#define INIT_REQUEST \
repeat: \
    if (!CURRENT) {\
        CLEAR_DEVICE_INTR \
        CLEAR_DEVICE_TIMEOUT \
        return; \
    }\
    if (MAJOR(CURRENT->dev) != MAJOR_NR) \
        printk(DEVICE_NAME ": request list destroyed"); \
    if (CURRENT->bh) { \
        if (!CURRENT->bh->b_lock) \
            printk(DEVICE_NAME ": block not locked"); \
    }

#endif

#endif
