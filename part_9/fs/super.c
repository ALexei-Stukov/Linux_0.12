#include <linux/config.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <asm/system.h>

#include <errno.h>

int ROOT_DEV = 0;

struct super_block sb;

struct super_block * read_super(int dev) {
    struct super_block * s = &sb;
    struct buffer_head * bh;
    if (!(bh = bread(dev,1))) {
        printk("read super error!\n");
    }

    brelse(bh);

    *((struct d_super_block *) s) =
        *((struct d_super_block *) bh->b_data);

    if (s->s_magic != SUPER_MAGIC) {
        printk("read super error!\n");
        return NULL;
    }
    printk("read super successfully!\n");

    return s;
}

void mount_root() {
    struct super_block * p;
    if (!(p=read_super(ROOT_DEV))) {
        printk("Unable to mount root");
    }
}

