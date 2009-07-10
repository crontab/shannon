

#include <sstream>
#include <iomanip>

#include "common.h"


void fatal(int code, const char* msg) 
{
    fprintf(stderr, "\nInternal [%04x]: %s\n", code, msg);
    exit(code);
}


str to_string(integer value)
{
    std::stringstream s;
    s << value;
    return s.str();
}


str to_string(integer value, int base, int width, char fill)
{
    using namespace std;
    stringstream s;
    s << setbase(base) << setw(width) << setfill(fill) << value;
    return s.str();
}


uinteger from_string(const char* p, bool* error, bool* overflow, int base)
{
    *error = false;
    *overflow = false;

    if (p == 0 || *p == 0 || base < 2 || base > 64)
        { *error = true; return 0; }

    uinteger result = 0;

    do 
    {
        int c = *p++;

        if (c >= 'a')
        {
            // for the numeration bases that use '.', '/', digits and
            // uppercase letters the letter case is insignificant.
            if (base <= 38)
                c -= 'a' - '9' - 1;
            else  // others use both upper and lower case letters
                c -= ('a' - 'Z' - 1) + ('A' - '9' - 1);
        }
        else if (c > 'Z')
            { *error = true; return 0; }
        else if (c >= 'A')
            c -= 'A' - '9' - 1;
        else if (c > '9')
            { *error = true; return 0; }

        c -= (base > 36) ? '.' : '0';
        if (c < 0 || c >= base)
            { *error = true; return 0; }

        uinteger t = result * unsigned(base);
        if (t / base != result)
            { *overflow = true; return 0; }
        result = t;
        t = result + unsigned(c);
        if (t < result)
            { *overflow = true; return 0; }
        result = t;

    }
    while (*p != 0);

    return result;
}



emessage::~emessage() throw() { }

const char* emessage::what() const throw()  { return message.c_str(); }


static str sysErrorStr(int code, const str& arg)
{
    // For some reason strerror_r() returns garbage on my 64-bit Ubuntu. That's unfortunately
    // not the only strange thing about this computer and OS. Could be me, could be hardware
    // or could be Linux. Or all.
    // Upd: so I updated both hardware and OS, still grabage on 64 bit, but OK on 32-bit.
    // What am I doing wrong?
//    char buf[1024];
//    strerror_r(code, buf, sizeof(buf));
    str result = strerror(code);
    if (!arg.empty())
        result += " (" + arg + ")";
    return result;
}


esyserr::esyserr(int code, const str& arg)
    : emessage(sysErrorStr(code, arg))  { }



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


