

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "runtime.h"


// --- podvec -------------------------------------------------------------- //


// Vector template for POD elements (int, pointers, et al)

template <class T>
class podvec: protected contptr
{
    friend void test_podvec();

protected:
    enum { Tsize = sizeof(T) };

public:
    podvec(): contptr()                     { }
    podvec(const podvec& v): contptr(v)     { }
    bool empty() const                      { return contptr::empty(); }
    memint size() const                     { return contptr::size() / Tsize; }
    const T& operator[] (memint i) const    { return *(T*)contptr::data(i * Tsize); }
    const T& at(memint i) const             { return *(T*)contptr::at(i * Tsize); }
    const T& back() const                   { return *(T*)contptr::back(Tsize); }
    void clear()                            { contptr::clear(); }
    void operator= (const podvec& v)        { contptr::operator= (v); }
    void push_back(const T& t)              { new(contptr::_appendnz(Tsize)) T(t); }
    void pop_back()                         { contptr::pop_back(Tsize); }
    void insert(memint pos, const T& t)     { new(contptr::_insertnz(pos * Tsize, Tsize)) T(t); }
    void erase(memint pos)                  { contptr::erase(pos * Tsize, Tsize); }
};


// --- vector -------------------------------------------------------------- //


template <class T>
class vector: public podvec<T>
{
protected:
public:
};


// --- tests --------------------------------------------------------------- //


#define check(x) assert(x)


#define XSTR(s) _STR(s)
#define _STR(s) #s

#ifdef SH64
#  define INTEGER_MAX_STR "9223372036854775807"
#  define INTEGER_MAX_STR_PLUS "9223372036854775808"
#  define INTEGER_MIN_STR "-9223372036854775808"
#else
#  define INTEGER_MAX_STR "2147483647"
#  define INTEGER_MAX_STR_PLUS "2147483648"
#  define INTEGER_MIN_STR "-2147483648"
#endif


void test_podvec()
{
    podvec<int> v1;
    check(v1.empty());
    podvec<int> v2 = v1;
    check(v1.empty() && v2.empty());
    v1.push_back(10);
    v1.push_back(20);
    v1.push_back(30);
    v1.push_back(40);
    check(v1.size() == 4);
    check(v2.empty());
    check(v1[0] == 10);
    check(v1[1] == 20);
    check(v1[2] == 30);
    check(v1[3] == 40);
    v2 = v1;
    check(!v1.unique() && !v2.unique());
    check(v2.size() == 4);
    v1.erase(2);
    check(v1.size() == 3);
    check(v2.size() == 4);
    check(v1[0] == 10);
    check(v1[1] == 20);
    check(v1[2] == 40);
    v1.erase(2);
    check(v1.size() == 2);
    v1.insert(0, 50);
    check(v1.size() == 3);
    check(v1[0] == 50);
    check(v1[1] == 10);
    check(v1[2] == 20);
    v2.clear();
    check(v2.empty());
    check(!v1.empty());
    check(v1.back() == 20);
}


int main()
{
    printf("%lu %lu\n", sizeof(object), sizeof(container));

    test_podvec();

    if (object::allocated != 0)
    {
        fprintf(stderr, "object::allocated: %d\n", object::allocated);
        _fatal(0xff01);
    }

    return 0;
}
