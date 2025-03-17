#include <ctype.h>

#include <linux/tty.h>
#include <linux/sched.h>
#include <asm/segment.h>

#define _L_FLAG(tty,f)  ((tty)->termios.c_lflag & f)
#define _O_FLAG(tty,f)  ((tty)->termios.c_oflag & f)
#define _I_FLAG(tty,f)  ((tty)->termios.c_iflag & f)

#define L_CANON(tty)    _L_FLAG((tty),ICANON)
#define L_ECHO(tty)     _L_FLAG((tty),ECHO)
#define L_ECHOCTL(tty)  _L_FLAG((tty),ECHOCTL)

#define O_POST(tty)     _O_FLAG((tty),OPOST)
#define O_NLCR(tty)     _O_FLAG((tty),ONLCR)
#define O_CRNL(tty)     _O_FLAG((tty),OCRNL)
#define O_NLRET(tty)    _O_FLAG((tty),ONLRET)
#define O_LCUC(tty)     _O_FLAG((tty),OLCUC)

#define I_UCLC(tty)     _I_FLAG((tty),IUCLC)
#define I_NLCR(tty)     _I_FLAG((tty),INLCR)
#define I_CRNL(tty)     _I_FLAG((tty),ICRNL)
#define I_NOCR(tty)     _I_FLAG((tty),IGNCR)

struct tty_struct tty_table[] = {
    {
        {
            ICRNL,
            OPOST | ONLCR,
            0,
            ISIG | ICANON | ECHO | ECHOCTL | ECHOKE,
            0,
            INIT_C_CC
        },
        0,
        0,
        con_write,
        {0, 0, 0, 0, ""},
        {0, 0, 0, 0, ""},
        {0, 0, 0, 0, ""},
    }
};

unsigned long CHARS(struct tty_queue* q) {
    return (q->head - q->tail) & (TTY_BUF_SIZE - 1);
}

void PUTCH(char c, struct tty_queue* q) {
    q->buf[q->head++] = c;
    q->head &= (TTY_BUF_SIZE - 1);
}

char GETCH(struct tty_queue* q) {
    char c = q->buf[q->tail++];
    q->tail &= (TTY_BUF_SIZE - 1);
    return c;
}

char EMPTY(struct tty_queue* q) {
    return q->tail == q->head;
}

void put_fs_byte(char val,char *addr) {
    __asm__ ("movb %0,%%fs:%1"::"r" (val),"m" (*addr));
}

char get_fs_byte(const char * addr) {
    unsigned register char _v;
    __asm__ ("movb %%fs:%1,%0":"=r" (_v):"m" (*addr));
    return _v;
}

void put_fs_long(unsigned long val,unsigned long * addr) {
    __asm__ ("movl %0,%%fs:%1"::"r" (val),"m" (*addr));
}

void tty_init() {
    con_init();
}

int tty_write(unsigned channel, char* buf, int nr) {
    static int cr_flag = 0;
    struct tty_struct * tty;
    char c, *b=buf;

    if (channel > 2 || nr < 0)
        return -1;

    tty = tty_table + channel;
    while (nr > 0) {
        c = get_fs_byte(b);
        if (O_POST(tty)) {
            if (c=='\r' && O_CRNL(tty))
                c = '\n';
            else if (c=='\n' && O_NLRET(tty))
                c = '\r';

            if (c=='\n' && !cr_flag && O_NLCR(tty)) {
                cr_flag = 1;
                PUTCH(13, &tty->write_q);
                continue;
            }

            if (O_LCUC(tty)) {
                c = toupper(c);
            }
        }

        b++; nr--;
        cr_flag = 0;
        PUTCH(c, &tty->write_q);
    }

    tty->write(tty);
    return (b-buf);
}

int tty_read(unsigned channel, char * buf, int nr) {
    struct tty_struct * tty;
    char c, * b=buf;

    if (channel > 2 || nr < 0)
        return -1;

    tty = tty_table + channel;

    while (nr > 0) {
        if (EMPTY(&tty->secondary) || (L_CANON(tty) &&
            !FULL(tty->read_q) && !tty->secondary.data)) {
            interruptible_sleep_on(&tty->secondary.proc_list);
            continue;
        }

        do {
            c = GETCH(&tty->secondary);
            if ((c == EOF_CHAR(tty)) || (c == 10)) {
                tty->secondary.data--;
            }
            if ((c == EOF_CHAR(tty)) && L_CANON(tty)) {
                return (b - buf);
            }
            else {
                put_fs_byte(c,b++);
                if (!--nr)
                    break;
            }
        } while (nr>0 && !EMPTY(&tty->secondary));

        if (L_CANON(tty) && (b - buf)) {
            break;
        }
    }

    return (b - buf);
}

void copy_to_cooked(struct tty_struct * tty) {
    signed char c; 
    
    while (!EMPTY(&tty->read_q) && !FULL(tty->secondary)) {
        c = GETCH(&tty -> read_q);

        if (c == 13) {
            if (I_CRNL(tty))
                c = 10;
            else if (I_NOCR(tty))
                continue;
        }
        else if (c == 10 && I_NLCR(tty))
            c = 13;

        if (I_UCLC(tty))
            c = tolower(c);

        if (L_CANON(tty)) {
            if (c == KILL_CHAR(tty)) {
                while (!(EMPTY(&tty->secondary) ||
                            (c = LAST(tty->secondary)) == 10 ||
                            c == EOF_CHAR(tty))) {
                    if (L_ECHO(tty)) {
                        if (c < 32)
                            PUTCH(127, &tty->write_q);
                        PUTCH(127, &tty->write_q);
                        tty->write(tty);
                    }
                    DEC(tty->secondary.head);
                }
                continue;
            }
           
            if (c == ERASE_CHAR(tty)) {
                if (EMPTY(&tty->secondary) ||
                        (c = LAST(tty->secondary)) == 10 ||
                        c == EOF_CHAR(tty))
                    continue; 
                if (L_ECHO(tty)) {
                    if (c < 32)
                        PUTCH(127, &tty->write_q);
                    PUTCH(127, &tty->write_q);
                    tty->write(tty);
                }
                DEC(tty->secondary.head);
                continue;
            }
           
            if (c == STOP_CHAR(tty)) {
                tty->stopped = 1;
                continue;
            }
           
            if (c == START_CHAR(tty)) {
                tty->stopped = 0;
                continue;
            }
        }
       
        if (c == 10 || c == EOF_CHAR(tty))
            tty->secondary.data++;
       
        if (L_ECHO(tty)) {
            if (c == 10) {
                PUTCH(10, &tty->write_q);
                PUTCH(13, &tty->write_q);
            }
            else if (c < 32) {
                if (L_ECHOCTL(tty)) {
                    PUTCH('^', &tty->write_q);
                    PUTCH(c + 64, &tty->write_q);
                }
            }
            else
                PUTCH(c, &tty->write_q);
            tty->write(tty);
        }
        PUTCH(c, &tty->secondary);
    }
    wake_up(&tty->secondary.proc_list);
}

void do_tty_interrupt(int tty) {
    copy_to_cooked(tty_table + tty);
}

