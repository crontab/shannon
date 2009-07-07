#ifndef __COMMON_H
#define __COMMON_H


#include <limits.h>

#include <string>
#include <exception>


#if !defined(SINGLE_THREADED) && !defined(MULTI_THREADED)
#  define SINGLE_THREADED
#endif


#if (defined(DEBUG) || defined(_DEBUG)) && !defined(RANGE_CHECKING)
#  define RANGE_CHECKING
#endif


#if defined __x86_64__
#  define PTR64
#elif defined __i386__
#  define PTR32
#else
#  error Unknown architecure.
#endif


// --- BASIC DATA TYPES --------------------------------------------------- //


// Default fundamental types: string, integer, real and memory size
// Normally the "integer" type should be 64 bit, unless a port is needed
// to a 16-bit system.
typedef long long integer;
typedef unsigned long long uinteger;

#define INTEGER_MIN LLONG_MIN
#define INTEGER_MAX LLONG_MAX

typedef size_t mem;
typedef std::string str;
typedef double real;


// --- MISC --------------------------------------------------------------- //


void fatal(int code, const char* msg);


template<class T>
    inline T imax(T x, T y)  { return (x > y) ? x : y; }
template<class T>
    inline T imin(T x, T y)  { return (x < y) ? x : y; }


// The eternal int-to-string problem in C++
str to_string(integer value);
str to_string(integer value, int base, int width = 0, char fill = '0');
uinteger from_string(const char*, bool* error, bool* overflow, int base = 10);


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


struct emessage: public std::exception
{
    const str message;
    emessage(const str& message): message(message) { }
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
