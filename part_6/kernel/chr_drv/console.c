#include <linux/tty.h>

#include <asm/io.h>
#include <asm/system.h>

//这些是屏幕的信息，在boot期间就存储在了对应的位置上，现在取出来开始使用。
#define ORIG_X          (*(unsigned char *)0x90000)
#define ORIG_Y          (*(unsigned char *)0x90001)
#define ORIG_VIDEO_PAGE     (*(unsigned short *)0x90004)
#define ORIG_VIDEO_MODE     ((*(unsigned short *)0x90006) & 0xff)
#define ORIG_VIDEO_COLS     (((*(unsigned short *)0x90006) & 0xff00) >> 8)
#define ORIG_VIDEO_LINES    ((*(unsigned short *)0x9000e) & 0xff)
#define ORIG_VIDEO_EGA_AX   (*(unsigned short *)0x90008)
#define ORIG_VIDEO_EGA_BX   (*(unsigned short *)0x9000a)
#define ORIG_VIDEO_EGA_CX   (*(unsigned short *)0x9000c)

//不同类型的显示器，对应着不同的字符输出手法。
#define VIDEO_TYPE_MDA      0x10    /* Monochrome Text Display  */
#define VIDEO_TYPE_CGA      0x11    /* CGA Display          */
#define VIDEO_TYPE_EGAM     0x20    /* EGA/VGA in Monochrome Mode   */
#define VIDEO_TYPE_EGAC     0x21    /* EGA/VGA in Color Mode    */

static unsigned char    video_type;     /* Type of display being used   */  //显示器类型
static unsigned long    video_num_columns;  /* Number of text columns   */  //屏幕上有多少列
static unsigned long    video_num_lines;    /* Number of test lines     */  //屏幕上有多少行
static unsigned long    video_mem_base;     /* Base of video memory     */  //显存的起点地址
static unsigned long    video_mem_term;     /* End of video memory      */  //显存的终点地址

static unsigned long    video_size_row;     /* Bytes per row        */      //一行如果写满能占据多少Byte数据。
                                                                            //需要注意，video_num_columns 指的是一行的字符数量，而video_size_row是一行的字节数量。这两者是有区别的。
                                                                            //因为终端的字符有时候是有颜色的，此时一个字符还伴随着一个控制字符。
                                                                            //所以严格来说，video_size_row 是 大于等于 video_num_columns的。
                                                                            //代码中也可以看到，video_size_row = video_num_columns * 2;

static unsigned char    video_page;     /* Initial video page       */      //显存页面
                                                                            //对于早期的显示器来说，显存会被分为多个页面，每个页面可以保存不同的内容。只需切换页面，就可以实现切屏的效果。
                                                                            //显存页面也可以用于缓存，我们可以在后台页面准备数据，当时机成熟，使用页面切换来展示后台数据。
                                                                            //因为代码还不完善，所以这个变量暂时是无效的。

static unsigned short   video_port_reg;     /* Video register select port   */  //寄存器端口。
static unsigned short   video_port_val;     /* Video register value port    */  //数据端口。
                                                                                //这两个寄存器和显示器控制有关。
                                                                                //对于不同类型的显示器，寄存器端口和数据端口往往是不同的。

static unsigned short   video_erase_char;                                       //如果需要删除一个字符，该用什么字符来覆盖。删除字符本质上就是覆盖新字符。

static unsigned long    origin;         // 显示器当前内容的显存起始位置，简称为屏幕显存起始位置
static unsigned long    scr_end;        // 显示器当前内容的显存结束位置，简称为屏幕显存结束位置
                                        // 可以认为，[origin,scrend] 组成了一个滑动窗口（sliding window），它可以在显存范围 [video_mem_base,video_mem_term] 之内任意滑动。这就是屏幕滚动的原理。

static unsigned long    pos;            // 当前光标位置相对于显存起始位置video_mem_base的偏移量
static unsigned long    x, y;           // 光标在屏幕上的坐标位置（x为列，y为行）
                                        // pos 和 x,y 是光标在两个领域内的表现。
                                        // 无论光标如何移动，光标肯定在屏幕之内，所以需要一组坐标变量来描述光标在屏幕中的位置，也就是x,y
                                        // 但是光标还指向了显存数据，我们需要把光标和显存内容对应起来，因此就得到了pos。

static unsigned long    top, bottom;    // 滚动显示区域的上下界限，即当前的顶部行和底部行分别对应第几行。这两个值不应该超过[0,video_num_lines)的范围。

static unsigned long    attr = 0x07;    // 字符显示属性-黑底白字
                                        //attr = 0x07 的二进制表示：  0000 0111
                                        //高4位（背景色）：0000     低4位（前景色）：0111

/*
    设置光标在屏幕中和显存中的位置

    根据坐标，计算出下一个字符应该被输出在什么位置，然后把光标设置到这个位置。

    需要判断新位置是否超出了屏幕显示范围。
    如果超出范围，那就不更新坐标。
    如果没有超出范围，那么就修改新的字符位置，并且重新计算光标相对于显存的偏移量。
*/
static inline void gotoxy(int new_x,unsigned int new_y) {    
    if (new_x > video_num_columns || new_y >= video_num_lines)
        return;

    x = new_x;
    y = new_y;
    pos = origin + y*video_size_row + (x << 1); //(x << 1) 是 x*2。
}

/*
    在给定新的显存起始位置的情况下，让屏幕显示范围对应显存起始位置。
    origin - video_mem_base 是 滑动窗口[origin,scrend] 相对于显存起点的偏移。
    我们只需要把这个偏 移量的1个字节写入12端口，另一个字节写入13端口，即可让显示器内容与滑动窗口同步。
    当然，>>9 和 >>1 看起来很诡异，但这没办法，这就是早期显示器的标准。
*/
static inline void set_origin() {
    cli();
    outb_p(12, video_port_reg);
    outb_p(0xff & ((origin - video_mem_base) >> 9), video_port_val);
    outb_p(13, video_port_reg);
    outb_p(0xff & ((origin - video_mem_base) >> 1), video_port_val);
    sti();
}

/*
    在显示器上更新光标位置
    屏蔽中断后使用outb_p写入数值，然后恢复中断。
*/
static inline void set_cursor() {
    cli();
    outb_p(14, video_port_reg);
    outb_p(0xff&((pos-video_mem_base)>>9), video_port_val);
    outb_p(15, video_port_reg);
    outb_p(0xff&((pos-video_mem_base)>>1), video_port_val);
    sti();
}

/*
    这个函数专门用于应对向上滚屏。(scroll up)
    
    屏幕内容向上滚动一行。第一行会被隐去，最后一行会出现新的内容。
*/
static void scrup() {
    /*
        滚屏分为两种情况，如果屏幕几乎写满了，从第一行到最后一行都有数据，即 top == 0 且 bottom == video_num_lines
        此时可以认为，这是滑动窗口在向显存末尾移动。
    */
    if (!top && bottom == video_num_lines) {
        //尝试向显存末尾整体移动滑动窗口，移动距离为1行。
        origin += video_size_row;   //新的屏幕显存起点
        pos += video_size_row;      //新的光标偏移地址
        scr_end += video_size_row;  //新的屏幕显存终点

        /*
            先处理简单情况。
            如果滑动窗口没有超出显存范围，那就好处理得多。
            先用空白符把最后一行覆盖，然后调用set_origin()重新设置显存范围。
            这样一来，就会出现这样的效果：
                最后一行先被清空，然后整个屏幕更新数据。
            从视觉上来说，这就实现了滑动窗口向下滚动的效果。
        */
        if (scr_end <= video_mem_term) { 
            __asm__("cld\n\t"
                "rep\n\t"
                "stosw"
                ::"a" (video_erase_char),
                "c" (video_num_columns),
                "D" (scr_end-video_size_row):);
        }
        else {
            /*
                然后开始处理复杂情况。滑动窗口移动到显存末尾，有超出显存范围的可能。这个时候必须要想一个解决办法。
                Linus当时设计了一个“地址回卷”的方案。他决定先把滑动窗口移动到显存起始位置（video_mem_base），然后再执行窗口向下滚动。
                这是一个非常反直觉的做法，但是实现了显存的循环利用，避免超出物理显存范围导致的显示错误。

                接下来就是终点了，如果直接把滑动窗口设置到初始位置，然后执行set_origin()更新屏幕内容，会造成严重的割裂感。
                为了不产生割裂感，首先要将当前窗口的内容（从origin开始）复制到显存起始位置（video_mem_base），覆盖旧的显存数据。

                因为滑动窗口已经越界，所以最下方新添加的行是无效的，我们应当把新的一行手动设置为空行。
                最后经过一些滑动窗口的相关设置后，调用set_origin()刷新显示器即可。
            */
            __asm__("cld\n\t"                       //设置地址递增方向，为后续所有rep做好准备

                    "rep\n\t"                       //重复次数是 (video_num_lines-1)*video_num_columns >>1。
                                                    //因为此时窗口已经越界，所以窗口中只有 video_num_lines-1 行有效数据。我们复制这部分就可以了。
                    "movsl\n\t"                     //源头(source)是origin,目标(desc)是video_mem_base
                                                    
                                                    //既然发生了越界访问，那么获得的新行是无效数据，应当为空白。
                                                    //所以，我们还需要在显存起始位置附近补充一行空白字符。
                                                    //无需重新设置写入地址，因为movsl指令会自动推进写入地址。
                    "movl video_num_columns,%1\n\t" //ECX(循环次数)寄存器的值 = 屏幕字符数量
                    "rep\n\t"                       //
                    "stosw"                         //stosw指令是把EAX的值写入到指定位置，此时的eax是覆盖用的空字符。通过这种手法，我们又写入了一行空字符。
                    ::"a" (video_erase_char),
                    "c" ((video_num_lines-1)*video_num_columns>>1),
                    "D" (video_mem_base),
                    "S" (origin):);

            /*
                最后，手动设置滑动窗口的位置。注意，scr_end pos 相对于 origin 的偏移量不能改变。因为我们是在做滑动窗口“整体移动”。
                滑动窗口移动的距离是 origin-video_mem_base，所以要让 scr_end pos 都减去 origin-video_mem_base
            */
            scr_end -= origin-video_mem_base;
            pos -= origin-video_mem_base;
            origin = video_mem_base;
        }
        set_origin();
    }
    else {
        /*
            如果是不是整页滚屏，而是局部向上滚屏，那么处理方法是不同的。
            首先，因为只是局部变化，所以滑动窗口固定。此时显示器对应着滑动窗口中的内容，我们直接修改滑动窗口内的显存数据，屏幕内容就会马上更新。
            
            查阅资料，“局部向上滚屏”可能被用于如下的范围：
            这种技术常用于：
                实时监控界面
                游戏界面
                文本编辑器
                终端多窗口应用
            
            但是当前的代码还是有BUG的，因为局部向上滚屏之后，数据会直接消失（被覆盖）。导致当前的代码根本就不实用。
            后续必须得更新才行。
        */
        __asm__("cld\n\t"                               //设置地址递增方向，为后续所有rep做好准备
                "rep\n\t"                               //重复次数是 (bottom-top-1)*video_num_columns>>1
                "movsl\n\t"                             //让top到bottom范围内的数据整体上移一行。

                                                        //原本的bottom行用空白行填充
                "movl video_num_columns,%%ecx\n\t"      //再次设置循环次数为video_num_columns，
                "rep\n\t"                               // 
                "stosw"                                 //stosw指令是把EAX的值写入到指定位置，此时的eax是覆盖用的空字符。通过这种手法，我们又写入了一行空字符。
                ::"a" (video_erase_char),
                "c" ((bottom-top-1)*video_num_columns>>1),
                "D" (origin+video_size_row*top),
                "S" (origin+video_size_row*(top+1)):);
    }
}


/*
    处理换行符LF。
    需要预先判断换行是否会导致滑动窗口移动。
*/
static void lf() {
    // y+1 < bottom 的场合，换行不会导致滑动窗口移动
    if (y + 1 < bottom) {
        y++;
        pos += video_size_row;
        return;
    }

    // y+1 >= bottom 的场合，换行会导致滑动窗口移动。
    // scrup函数默认滑动1行。
    scrup();
}

/*
    处理回车符CR，把光标退回到当前行的起点处。
*/
static void cr() {
    pos -= x << 1;  // 将光标位置回退到当前行的开始处
    x = 0;      // 将列坐标设置为0
}

/*
    处理删除符DEL，删除屏幕上的一个字符。
*/
static void del() {
    //只有光标的列不为0时，才可以删除。
    if (x) {
        pos -= 2;   //让pos指向上一个字符（在显存中）
        x--;    //移动光标的列坐标
        *(unsigned short*)pos = video_erase_char;   //覆盖这个字符
    }
}

/*
    设置一些控制台基础属性。
*/
void con_init() {
    char * display_desc = "????";
    char * display_ptr;

    video_num_columns = ORIG_VIDEO_COLS;        //显示器最大列数
    video_size_row = video_num_columns * 2;     //一行占据的Byte数，包括控制字符
    video_num_lines = ORIG_VIDEO_LINES;         //显示器最大行数
    video_page = ORIG_VIDEO_PAGE;               //暂时无效
    video_erase_char = 0x0720;                  //用于覆盖的字符

    //根据不同显示器类型，设置相关变量。
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

    //这个循环没啥用，只是往显存中随意填充了一些字符而已。
    display_ptr = ((char *)video_mem_base) + video_size_row - 8;
    while (*display_desc) {
        *display_ptr++ = *display_desc++;
        display_ptr++;
    }

    
    origin = video_mem_base;                                            //设置滑动窗口起点
    scr_end = video_mem_base + video_num_lines * video_size_row;        //设置滑动窗口终点
    top = 0;                                                            //初始化时，屏幕的top行是第0行
    bottom  = video_num_lines;                                          //初始化时，屏幕的bottom行是最下方一行。

    gotoxy(ORIG_X, ORIG_Y);                                             //设置光标在屏幕中和显存中的位置
    set_cursor();                                                       //在显示器上更新光标位置
}

void console_print(const char* buf, int nr) {
    const char* s = buf;
    //逐个处理buf中的字符
    while(nr--) {
        char c = *s++;
        //可见字符
        if (c > 31 && c < 127) {
            //如果当前光标的坐标已经超出范围，则先执行1次换行，再打印可见字符。
            if (x >= video_num_columns) {
                x -= video_num_columns;
                pos -= video_size_row;
                lf();
            }

            *(char *)pos = c;
            *(((char*)pos) + 1) = attr; //控制字符统一设置为 attr = 0x07
            pos += 2;
            x++;
        }
        //换行符
        else if (c == 10 || c == 11 || c == 12)
            lf();
        //回车符
        else if (c == 13)
            cr();
        //删除符
        else if (c == 127) {
            del();
        }
        //退格符（BS），注意，退格本身不具备覆盖符号的功能。
        //因为时代原因，DEL和BS这两个键如今的效果已经发生了变化。如今的DEL是向后删除，BS是向前删除。
        else if (c == 8) {
            if (x) {
                x--;
                pos -= 2;
            }
        }
    }

    //每次处理完字符都重置并刷新光标位置。
    gotoxy(x, y);
    set_cursor();
}

