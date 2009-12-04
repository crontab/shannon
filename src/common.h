#ifndef __COMMON_H
#define __COMMON_H

#if !defined(DEBUG) && !defined(NDEBUG)
#  define NDEBUG    // to suppress assert()
#endif

#include <assert.h>
#include <limits.h>
#include <string.h>


#if !defined(SINGLE_THREADED) && !defined(MULTI_THREADED)
#  define SINGLE_THREADED
#endif

#if (defined(DEBUG) || defined(_DEBUG)) && !defined(RANGE_CHECKING)
#  define RANGE_CHECKING
#endif


#if defined(__x86_64__)
#  define PTR64
#elif defined(__i386__)
#  define PTR32
#else
#  error Unknown architecure.
#endif

// On a Mac:
// short: 2  long: 4  long long: 8  int: 4  void*: 4  float: 4  double: 8
// integer: 4  mem: 4  real: 4  variant: 8

// On 64-bit Linux:
// short: 2  long: 8  long long: 8  int: 4  void*: 8  float: 4  double: 8
// integer: 8  mem: 8  real: 8  variant: 16  object: 16/24

// SH64 can be enabled both on 64 and 32-bit systems
#ifdef PTR64
#  define SH64
#endif


// --- BASIC DATA TYPES --------------------------------------------------- //


// Default fundamental types

#ifdef SH64
    typedef long long integer;
    typedef unsigned long long uinteger;
    typedef double real;
#   define INTEGER_MIN LLONG_MIN
#   define INTEGER_MAX LLONG_MAX
#else
    typedef int integer;
    typedef unsigned int uinteger;
    typedef float real;
#   define INTEGER_MIN INT_MIN
#   define INTEGER_MAX INT_MAX
#endif


typedef long memint;
#define MEMINT_MAX LONG_MAX
#define ALLOC_MAX (MEMINT_MAX-255)

typedef unsigned char uchar;
typedef char* pchar;
typedef uchar* puchar;
typedef const char* pconst;
typedef const uchar* puconst;


// --- MISC --------------------------------------------------------------- //


void _fatal(int code, const char* msg);
void _fatal(int code);

#ifdef DEBUG
#  define fatal(code,msg)  _fatal(code, msg)
#else
#  define fatal(code,msg)  _fatal(code)
#endif

void notimpl();


template<class T>
    inline T imax(T x, T y)  { return (x > y) ? x : y; }

template<class T>
    inline T imin(T x, T y)  { return (x < y) ? x : y; }

template <class T>
    inline T exchange(T& target, const T& value)
        { T temp = target; target = value; return temp; }


template <class T, class X>
    T cast(X x)
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


class exception: public noncopyable
{
public:
    exception();
    virtual ~exception();
    virtual const char* what() const = 0;
};


#define DEF_EXCEPTION(name,msg) \
    struct name: public exception \
        { virtual const char* what() const throw() { return msg; } };


inline memint pstrlen(const char* s)
    { return s == NULL ? 0 : ::strlen(s); }


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


// --- ATOMIC OPERATIONS -------------------------------------------------- //


#ifdef SINGLE_THREADED

inline int pincrement(int* target)  { return ++(*target); }
inline int pdecrement(int* target)  { return --(*target); }

#else

int pincrement(int* target);
int pdecrement(int* target);

#endif



#endif // __COMMON_H
