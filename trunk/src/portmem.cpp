

#include <stdlib.h>
#include <stdio.h>

#include "port.h"


void fatal(int code, const char* msg) 
{
    fprintf(stderr, "\nInternal [%04x]: %s\n", code, msg);
    exit(code);
}



// Will be redefined later; currently the global new/delete are needed
// by the STL classes we use.

static void newdel()
{
    fatal(CRIT_FIRST + 4, "Global new/delete are disabled");
}

void* operator new(size_t) throw()       { newdel(); return NULL; }
void* operator new[](size_t) throw()     { newdel(); return NULL; }
void  operator delete  (void*) throw()   { newdel(); }
void  operator delete[](void*) throw()   { newdel(); }



// dynamic reallocation policy for strings and lists

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


static void memerror() 
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


void* memcalloc(uint a) 
{
    if (a == 0)
        return NULL;
    else
    {
        void* p = calloc(1, a);
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


