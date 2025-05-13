#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <linux/sched.h>

extern int tty_ioctl(int dev, int cmd, int arg);

int sys_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg) {
    if (fd == 0 || fd == 1) {
        tty_ioctl(0, cmd, arg);
    }
    return 0;
}

