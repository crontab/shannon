#ifndef __COMMON_H
#define __COMMON_H

#if !defined(DEBUG) && !defined(NDEBUG)
#  define NDEBUG    // to suppress assert()
#endif

#include <assert.h>
#include <limits.h>

#include <string>
#include <exception>

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


typedef size_t mem;
typedef int16_t joffs_t;    // jump offset in code
typedef std::string str;

typedef unsigned char uchar;


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

#ifdef DEBUG
#  define CAST(t,x) (dynamic_cast<t>(x))
#else
#  define CAST(t,x) ((t)x)
#endif

// The eternal int-to-string problem in C++
str _to_string(long long value, int base, int width, char fill);
str _to_string(long long);
template<class T>
    inline str to_string(const T& value, int base, int width = 0, char fill = '0')
        { return _to_string((long long)value, base, width, fill); }
template<class T>
    inline str to_string(const T& value)
        { return _to_string((long long)value); }

unsigned long long from_string(const char*, bool* error, bool* overflow, int base = 10);


class noncopyable 
{
private:
    noncopyable(const noncopyable&);
    const noncopyable& operator= (const noncopyable&);
public:
    noncopyable() {}
    ~noncopyable() {}
};



#define DEF_EXCEPTION(name,msg) \
    struct name: public std::exception \
        { virtual const char* what() const throw() { return msg; } };


typedef std::exception exception;

struct emessage: public exception
{
    const str message;
    emessage(const str&) throw();
    emessage(const char*) throw();
    ~emessage() throw();
    virtual const char* what() const throw();
};


struct esyserr: public emessage
{
    esyserr(int icode, const str& iArg = "");
};


// --- ATOMIC OPERATIONS -------------------------------------------------- //

#ifdef SINGLE_THREADED

inline int pincrement(int* target)  { return ++(*target); }
inline int pdecrement(int* target)  { return --(*target); }

#else

int pincrement(int* target);
int pdecrement(int* target);

#endif



#endif // __COMMON_H
