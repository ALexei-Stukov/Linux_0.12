#include <errno.h>
#include <termios.h>

#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/tty.h>

#include <asm/io.h>
#include <asm/segment.h>
#include <asm/system.h>

static int get_termios(struct tty_struct * tty, struct termios * termios) {
    int i;

    verify_area(termios, sizeof (*termios));
    for (i=0 ; i< (sizeof (*termios)) ; i++)
        put_fs_byte( ((char *)&tty->termios)[i] , i+(char *)termios );
    return 0;
}

static int set_termios(struct tty_struct * tty, struct termios * termios, int channel) {
    int i;

    for (i=0 ; i< (sizeof (*termios)) ; i++)
        ((char *)&tty->termios)[i]=get_fs_byte(i+(char *)termios);
    //change_speed(tty);
    return 0;
}

static int get_termio(struct tty_struct * tty, struct termio * termio) {
    int i;
    struct termio tmp_termio;

    verify_area(termio, sizeof (*termio));
    tmp_termio.c_iflag = tty->termios.c_iflag;
    tmp_termio.c_oflag = tty->termios.c_oflag;
    tmp_termio.c_cflag = tty->termios.c_cflag;
    tmp_termio.c_lflag = tty->termios.c_lflag;
    tmp_termio.c_line = tty->termios.c_line;
    for(i=0 ; i < NCC ; i++)
        tmp_termio.c_cc[i] = tty->termios.c_cc[i];
    for (i=0 ; i< (sizeof (*termio)) ; i++)
        put_fs_byte( ((char *)&tmp_termio)[i] , i+(char *)termio );
    return 0;
}

static int set_termio(struct tty_struct * tty, struct termio * termio,
            int channel) {
    int i;
    struct termio tmp_termio;

    for (i=0 ; i< (sizeof (*termio)) ; i++)
        ((char *)&tmp_termio)[i]=get_fs_byte(i+(char *)termio);
    *(unsigned short *)&tty->termios.c_iflag = tmp_termio.c_iflag;
    *(unsigned short *)&tty->termios.c_oflag = tmp_termio.c_oflag;
    *(unsigned short *)&tty->termios.c_cflag = tmp_termio.c_cflag;
    *(unsigned short *)&tty->termios.c_lflag = tmp_termio.c_lflag;
    tty->termios.c_line = tmp_termio.c_line;
    for(i=0 ; i < NCC ; i++)
        tty->termios.c_cc[i] = tmp_termio.c_cc[i];
    //change_speed(tty);
    return 0;
}

static void flush(struct tty_queue * queue) {
    cli();
    queue->head = queue->tail;
    sti();
}

static void wait_until_sent(struct tty_struct * tty) {
    /* do nothing - not implemented */
}

static void send_break(struct tty_struct * tty) {
    /* do nothing - not implemented */
}

int tty_ioctl(int dev, int cmd, int arg) {
    struct tty_struct * tty;

    tty = tty_table + dev;
    switch (cmd) {
        case TCGETS:
            return get_termios(tty,(struct termios *) arg);
        case TCSETSF:
            flush(&tty->read_q); /* fallthrough */
        case TCSETSW:
            wait_until_sent(tty); /* fallthrough */
        case TCSETS:
            return set_termios(tty,(struct termios *) arg, dev);
        case TCGETA:
            return get_termio(tty,(struct termio *) arg);
        case TCSETAF:
            flush(&tty->read_q); /* fallthrough */
        case TCSETAW:
            wait_until_sent(tty); /* fallthrough */
        case TCSETA:
            return set_termio(tty,(struct termio *) arg, dev);
        case TCSBRK:
            if (!arg) {
                wait_until_sent(tty);
                send_break(tty);
            }
            return 0;
        case TCXONC:
            switch (arg) {
            case TCOOFF:
                tty->stopped = 1;
                tty->write(tty);
                return 0;
            case TCOON:
                tty->stopped = 0;
                tty->write(tty);
                return 0;
            case TCIOFF:
                if (STOP_CHAR(tty))
                    PUTCH(STOP_CHAR(tty), &tty->write_q);
                return 0;
            case TCION:
                if (START_CHAR(tty))
                    PUTCH(START_CHAR(tty), &tty->write_q);
                return 0;
            }
            return -EINVAL; /* not implemented */
        case TCFLSH:
            if (arg==0)
                flush(&tty->read_q);
            else if (arg==1)
                flush(&tty->write_q);
            else if (arg==2) {
                flush(&tty->read_q);
                flush(&tty->write_q);
            } else
                return -EINVAL;
            return 0;
        case TIOCEXCL:
            return -EINVAL; /* not implemented */
        case TIOCNXCL:
            return -EINVAL; /* not implemented */
        case TIOCSCTTY:
            return -EINVAL; /* set controlling term NI */
        case TIOCGPGRP:
            verify_area((void *) arg,4);
            put_fs_long(tty->pgrp,(unsigned long *) arg);
            return 0;
        case TIOCSPGRP:
            return 0;
        case TIOCOUTQ:
            verify_area((void *) arg,4);
            put_fs_long(CHARS(&tty->write_q),(unsigned long *) arg);
            return 0;
        case TIOCINQ:
            verify_area((void *) arg,4);
            put_fs_long(CHARS(&tty->secondary),
                (unsigned long *) arg);
            return 0;
        case TIOCSTI:
            return -EINVAL; /* not implemented */
        case TIOCGWINSZ:
            return -EINVAL; /* not implemented */
        case TIOCSWINSZ:
            return -EINVAL; /* not implemented */
        case TIOCMGET:
            return -EINVAL; /* not implemented */
        case TIOCMBIS:
            return -EINVAL; /* not implemented */
        case TIOCMBIC:
            return -EINVAL; /* not implemented */
        case TIOCMSET:
            return -EINVAL; /* not implemented */
        case TIOCGSOFTCAR:
            return -EINVAL; /* not implemented */
        case TIOCSSOFTCAR:
            return -EINVAL; /* not implemented */
        default:
            return -EINVAL;
    }
}
