
#include "port.h"


#ifdef SINGLE_THREADED
// single-threaded version


int pexchange(int* target, int value)
{
    int r = *target;
    *target = value;
    return r;
}


void* pexchange(void** target, void* value)
{
    void* r = *target;
    *target = value;
    return r;
}


int pincrement(int* target)
{
    return ++(*target);
}


int pdecrement(int* target)
{
    return --(*target);
}


#elif defined(__GNUC__) && (defined(__i386__) || defined(__I386__))
// multi-threaded version with GCC on i386


int pexchange(int* target, int value)
{
    __asm__ __volatile ("lock ; xchgl (%1),%0" : "+r" (value) : "r" (target));
    return value;
}


void* pexchange(void** target, void* value)
{
    __asm__ __volatile ("lock ; xchgl (%1),%0" : "+r" (value) : "r" (target));
    return value;
}


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


#endif

