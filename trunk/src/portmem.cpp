

#include <stdlib.h>
#include <stdio.h>


#include "port.h"


void fatal(int code, const char* msg) 
{
    fprintf(stderr, "\nInternal [%04x]: %s\n", code, msg);
    exit(code);
}


// dynamic reallocation policy for strings and lists

const int quant = 64;
const int qmask = ~63;
const int quant2 = 4096;
const int qmask2 = ~4095;

int memquantize(int a)
{
    if (a <= 16)
        return 16;
    if (a <= 32)
        return 32;
    else if (a <= 2048)
        return (a + quant - 1) & qmask;
    else
        return (a + quant2 - 1) & qmask2;
}


void memerror() 
{
    fatal(CRIT_FIRST + 5, "Not enough memory");
}


void* memalloc(uint a) 
{
    if (a == 0)
        return NULL;
    else
    {
        void* p = malloc(a);
        if (p == NULL) 
            memerror();
        return p;
    }
}


void* memrealloc(void* p, uint a) 
{
    if (a == 0)
    {
        memfree(p);
        return NULL;
    }
    else if (p == NULL)
        return memalloc(a);
    else
    {
        p = realloc(p, a);
        if (p == NULL) 
            memerror();
        return p;
    }
}


void memfree(void* p) 
{
    if (p != NULL)
        free(p);
}


