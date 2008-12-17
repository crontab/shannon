#ifndef __ARRAY_H
#define __ARRAY_H

#ifndef __STR_H
#include "str.h"
#endif


// Dynamic ref-counted array of chars, privately based on string

class arrayimpl: protected string
{
public:
    arrayimpl(): string()  { }
    arrayimpl(const arrayimpl& a): string(a) { }
    ~arrayimpl()           { }
    
    int size() const                { return string::size(); }
    int bytesize() const            { return string::bytesize(); }
    void clear()                    { string::clear(); }
    bool empty() const              { return string::empty(); }
    char* add(int cnt)              { return string::appendn(cnt); }
    char* ins(int where, int cnt)   { return string::ins(where, cnt); }
    void del(int where, int cnt)    { string::del(where, cnt); }
    void pop(int cnt)               { string::resize(size() - cnt); }
    char* operator[] (int i)        { return &string::operator[] (i); }
    const char* operator[] (int i) const  { return &string::operator[] (i); }
    void operator= (const arrayimpl& a)   { string::assign(a); }
};



template <class T>
class PodArray: protected arrayimpl
{
protected:
    typedef T* Tptr;
    enum { Tsize = sizeof(T) };

#ifdef CHECK_BOUNDS
    int idx(int i) const
        { if (unsigned(i * Tsize) >= unsigned(arrayimpl::size())) idxerror(); return i * Tsize; }
    int idxa(int i) const
        { if (unsigned(i * Tsize) > unsigned(arrayimpl::size())) idxerror(); return i * Tsize; }
#else
    int idx(int i) const        { return i * Tsize; }
    int idxa(int i) const       { return i * Tsize; }
#endif

public:
    PodArray(): arrayimpl()  { }
    PodArray(const PodArray<T>& a): arrayimpl(a)  { }
    ~PodArray() { }

    int size() const                { return arrayimpl::size() / Tsize; }
    int bytesize() const            { return arrayimpl::bytesize(); }
    void clear()                    { arrayimpl::clear(); }
    bool empty() const              { return arrayimpl::empty(); }
    int refcount() const            { return arrayimpl::refcount(); }
    T& add()                        { return *Tptr(arrayimpl::add(Tsize)); }
    void add(const T& t)            { add() = t; }
    T& ins(int i)                   { return *Tptr(arrayimpl::ins(idxa(i), Tsize)); }
    void ins(int i, const T& t)     { ins(i) = t; }
    void del(int i)                 { arrayimpl::del(idx(i), Tsize); }
    T& operator[] (int i)           { return *Tptr(arrayimpl::operator[] (idx(i))); }
    const T& operator[] (int i) const  { return *Tptr(arrayimpl::operator[] (idx(i))); }
    T& top()                        { return operator[] (size() - 1); }
    const T& top() const            { return operator[] (size() - 1); }
    T pop()                         { T t = top(); arrayimpl::pop(Tsize); return t; }
    void pull()                     { arrayimpl::del(0, Tsize); }
    void operator= (const PodArray<T>& a)  { arrayimpl::assign(a); }
    T& _at(int i) const             { return *Tptr(data + i * Tsize); }
};


template <class T>
class Array: protected PodArray<T>
{
protected:
    void unique()
    {
        if (!string::empty() && string::refcount() > 1)
        {
            PodArray<T> old = *this;
            PodArray<T>::_alloc(old.bytesize());
            for (int i = 0; i < PodArray<T>::size(); i++)
                ::new(&PodArray<T>::_at(i)) T(old._at(i));
            PodArray<T>::_unref(old);
        }
    }

public:
    Array(): PodArray<T>()          { }
    Array(const Array& a): PodArray<T>(a)  { }
    ~Array()                        { }
    void operator= (const Array& a) { PodArray<T>::operator= (a); }
    T& top()                        { unique(); return PodArray<T>::top(); }
    T& add()                        { unique(); return PodArray<T>::add(); }
    void add(const T& t)            { add() = t; }
    T& ins(int i)                   { unique(); return PodArray<T>::ins(); }
    void ins(int i, const T& t)     { ins(i) = t; }
    T& operator[] (int i)           { unique(); return PodArray<T>::operator[] (i); }
    const T& operator[] (int i) const  { return PodArray<T>::operator[] (i); }
    void pop()                      { del(PodArray<T>::size() - 1); }
    void pull()                     { del(0); }
    void del(int i)                 { unique(); PodArray<T>::_at(i).~T(); PodArray<T>::del(i); }
    void clear()
    {
        if (PodArray<T>::_unlock() == 0)
        {
            for (int i = PodArray<T>::size() - 1; i >= 0; i--)
                PodArray<T>::_at(i).~T();
            PodArray<T>::_free();
        }
    }
};


#endif

