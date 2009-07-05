
#include "common.h"


#ifndef SINGLE_THREADED

#if defined(__GNUC__) && (defined(__i386__) || defined(__I386__)|| defined(__x86_64__))
// multi-threaded version with GCC on i386


int pincrement(int* target)
{
    int temp = 1;
    __asm__ __volatile ("lock ; xaddl %0,(%1)" : "+r" (temp) : "r" (target));
    return temp + 1;
}


int pdecrement(int* target)
{
    int temp = -1;
    __asm__ __volatile ("lock ; xaddl %0,(%1)" : "+r" (temp) : "r" (target));
    return temp - 1;
}


#else

#error Undefined architecture: atomic functions are not available

#endif

#endif

