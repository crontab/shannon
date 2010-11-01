#ifndef __COMMON_H
#define __COMMON_H

#if !defined(DEBUG) && !defined(NDEBUG)
#  define NDEBUG    // to suppress assert()
#endif

// All standard library headers should go only here
#include <sys/stat.h>
#include <stdint.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "version.h"


// SHN_64 can be enabled on 32-bit systems, and should be enabled on 64-bit 
// systems; it affects the size of the default int (defined as `integer' 
// below) and accordingly the size of the `variant' structure. In any case, 
// for various reasons the `integer' type should not be smaller than 
// sizeof(void*), otherwise initRuntime() will fail at startup.
#if defined(__x86_64__) || defined(_WIN64)
#  define SHN_64
#endif


// Generate faster but bigger code (more inlined functions)
// #define SHN_FASTER


#define SOURCE_EXT ".shn"


// --- BASIC DATA TYPES --------------------------------------------------- //


// Default fundamental types

#ifdef SHN_64
    typedef int64_t integer;
    typedef uint64_t uinteger;
    typedef double real;
#   define INTEGER_MIN INT64_MIN
#   define INTEGER_MAX INT64_MAX
#else
    typedef int32_t integer;
    typedef uint32_t uinteger;
    typedef float real;
#   define INTEGER_MIN INT32_MIN
#   define INTEGER_MAX INT32_MAX
#endif

// Equivalent of size_t, signed; used everywhere for container sizes/indexes
typedef ssize_t memint;
typedef size_t umemint;
typedef int16_t jumpoffs;
#define MEMINT_MAX LONG_MAX

// Convenient aliases
typedef unsigned char uchar;
typedef long long large;
typedef unsigned long long ularge;


// --- MISC --------------------------------------------------------------- //


void _fatal(int code, const char* msg);
void _fatal(int code);

#ifdef DEBUG
#  define fatal(code,msg)  _fatal(code, msg)
#else
#  define fatal(code,msg)  _fatal(code)
#endif

void notimpl();


template <class T>
    inline T imax(T x, T y)  { return (x > y) ? x : y; }

template <class T>
    inline T imin(T x, T y)  { return (x < y) ? x : y; }

template <class T>
    inline T exchange(T& target, const T& value)
        { T temp = target; target = value; return temp; }


template <class T, class X>
    inline T cast(const X& x)  
#ifdef DEBUG
        { return dynamic_cast<T>(x); }
#else
        { return (T)x; }
#endif


class noncopyable
{
private:
    noncopyable(const noncopyable&);
    const noncopyable& operator= (const noncopyable&);
public:
    noncopyable() {}
    ~noncopyable() {}
};


struct exception // : public noncopyable -- doesn't work
{
    exception() throw();
    virtual ~exception() throw();
    virtual const char* what() throw() = 0;
};


inline memint pstrlen(const char* s)
    { return s == NULL ? 0 : ::strlen(s); }

void outofmemory();

inline void* pmemcheck(void* p)
    { if (p == NULL) outofmemory(); return p; }

inline void* pmemalloc(memint s)
    { return pmemcheck(::malloc(s)); }

inline void* pmemcalloc(memint s)
    { return pmemcheck(::calloc(1, s)); }

inline void* pmemrealloc(void* p, memint s)
    { return pmemcheck(::realloc(p, s)); }

inline void pmemfree(void* p)
    { ::free(p); }


// Default placement versions of new and delete
inline void* operator new(size_t, void* p) throw() { return p; }
inline void  operator delete (void*, void*) throw() { }

// Disable all new/delete by default; redefine where necessary
void* operator new(size_t) throw();
void* operator new[](size_t) throw();
void  operator delete  (void*) throw();
void  operator delete[](void*) throw();


// --- ATOMIC OPERATIONS -------------------------------------------------- //


typedef int atomicint;

// TODO: the atomic functions below should be 64-bit on a 64-bit platform

#ifndef SHN_THR
    inline atomicint pincrement(atomicint* target)  { return ++(*target); }
    inline atomicint pdecrement(atomicint* target)  { return --(*target); }
#else
    atomicint pincrement(atomicint* target);
    atomicint pdecrement(atomicint* target);
#endif


#endif // __COMMON_H
