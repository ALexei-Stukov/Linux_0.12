#include <sys/stat.h>
#include <errno.h>
#include <sys/types.h>

#include <linux/kernel.h>
#include <linux/sched.h>

extern int tty_write(int channel, const char* buf, int count);
extern int tty_read(unsigned channel, char * buf, int nr);

int sys_write(unsigned int fd,char * buf,int count) {
    if (fd == 1) {
        return tty_write(0, buf, count);
    }

    return 0;
}

int sys_read(unsigned int fd,char * buf,int count) {
    if (fd == 0) {
        return tty_read(0, buf, count);
    }

    return 0;
}

