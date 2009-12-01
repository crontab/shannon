

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "runtime.h"


// --- podvec -------------------------------------------------------------- //


template <class T>
class podvec: protected container
{
    friend void test_podvec();

protected:
    enum { Tsize = sizeof(T) };
    T* data(memint i) const                 { return (T*)container::data(i * Tsize); }

public:
    podvec(): container()                   { }
    podvec(const podvec<T>& v): container(v)   { }
    ~podvec()                               { }
    bool empty() const                      { return container::empty(); }
    memint size() const                     { return container::size() / Tsize; }
    const T& operator[] (memint i) const    { return *(T*)container::data(i * Tsize); }
    const T& at(memint i) const             { return *(T*)container::at(i * Tsize); }
    void clear()                            { container::clear(); }
    void operator= (const podvec<T>& v)     { container::assign(v); }
    void push_back(const T& t)              { *(T*)container::append(Tsize) = t; }
    void pop_back()                         { container::pop_back<T>(); }
    void insert(memint pos, const T& t)     { *(T*)container::insert(pos * Tsize, Tsize) = t; }
    void erase(memint pos)                  { container::erase(pos * Tsize, Tsize); }
};


// --- vector -------------------------------------------------------------- //


template <class T>
class vector: public podvec<T>
{
    // Note: the inheritance is public for the sake of simplicity, but be
    // careful when adding new methods to podvec: make sure they work properly
    // in vector too, or otherwise redefine them here.
protected:
    enum { Tsize = sizeof(T) };
    T* mkunique();
    
public:
    vector(): podvec<T>()                   { }
    vector(const vector& v): podvec<T>(v)   { }
    ~vector()                               { clear(); }
    void clear();
    void push_back(const T& t)              { new((T*)container::append(Tsize)) T(t); }
    void pop_back()                         { mkunique()[podvec<T>::size() - 1].~T(); podvec<T>::pop_back(); }
    void insert(memint pos, const T& t)     { new((T*)container::insert(pos * Tsize, Tsize)) T(t); }
    void erase(memint pos)                  { mkunique()[pos].~T(); podvec<T>::erase(pos); }
};


template <class T>
T* vector<T>::mkunique()
{
    if (!vector::unique())
    {
        rcdynamic* d = vector::_precise_alloc(vector::size());
        for (memint i = 0; i < vector::size(); i++)
            ::new(d->data<T>(i)) T(vector::operator[](i));
        vector::_replace(d);
    }
    return vector::data();
}


template <class T>
void vector<T>::clear()
{
     if (!vector::empty())
     {
        if (vector::dyn->deref())
        {
            for (memint i = vector::size() - 1; i >= 0; i--)
                vector::operator[](i).~T();
            delete vector::dyn;
        }
        vector::_init();
     }
}


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
    check(v1.unique());
    podvec<int> v2 = v1;
    check(v1.unique());
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
}


void test_vector()
{
    vector<str> v1;
}


int main()
{
    printf("%lu %lu %lu\n", sizeof(rcblock), sizeof(rcdynamic), sizeof(container));

    test_podvec();
    test_vector();

    if (rcblock::allocated != 0)
    {
        fprintf(stderr, "rcblock::allocated: %d\n", rcblock::allocated);
        _fatal(0xff01);
    }

    return 0;
}
