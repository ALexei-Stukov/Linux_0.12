#ifndef _STDARG_H
#define _STDARG_H

//这个头文件用于支持可变参数

//首先，可变参数列表 va_list 是一个 char*。
typedef char *va_list;

/* Amount of space required in an argument list for an arg of type TYPE.
 *    TYPE may alternatively be an expression whose type is used.  */
/*   这个宏的作用是计算给定类型的大小，并将其向上取整到最近的int边界。目的是为了对齐内存，确保在处理不同数据类型时不会出现偏移。
*    举个例子，假设TYPE的大小是3，int的大小是4，那么计算过程为：
*   (( 3 + 4 - 1) / 4) * 4 = 1 * 4 = 4
*/
#define __va_rounded_size(TYPE)  \
      (((sizeof (TYPE) + sizeof (int) - 1) / sizeof (int)) * sizeof (int))

//此处有一个条件编译指令，针对__sparc__这个宏进行判断。如果定义了这个宏，则使用不同的va_start实现。否则，采用默认的实现。
#ifndef __sparc__
#define va_start(AP, LASTARG)                       \
     (AP = ((char *) &(LASTARG) + __va_rounded_size (LASTARG)))
#else
#define va_start(AP, LASTARG)                       \
     (__builtin_saveregs (),                        \                 //__builtin_saveregs() 似乎是针对某些平台设计的函数，可能和保存寄存器状态有关。
        AP = ((char *) &(LASTARG) + __va_rounded_size (LASTARG)))
#endif

/*   首先解释一下  AP = ((char *) &(LASTARG) + __va_rounded_size (LASTARG)) 有何作用：
     AP 是一个指针，这行语句把 AP 设置为了 “参数列表中最后一个参数的地址” + “最后一个参数经过int取整之后的大小”。
     参数列表是一个字符串，可以认为，AP此时就在字符串最尾部。
     为什么要这么做呢？因为参数列表实际上是被反向遍历的。
     逐个访问参数列表的过程，实际上就是AP指针从数组尾部不断移动到数组头部的过程。
     下方的va_arg(AP, TYPE) 宏就给出了迭代访问参数列表的手法。多次使用 va_arg(AP, TYPE)，可以逐个返回参数列表中的所有参数。
*/


/*
     声明函数va_end(va_list),但是不实现。
     因为可变参数实际上是C语言提供的现成功能，只不过Linus又单独做了一些封装而已。对于va_end(va_list)函数，Linus希望使用gnulib库中现有的函数。
*/
void va_end (va_list);      /* Defined in gnulib */

/*
     强行用宏定义，把va_end(AP)定义为空。只要引入了这个宏，代码 va_end(xxx) 已经变得无效了。
     因为va_end操作实际上是C语言可变参数操作中的一个步骤，而在不同的平台上，C语言库关于可变参数的实现都各不相同。
     
     va_end()和堆栈有关。如果参数通过栈来传递，那么va_end()操作是不必要的。

     但是在部分平台上，参数不通过栈来传递，那么va_end()就是一个必不可少的操作了。
     显然，目前这份代码是针对“用栈传递参数”这一情况而设计的，所以va_end()函数被直接设置为空。
*/
#define va_end(AP)

#define va_arg(AP, TYPE)                        \
     (AP += __va_rounded_size (TYPE),                   \
        *((TYPE *) (AP - __va_rounded_size (TYPE))))

#endif /* _STDARG_H */