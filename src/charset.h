#ifndef __CHARSET_H
#define __CHARSET_H

#include <string.h>

#ifndef __PORT_H
#include "port.h"
#endif


class charset 
{
protected:
    enum
    {
        charsetbits = 256,
        charsetbytes = charsetbits / 8,
        charsetwords = charsetbytes / sizeof(int)
    };

    char data[charsetbytes];

public:
    charset()                                      { clear(); }
    charset(const charset& s)                      { assign(s); }
    charset(const char* setinit)                   { assign(setinit); }

    void assign(const charset& s)                  { memcpy(data, s.data, charsetbytes); }
    void assign(const char* setinit);
    void clear()                                   { memset(data, 0, charsetbytes); }
    void fill()                                    { memset(data, -1, charsetbytes); }
    void include(char b)                           { data[uchar(b) / 8] |= uchar(1 << (uchar(b) % 8)); }
    void include(char min, char max);
    void exclude(char b)                           { data[uchar(b) / 8] &= uchar(~(1 << (uchar(b) % 8))); }
    void unite(const charset& s);
    void subtract(const charset& s);
    void intersect(const charset& s);
    void invert();
    bool contains(char b) const                    { return (data[uchar(b) / 8] & (1 << (uchar(b) % 8))) != 0; }
    bool eq(const charset& s) const                { return memcmp(data, s.data, charsetbytes) == 0; }
    bool le(const charset& s) const;

    charset& operator=  (const charset& s)         { assign(s); return *this; }
    charset& operator+= (const charset& s)         { unite(s); return *this; }
    charset& operator+= (char b)                   { include(b); return *this; }
    charset  operator+  (const charset& s) const   { charset t = *this; return t += s; }
    charset  operator+  (char b) const             { charset t = *this; return t += b; }
    charset& operator-= (const charset& s)         { subtract(s); return *this; }
    charset& operator-= (char b)                   { exclude(b); return *this; }
    charset  operator-  (const charset& s) const   { charset t = *this; return t -= s; }
    charset  operator-  (char b) const             { charset t = *this; return t -= b; }
    charset& operator*= (const charset& s)         { intersect(s); return *this; }
    charset  operator*  (const charset& s) const   { charset t = *this; return t *= s; }
    charset  operator!  () const                   { charset t = *this; t.invert(); return t; }
    bool operator== (const charset& s) const       { return eq(s); }
    bool operator!= (const charset& s) const       { return !eq(s); }
    bool operator<= (const charset& s) const       { return le(s); }
    bool operator>= (const charset& s) const       { return s.le(*this); }
    bool operator[] (char b) const                 { return contains(b); }
};


inline charset operator+ (char b, const charset& s)  { return s + b; }

#endif // _charset_H
