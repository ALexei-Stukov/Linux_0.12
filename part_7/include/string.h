#ifndef _STRING_H_
#define _STRING_H_

#ifndef NULL
#define NULL ((void *) 0)
#endif

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif

extern char * strerror(int errno);
// 这是Linus写的函数头，封装了字符串处理函数。
// 函数实现在 ../lib/string.c 内
/*
 * This string-include defines all string functions as inline
 * functions. Use gcc. It also assumes ds=es=data space, this should be
 * normal. Most of the string-functions are rather heavily hand-optimized,
 * see especially strtok,strstr,str[c]spn. They should work, but are not
 * very easy to understand. Everything is done entirely within the register
 * set, making the functions fast and clean. String instructions have been
 * used through-out, making for "slightly" unclear code :-)
 *
 *      (C) 1991 Linus Torvalds
 */

//此处原有的extern和inline标记其实是多余的，只要实现时有inline就可以了，所以把原有的extern和inline全都去掉。
char * strcpy(char * dest,const char *src);

char * strncpy(char * dest,const char *src,int count);

char * strcat(char * dest,const char * src);

char * strncat(char * dest,const char * src,int count);

int strcmp(const char * cs,const char * ct);

int strncmp(const char * cs,const char * ct,int count);

char * strchr(const char * s,char c);

char * strrchr(const char * s,char c);

int strspn(const char * cs, const char * ct);

int strcspn(const char * cs, const char * ct);

char * strpbrk(const char * cs,const char * ct);

char * strstr(const char * cs,const char * ct);

int strlen(const char * s);

extern char * ___strtok;

char * strtok(char * s,const char * ct);

void * memcpy(void * dest,const void * src, int n);

void * memmove(void * dest,const void * src, int n);

int memcmp(const void * cs,const void * ct,int count);

void * memchr(const void * cs,char c,int count);

void * memset(void * s,char c,int count);

#endif

