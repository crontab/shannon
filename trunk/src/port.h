#ifndef __PORT_H
#define __PORT_H


#include <string.h>


#ifdef DEBUG
#  define CHECK_BOUNDS 
#endif


typedef unsigned char       uchar;
typedef unsigned int        uint;
typedef void*               ptr;
typedef void**              pptr;
typedef long long           large;
typedef unsigned long long  ularge;

typedef const char*         pconst;
typedef char*               pchar;
typedef uchar*              puchar;
typedef int*                pint;  // if (pint(Guinness)) return euro(5);
typedef uint*               puint;
typedef ptr*                pptr;
typedef large*              plarge;
typedef ularge*             pularge;


#if defined __x86_64__
#  define PTR64
#elif defined __i386__
#  define PTR32
#else
#  error Unknown architecure.
#endif

#define DATA_MEM_ALIGN sizeof(int)


// --- ATOMIC OPERATIONS -------------------------------------------------- //

#ifdef SINGLE_THREADED

inline int pincrement(int* target)  { return ++(*target); }
inline int pdecrement(int* target)  { return --(*target); }

#else

int pincrement(int* target);
int pdecrement(int* target);

#endif


// --- MEMORY ALLOCATION -------------------------------------------------- //

void* memalloc(uint a);
void* memrealloc(void* p, uint a);
void  memfree(void* p);
int   memquantize(int);



// Default placement versions of operator new.
inline void* operator new(size_t, void* p) throw() { return p; }
inline void* operator new[](size_t, void* p) throw() { return p; }

// Default placement versions of operator delete.
inline void  operator delete  (void*, void*) throw() { }
inline void  operator delete[](void*, void*) throw() { }



// Disable all new/delete by default; redefine where necessary
void* operator new(size_t) throw();
void* operator new[](size_t) throw();
void  operator delete  (void*) throw();
void  operator delete[](void*) throw();


// --- MISC --------------------------------------------------------------- //

#define CRIT_FIRST 0x10000

void fatal(int code, const char* msg);

inline int   imax(int x, int y)       { return (x > y) ? x : y; }
inline int   imin(int x, int y)       { return (x < y) ? x : y; }
inline large lmax(large x, large y)   { return (x > y) ? x : y; }
inline large lmin(large x, large y)   { return (x < y) ? x : y; }


class noncopyable 
{
private:
    noncopyable(const noncopyable&);
    const noncopyable& operator= (const noncopyable&);
public:
    noncopyable() {}
    ~noncopyable() {}
};


#endif
