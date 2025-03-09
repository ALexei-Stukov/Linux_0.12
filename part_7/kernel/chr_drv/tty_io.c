#include <linux/tty.h>

void tty_init() {
    con_init();
}

void tty_write(unsigned channel, char* buf, int nr) {
    console_print(buf, nr);
}

