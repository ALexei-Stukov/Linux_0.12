#ifndef _STDDEF_H
#define _STDDEF_H

#ifndef _PTRDIFF_T
#define _PTRDIFF_T
typedef long ptrdiff_t;
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned long size_t;
#endif

#undef NULL
#define NULL ((void *)0)

// 这是一个计算成员偏移量的宏
// 简单来说:
// 如果有一个结构体，名为pointer，它是TYPE类型的指针。它里面有一个成员，名为MEMBER。
// 那么，通过 &(pointer->MEMBER) 即可获得MEMBER的地址。因为结合性的优先级，写成 &pointer->MEMBER 也没问题。

// 如果 pointer 指向内存地址 A，那么成员MEMBER的所在位置 = A + MEMBGER在结构体内的偏移量。
// 可以把这个结果简称为 基准地址 + 偏移量。

// 现在虚构一个指针，它指向 0 这个地址，它的类型是TYPE*。也就是(TYPE *)0。
// 通过这一番操作，我们虚构了一个 TYPE对象。
// 此时我们获取这个虚构对象的虚构成员 ((TYPE *)0)->MEMBER ，并且计算这个虚构成员的地址。 &(((TYPE *)0)->MEMBER)。
// 我们的计算结果是 0 + 偏移量，这么一来，MEMBER成员的偏移量结果就被我们计算出来了。
// linus 这里让返回类型是 size_t
#define offsetof(TYPE, MEMBER) ((size_t) &(((TYPE *)0)->MEMBER))
#endif

