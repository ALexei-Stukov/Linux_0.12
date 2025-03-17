#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/kernel.h>

#include <asm/io.h>
#include <asm/system.h>

#define ORIG_X          (*(unsigned char *)0x90000)
#define ORIG_Y          (*(unsigned char *)0x90001)
#define ORIG_VIDEO_PAGE     (*(unsigned short *)0x90004)
#define ORIG_VIDEO_MODE     ((*(unsigned short *)0x90006) & 0xff)
#define ORIG_VIDEO_COLS     (((*(unsigned short *)0x90006) & 0xff00) >> 8)
#define ORIG_VIDEO_LINES    ((*(unsigned short *)0x9000e) & 0xff)
#define ORIG_VIDEO_EGA_AX   (*(unsigned short *)0x90008)
#define ORIG_VIDEO_EGA_BX   (*(unsigned short *)0x9000a)
#define ORIG_VIDEO_EGA_CX   (*(unsigned short *)0x9000c)

#define VIDEO_TYPE_MDA      0x10    /* Monochrome Text Display  */
#define VIDEO_TYPE_CGA      0x11    /* CGA Display          */
#define VIDEO_TYPE_EGAM     0x20    /* EGA/VGA in Monochrome Mode   */
#define VIDEO_TYPE_EGAC     0x21    /* EGA/VGA in Color Mode    */

#define NPAR 16

extern void keyboard_interrupt(void);

static unsigned char    video_type;     /* Type of display being used   */
static unsigned long    video_num_columns;  /* Number of text columns   */
static unsigned long    video_num_lines;    /* Number of test lines     */
static unsigned long    video_mem_base;     /* Base of video memory     */
static unsigned long    video_mem_term;     /* End of video memory      */
static unsigned long    video_size_row;     /* Bytes per row        */
static unsigned char    video_page;     /* Initial video page       */
static unsigned short   video_port_reg;     /* Video register select port   */
static unsigned short   video_port_val;     /* Video register value port    */
static unsigned short   video_erase_char;

static unsigned long    origin;
static unsigned long    scr_end;
static unsigned long    pos;
static unsigned long    x, y;
static unsigned long    top, bottom;
static unsigned long    state = 0;
static unsigned long    npar, par[NPAR];
static unsigned long    ques = 0;
static unsigned long    attr = 0x07;
static unsigned long    def_attr = 0x07;

#define RESPONSE "\033[?1;2c"

static inline void gotoxy(int new_x,unsigned int new_y) {
    if (new_x > video_num_columns || new_y >= video_num_lines)
        return;

    x = new_x;
    y = new_y;
    pos = origin + y*video_size_row + (x << 1);
}

static inline void set_origin() {
    cli();
    outb_p(12, video_port_reg);
    outb_p(0xff & ((origin - video_mem_base) >> 9), video_port_val);
    outb_p(13, video_port_reg);
    outb_p(0xff & ((origin - video_mem_base) >> 1), video_port_val);
    sti();
}

static inline void set_cursor() {
    cli();
    outb_p(14, video_port_reg);
    outb_p(0xff&((pos-video_mem_base)>>9), video_port_val);
    outb_p(15, video_port_reg);
    outb_p(0xff&((pos-video_mem_base)>>1), video_port_val);
    sti();
}

static void respond(struct tty_struct * tty) {
    char * p = RESPONSE;

    cli();
    while (*p) {
        PUTCH(*p, &tty->read_q);
        p++;
    }
    sti();
    copy_to_cooked(tty);
}

static void scrup() {
    if (!top && bottom == video_num_lines) {
        origin += video_size_row;
        pos += video_size_row;
        scr_end += video_size_row;

        if (scr_end > video_mem_term) {
            __asm__("cld\n\t"
                    "rep\n\t"
                    "movsl\n\t"
                    "movl video_num_columns,%1\n\t"
                    "rep\n\t"
                    "stosw"
                    ::"a" (video_erase_char),
                    "c" ((video_num_lines-1)*video_num_columns>>1),
                    "D" (video_mem_base),
                    "S" (origin):);
            scr_end -= origin-video_mem_base;
            pos -= origin-video_mem_base;
            origin = video_mem_base;
        }
        else {
             __asm__("cld\n\t"
                     "rep\n\t"
                     "stosw"
                     ::"a" (video_erase_char),
                     "c" (video_num_columns),
                     "D" (scr_end-video_size_row):);
        }
        set_origin();
    }
    else {
        __asm__("cld\n\t"
                "rep\n\t"
                "movsl\n\t"
                "movl video_num_columns,%%ecx\n\t"
                "rep\n\t"
                "stosw"
                ::"a" (video_erase_char),
                "c" ((bottom-top-1)*video_num_columns>>1),
                "D" (origin+video_size_row*top),
                "S" (origin+video_size_row*(top+1)):);
    }
}

static void scrdown() {
    if (bottom <= top)
        return;

    __asm__("std\n\t"
            "rep\n\t"
            "movsl\n\t"
            "addl $2,%%edi\n\t"
            "movl video_num_columns,%%ecx\n\t"
            "rep\n\t"
            "stosw"
            ::"a" (video_erase_char),
            "c" ((bottom-top-1)*video_num_columns>>1),
            "D" (origin+video_size_row*bottom-4),
            "S" (origin+video_size_row*(bottom-1)-4):);
}

static void ri() {
    if (y>top) {
        y--;
        pos -= video_size_row;
        return;
    }
    scrdown();
}

static void lf() {
    if (y + 1 < bottom) {
        y++;
        pos += video_size_row;
        return;
    }
    scrup();
}

static void cr() {
    pos -= x << 1;
    x = 0;
}

static void del() {
    if (x) {
        pos -= 2;
        x--;
        *(unsigned short*)pos = video_erase_char;
    }
}

static void csi_J(int vpar) {
    long count, start;

    switch (vpar) {
        case 0:
            count = (scr_end-pos)>>1;
            start = pos;
            break;
        case 1:
            count = (pos-origin)>>1;
            start = origin;
            break;
        case 2:
            count = video_num_columns * video_num_lines;
            start = origin;
            break;
        default:
            return;
    }

    __asm__("cld\n\t"
            "rep\n\t"
            "stosw\n\t"
            ::"c" (count),
            "D" (start),"a" (video_erase_char)
            :);
}

static void csi_K(int vpar) {
    long count, start;

    switch (vpar) {
        case 0:
            if (x>=video_num_columns)
                return;
            count = video_num_columns-x;
            start = pos;
            break;
        case 1:
            start = pos - (x<<1);
            count = (x<video_num_columns)?x:video_num_columns;
            break;
        case 2:
            start = pos - (x<<1);
            count = video_num_columns;
            break;
        default:
            return;
    }

    __asm__("cld\n\t"
            "rep\n\t"
            "stosw\n\t"
            ::"c" (count),
            "D" (start),"a" (video_erase_char)
            :);
}

void csi_m() {
    int i;
    for (i=0;i<=npar;i++) {
        switch (par[i]) {
            case 0: attr= 0x07; break;
            case 1: attr= 0x0f; break;
            case 4: attr = 0x0f; break;
			case 5: attr=attr|0x80;break;  /* blinking */
			case 7: attr=(attr<<4)|(attr>>4);break;  /* negative */
			case 22: attr=attr&0xf7;break; /* not bold */ 
			case 24: attr=attr&0xfe;break;  /* not underline */
			case 25: attr=attr&0x7f;break;  /* not blinking */
			case 27: attr=def_attr;break; /* positive image */
			case 39: attr=(attr & 0xf0)|(def_attr & 0x0f); break;
			case 49: attr=(attr & 0x0f)|(def_attr & 0xf0); break;
        default:
            if ((par[i]>=30) && (par[i]<=38))
                attr = (attr & 0xf0) | (par[i]-30);
            else if ((par[i]>=40) && (par[i]<=48))
                attr = (attr & 0x0f) | ((par[i]-40)<<4);
            else
                break;
        }
    }
}

static void delete_char() {
    int i;
    unsigned short * p = (unsigned short *) pos;
    if (x>=video_num_columns)
        return;
    i = x;
    while (++i < video_num_columns) {
        *p = *(p+1);
        p++;
    }
    *p = video_erase_char;
}

static void delete_line() {
    int oldtop,oldbottom;

    oldtop = top;
    oldbottom = bottom;
    top = y;
    bottom = video_num_lines;
    scrup();
    top = oldtop;
    bottom = oldbottom;
}

static void insert_char() {
    int i=x;
    unsigned short tmp, old = video_erase_char;
    unsigned short * p = (unsigned short *) pos;

    while (i++<video_num_columns) {
        tmp=*p;
        *p=old;
        old=tmp;
        p++;
    }
}

static void insert_line() {
    int oldtop,oldbottom;

    oldtop = top;
    oldbottom = bottom;
    top = y;
    bottom = video_num_lines;
    scrdown();
    top = oldtop;
    bottom = oldbottom;
}

static void csi_at(unsigned int nr) {
    if (nr > video_num_columns)
        nr = video_num_columns;
    else if (!nr)
        nr = 1;
    while (nr--)
        insert_char();
}

static void csi_L(unsigned int nr) {
    if (nr > video_num_lines)
        nr = video_num_lines;
    else if (!nr)
        nr = 1;
    while (nr--)
        insert_line();
}

static void csi_P(unsigned int nr) {
    if (nr > video_num_columns)
        nr = video_num_columns;
    else if (!nr)
        nr = 1;
    while (nr--)
        delete_char();
}

static void csi_M(unsigned int nr) {
    if (nr > video_num_lines)
        nr = video_num_lines;
    else if (!nr)
        nr=1;
    while (nr--)
        delete_line();
}

static int saved_x = 0;
static int saved_y = 0;

static void save_cur() {
    saved_x=x;
    saved_y=y;
}

static void restore_cur() {
    gotoxy(saved_x, saved_y);
}

void con_init() {
    register unsigned char a;

    char * display_desc = "????";
    char * display_ptr;

    video_num_columns = ORIG_VIDEO_COLS;
    video_size_row = video_num_columns * 2;
    video_num_lines = ORIG_VIDEO_LINES;
    video_page = ORIG_VIDEO_PAGE;
    video_erase_char = 0x0720;

    /* Is this a monochrome display? */
    if (ORIG_VIDEO_MODE == 7) {
        video_mem_base = 0xb0000;
        video_port_reg = 0x3b4;
        video_port_val = 0x3b5;
        if ((ORIG_VIDEO_EGA_BX & 0xff) != 0x10) {
            video_type = VIDEO_TYPE_EGAM;
            video_mem_term = 0xb8000;
            display_desc = "EGAm";
        }
        else {
            video_type = VIDEO_TYPE_MDA;
            video_mem_term = 0xb2000;
            display_desc = "*MDA";
        }
    }
    else { /* color display */
        video_mem_base = 0xb8000;
        video_port_reg  = 0x3d4;
        video_port_val  = 0x3d5;

        if ((ORIG_VIDEO_EGA_BX & 0xff) != 0x10) {
            video_type = VIDEO_TYPE_EGAC;
            video_mem_term = 0xc0000;
            display_desc = "EGAc";
        }
        else {
            video_type = VIDEO_TYPE_CGA;
            video_mem_term = 0xba000;
            display_desc = "*CGA";
        }
    }

    display_ptr = ((char *)video_mem_base) + video_size_row - 8;
    while (*display_desc) {
        *display_ptr++ = *display_desc++;
        display_ptr++;
    }

    origin = video_mem_base;
    scr_end = video_mem_base + video_num_lines * video_size_row;
    top = 0;
    bottom  = video_num_lines;

    gotoxy(ORIG_X, ORIG_Y);
    set_cursor();

    set_trap_gate(0x21,&keyboard_interrupt);    //设置键盘中断的处理函数

    outb_p(inb_p(0x21)&0xfd,0x21);  //与芯片交互，设置中断
    a=inb_p(0x61);      //读取键盘状态
    outb_p(a|0x80,0x61);    //关闭键盘
    outb_p(a,0x61);         //打开键盘
                            //通过一连串的手法，重置了键盘状态。键盘的指示灯会闪烁。
}

enum { ESnormal, ESesc, ESsquare, ESgetpars, ESgotpars, ESfunckey,
    ESsetterm, ESsetgraph };

void con_write(struct tty_struct* tty) {
    int nr;
    char c;

    nr = CHARS(&tty->write_q);

    while(nr--) {
        c = GETCH(&tty->write_q);
        switch (state) {
            case 0:
            if (c > 31 && c < 127) {
                if (x >= video_num_columns) {
                    x -= video_num_columns;
                    pos -= video_size_row;
                    lf();
                }

                *(char *)pos = c;
                *(((char*)pos) + 1) = attr;
                pos += 2;
                x++;
            }
            else if (c == 27) {
                state = ESesc;
            }
            else if (c == 10 || c == 11 || c == 12)
                lf();
            else if (c == 13)
                cr();
            else if (c == 127) {
                del();
            }
            else if (c == 8) {
                if (x) {
                    x--;
                    pos -= 2;
                }
            }
            else if (c == 9) {
                c=8-(x&7);
                x += c;
                pos += c<<1;
                if (x > video_num_columns) {
                    x -= video_num_columns;
                    pos -= video_size_row;
                    lf();
                }
                c = 9;
            }
            break;
        case ESesc:
            state = ESnormal;
            switch (c) {
                case '[':
                    state = ESsquare;
                    break;
                case 'E':
                    gotoxy(0, y+1);
                    break;
                case 'M':
                    ri();
                    break;
                case 'D':
                    lf();
                    break;
                case 'Z':
                    respond(tty);
                    break;
                case '7':
                    save_cur();
                    break;
                case '8':
                    restore_cur();
                    break;
                case '(':  case ')':
                    state = ESsetgraph;
                    break;
                case 'P':
                    state = ESsetterm;
                    break;
                case '#':
                    state = -1;
                    break;
                case 'c':
                    top = 0;
                    bottom = video_num_lines;
                    break;
            }
            break;
        case ESsquare:
            for(npar=0;npar<NPAR;npar++)
                par[npar]=0;
            npar=0;
            state=ESgetpars;
            if (c =='[') {
                state=ESfunckey;
                break;
            }

            if ((ques=(c=='?')))
                break;
        case ESgetpars:
            if (c==';' && npar<NPAR-1) {
                npar++;
                break;
            }
            else if (c>='0' && c<='9') {
                par[npar]=10*par[npar]+c-'0';
                break;
            }
            else
                state=ESgotpars;
        case ESgotpars:
            state = ESnormal;
            if (ques) {
                ques = 0;
                break;
            }
            switch(c) {
                case 'G': case '`':
                    if (par[0]) par[0]--;
                    gotoxy(par[0], y);
                    break;
                case 'A':
                    if (!par[0]) par[0]++;
                    gotoxy(x, y - par[0]);
                    break;
                case 'B': case 'e':
                    if (!par[0]) par[0]++;
                    gotoxy(x, y + par[0]);
                    break;
                case 'C': case 'a':
                    if (!par[0]) par[0]++;
                    gotoxy(x + par[0], y);
                    break;
                case 'D':
                    if (!par[0]) par[0]++;
                    gotoxy(x - par[0], y);
                    break;
                case 'E':
                    if (!par[0]) par[0]++;
                    gotoxy(0, y + par[0]);
                    break;
                case 'F':
                    if (!par[0]) par[0]++;
                    gotoxy(0, y - par[0]);
                    break;
                case 'd':
                    if (!par[0]) par[0]--;
                    gotoxy(x, par[0]);
                    break;
                case 'H': case 'f':
                    if (par[0]) par[0]--;
                    if (par[1]) par[1]--;
                    gotoxy(par[1],par[0]);
                    break;
                case 'J':
                    csi_J(par[0]);
                    break;
                case 'K':
                    csi_K(par[0]);
                    break;
                case 'L':
                    csi_L(par[0]);
                    break;
                case 'M':
                    csi_M(par[0]);
                    break;
                case 'P':
                    csi_P(par[0]);
                    break;
                case '@':
                    csi_at(par[0]);
                    break;
                case 'm':
                    csi_m(par[0]);
                    break;
                case 'r':
                    if (par[0]) par[0]--;
                    if (!par[1]) par[1] = video_num_lines;
                    if (par[0] < par[1] &&
                            par[1] <= video_num_lines) {
                        top=par[0];
                        bottom=par[1];
                    }
                    break;
                case 's':
                    save_cur();
                    break;
                case 'u':
                    restore_cur();
                    break;
            }
            break;
        case ESsetterm:
            state = ESnormal;
            if (c == 'S') {
			    def_attr = attr;
			    video_erase_char = (video_erase_char&0x0ff) | (def_attr<<8);
            } else if (c == 'L')
			    ; /*linewrap on*/
            else if (c == 'l')
			    ; /*linewrap off*/
            break;
        }
    }

    gotoxy(x, y);
    set_cursor();
}

void con_print(const char* buf, int nr) {
    const char* s = buf;

    while(nr--) {
        char c = *s++;
        if (c > 31 && c < 127) {
            if (x >= video_num_columns) {
                x -= video_num_columns;
                pos -= video_size_row;
                lf();
            }

            *(char *)pos = c;
            *(((char*)pos) + 1) = attr;
            pos += 2;
            x++;
        }
        else if (c == 10 || c == 11 || c == 12)
            lf();
        else if (c == 13)
            cr();
        else if (c == 127) {
            del();
        }
        else if (c == 8) {
            if (x) {
                x--;
                pos -= 2;
            }
        }
    }

    gotoxy(x, y);
    set_cursor();
}

