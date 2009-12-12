#ifndef __CHARSET_H
#define __CHARSET_H


#include <string.h>


class charset
{
public:
    enum
    {
        BITS = 256,
        BYTES = BITS / 8,
        WORDS = BYTES / sizeof(unsigned)
    };

protected:
    typedef unsigned char uchar;

    uchar data[BYTES];

public:
    charset()                                      { clear(); }
    charset(const charset& s)                      { assign(s); }
    charset(const char* setinit)                   { assign(setinit); }

    void assign(const charset& s);
    void assign(const char* setinit);
    bool empty() const;
    void clear()                                   { memset(data, 0, BYTES); }
    void fill()                                    { memset(data, -1, BYTES); }
    void include(int b)                            { data[uchar(b) / 8] |= uchar(1 << (uchar(b) % 8)); }
    void include(int min, int max); 
    void exclude(int b)                            { data[uchar(b) / 8] &= uchar(~(1 << (uchar(b) % 8))); }
    void unite(const charset& s);
    void subtract(const charset& s);
    void intersect(const charset& s);
    void invert();
    bool contains(int b) const                     { return (data[uchar(b) / 8] & (1 << (uchar(b) % 8))) != 0; }
    bool eq(const charset& s) const                { return memcmp(data, s.data, BYTES) == 0; }
    bool le(const charset& s) const;

    charset& operator=  (const charset& s)         { assign(s); return *this; }
    charset& operator+= (const charset& s)         { unite(s); return *this; }
    charset& operator+= (int b)                    { include(b); return *this; }
    charset  operator+  (const charset& s) const   { charset t = *this; return t += s; }
    charset  operator+  (int b) const              { charset t = *this; return t += b; }
    charset& operator-= (const charset& s)         { subtract(s); return *this; }
    charset& operator-= (int b)                    { exclude(b); return *this; }
    charset  operator-  (const charset& s) const   { charset t = *this; return t -= s; }
    charset  operator-  (int b) const              { charset t = *this; return t -= b; }
    charset& operator*= (const charset& s)         { intersect(s); return *this; }
    charset  operator*  (const charset& s) const   { charset t = *this; return t *= s; }
    charset  operator~  () const                   { charset t = *this; t.invert(); return t; }
    bool operator== (const charset& s) const       { return eq(s); }
    bool operator!= (const charset& s) const       { return !eq(s); }
    bool operator<= (const charset& s) const       { return le(s); }
    bool operator>= (const charset& s) const       { return s.le(*this); }
    bool operator[] (int b) const                  { return contains(b); }
};


#endif // __CHARSET_H

