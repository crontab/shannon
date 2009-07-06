#ifndef __COMMON_H
#define __COMMON_H


#include <stdint.h>

#include <string>
#include <exception>

#if !defined(SINGLE_THREADED) && !defined(MULTI_THREADED)
#  define SINGLE_THREADED
#endif


#if (defined(DEBUG) || defined(_DEBUG)) && !defined(RANGE_CHECKING)
#  define RANGE_CHECKING
#endif


typedef std::string str;
typedef int64_t integer;
typedef double real;


#if defined __x86_64__
#  define PTR64
#elif defined __i386__
#  define PTR32
#else
#  error Unknown architecure.
#endif


// --- ATOMIC OPERATIONS -------------------------------------------------- //

#ifdef SINGLE_THREADED

inline int pincrement(int* target)  { return ++(*target); }
inline int pdecrement(int* target)  { return --(*target); }

#else

int pincrement(int* target);
int pdecrement(int* target);

#endif


// --- MISC --------------------------------------------------------------- //

void fatal(int code, const char* msg);

template<class T>
    inline T imax(T x, T y)  { return (x > y) ? x : y; }
template<class T>
    inline T imin(T x, T y)  { return (x < y) ? x : y; }


int memquantize(int a);


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


#endif // __COMMON_H
