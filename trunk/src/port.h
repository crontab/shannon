#ifndef __PORT_H
#define __PORT_H


#ifdef DEBUG
#  define CHECK_BOUNDS 
#endif


typedef unsigned char       uchar;
typedef unsigned int        uint;
typedef void*               ptr;
typedef long long           large;
typedef unsigned long long  ularge;
typedef const char*         pconst;
typedef char*               pchar;

#define LARGE_MIN (-9223372036854775807ll-1)
#define LARGE_MAX (9223372036854775807ll)
#define ULARGE_MAX (18446744073709551615ull)


union quant
{
    void* ptr;
    int   ord;
};


// --- ATOMIC OPERATIONS -------------------------------------------------- //

#ifdef SINGLE_THREADED

int pincrement(int* target)  { return ++(*target); }
int pdecrement(int* target)  { return --(*target); }

#else

int pincrement(int* target);
int pdecrement(int* target);

#endif


// --- MEMORY ALLOCATION -------------------------------------------------- //

void* memalloc(uint a);
void* memrealloc(void* p, uint a);
void  memfree(void* p);
void  memerror();
int   memquantize(int);


#define CRIT_FIRST 0xC0000

void fatal(int code, const char* msg);

inline int   imax(int x, int y)       { return (x > y) ? x : y; }
inline int   imin(int x, int y)       { return (x < y) ? x : y; }
inline large lmax(large x, large y)   { return (x > y) ? x : y; }
inline large lmin(large x, large y)   { return (x < y) ? x : y; }

#endif
