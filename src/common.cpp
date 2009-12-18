

#include "common.h"


void _fatal(int code, const char* msg) 
{
#ifdef DEBUG
    // We want to see the stack backtrace in XCode debugger
    assert(code == 0);
#else
    fprintf(stderr, "\nInternal 0x%04x: %s\n", code, msg);
#endif
    exit(100);
}


void _fatal(int code) 
{
#ifdef DEBUG
    assert(code == 0);
#else
    fprintf(stderr, "\nInternal error [%04x]\n", code);
#endif
    exit(100);
}


void notimpl()
{
    fatal(0x0001, "Feature not implemented yet");
}


exception::exception() throw()  { }
exception::~exception() throw()  { }

void outofmemory()
{
    fatal(0x1001, "Out of memory");
}

static void newdel()
{
    fatal(0x0002, "Global new/delete are disabled");
}

void* operator new(size_t) throw()       { newdel(); return NULL; }
void* operator new[](size_t) throw()     { newdel(); return NULL; }
void  operator delete  (void*) throw()   { newdel(); }
void  operator delete[](void*) throw()   { newdel(); }


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


