

#include <stdlib.h>
#include <stdio.h>

#include "common.h"


void fatal(int code, const char* msg) 
{
    fprintf(stderr, "\nInternal [%04x]: %s\n", code, msg);
    exit(code);
}


const int quant = 64;
const int quant2 = 4096;

int memquantize(int a)
{
    if (a <= 32)
        return 32;
    else if (a <= 1024)
        return (a + quant - 1) & ~(quant - 1);
    else
        return (a + quant2 - 1) & ~(quant2 - 1);
}



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

