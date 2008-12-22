#ifndef __CONTAIN_H
#define __CONTAIN_H


#include "str.h"


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
    void operator= (const PodArray<T>& a)  { arrayimpl::assign(a); }

    int size() const                { return arrayimpl::size() / Tsize; }
    int bytesize() const            { return arrayimpl::bytesize(); }
    bool empty() const              { return arrayimpl::empty(); }
    int refcount() const            { return arrayimpl::refcount(); }
    void clear()                    { arrayimpl::clear(); }
    T& add()                        { return *::new(Tptr(arrayimpl::add(Tsize))) T(); }
    void add(const T& t)            { ::new(Tptr(arrayimpl::add(Tsize))) T(t); }
    T& ins(int i)                   { return *::new(Tptr(arrayimpl::ins(idxa(i), Tsize))) T(); }
    void ins(int i, const T& t)     { ::new(Tptr(arrayimpl::ins(idxa(i), Tsize))) T(t); }
    void del(int i)                 { arrayimpl::del(idx(i), Tsize); }
    const T& operator[] (int i) const  { return *Tptr(arrayimpl::operator[] (idx(i))); }
    const T& top() const            { return operator[] (size() - 1); }
    void pop()                      { arrayimpl::pop(Tsize); }
    T& _at(int i) const             { return *Tptr(arrayimpl::_at(i * Tsize)); }
};


template <class T>
class Array: public PodArray<T>
{
protected:
    void unique()
    {
        if (!PodArray<T>::empty() && PodArray<T>::refcount() > 1)
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
    ~Array()                        { clear(); }
    void operator= (const Array& a) { PodArray<T>::operator= (a); }
    void del(int i)                 { unique(); PodArray<T>::operator[](i).~T(); PodArray<T>::del(i); }
    void pop()                      { del(PodArray<T>::size() - 1); }
    void clear()
    {
        if (!PodArray<T>::empty())
        {
            if (PodArray<T>::_unlock() == 0)
            {
                for (int i = PodArray<T>::size() - 1; i >= 0; i--)
                    PodArray<T>::_at(i).~T();
                PodArray<T>::_free();
            }
            else
                PodArray<T>::_empty();
        }
    }
};


union fifoquant
{
    ptr   ptr_;
    int   int_;
    large large_;
};


#define FIFO_CHUNK_SIZE int(sizeof(fifoquant) * 16)


struct FifoChunk
{
    static int chunkCount;
    char* data;
    FifoChunk();
    FifoChunk(const FifoChunk& f);
    ~FifoChunk();
};


class fifoimpl: private Array<FifoChunk>
{
public:
    short left, right;

    fifoimpl();
    fifoimpl(const fifoimpl&);
    ~fifoimpl();
    void operator= (const fifoimpl&);

    void* _at(int) const;
    FifoChunk& _chunkat(int i) const  { return Array<FifoChunk>::_at(i); }
    int chunks() const                { return Array<FifoChunk>::size(); }

    void push(const char*, int);
    int  pull(char*, int);
    ptr  advance(int len)             { push(NULL, len); return _at(size() - len); }
    void skip(int len)                { pull(NULL, len); }
    int  size() const;
    void* at(int i) const
    {
#ifdef DEBUG
        if (unsigned(i) >= unsigned(size()))
            idxerror();
#endif
        return _at(i);
    }
};


template <class T>
class PodFifo: private fifoimpl
{
    typedef T* Tptr;
    enum { Tsize = sizeof(T) };
public:
    PodFifo(): fifoimpl()  { assert((FIFO_CHUNK_SIZE / Tsize) * Tsize == FIFO_CHUNK_SIZE); }
    PodFifo(const PodFifo& f): fifoimpl(f)  { }
    ~PodFifo()  { }
    
    int size() const  { return fifoimpl::size() / Tsize; }
    const T& operator[] (int i) const { return *Tptr(fifoimpl::at(i * Tsize)); }
    const T& preview()  { return *Tptr(fifoimpl::at(0)); }
    void push(const T& t)  { ::new(Tptr(fifoimpl::advance(Tsize))) T(t); }
    T pull()
    {
        ptr p = fifoimpl::at(0);
        T t = *Tptr(p);
        Tptr(p)->~T();
        fifoimpl::skip(Tsize);
        return t;
    }
};


class stackimpl
{
public:
    char* stack;
    int capacity;
    int count;

    stackimpl();
    ~stackimpl()  { clear(); }

    void stackunderflow() const;
    void invstackop() const;
    void realloc(int newsize);
    void clear();

    bool empty()  { return count == 0; }
    int size()    { return count; }

    void* advance(int len)
    {
        count += len;
        if (count > capacity)
            realloc(count);
        return stack + count - len;
    }
    
    void* withdraw(int len)
    {
        count -= len;
#ifdef DEBUG
        if (count < 0) invstackop();
#endif
        return stack + count;
    }
    
    void* _at(int index) const // always negative
    {
        return stack + count + index;
    }
    
    void* at(int index) const
    {
#ifdef DEBUG
        if (unsigned(~index) >= unsigned(count))
            invstackop();
#endif
        return _at(index);
    }

    static int stackAlloc;
};


template <class T>
class PodStack: protected stackimpl
{
protected:
    typedef T* Tptr;
    enum { Tsize = sizeof(T) };
public:
    PodStack(): stackimpl()   { }
    ~PodStack()               { }
    bool empty()              { return stackimpl::empty(); }
    int size()                { return stackimpl::size() / Tsize; }
    const T& _at(int i) const { return *Tptr(stackimpl::_at(i * Tsize)); }
    const T& at(int i) const  { return *Tptr(stackimpl::at(i * Tsize)); }
    T& push()                 { return *Tptr(stackimpl::advance(Tsize)); }
    void push(const T& t)     { ::new(&push()) T(t); }
    const T& top() const      { return *Tptr(stackimpl::at(-Tsize)); }
    const T& pop()            { return *Tptr(stackimpl::withdraw(Tsize)); }
};


template <class T>
class Stack: public PodStack<T>
{
public:
    Stack(): PodStack<T>()   { }
    ~Stack()                 { clear(); }
    void pop()               { PodStack<T>::top().~T(); PodStack<T>::pop(); }
    void clear()
    {
        if (!PodStack<T>::empty())
            for (int i = -1; -i <= PodStack<T>::size(); i--)
                PodStack<T>::_at(i).~T();
        PodStack<T>::clear();
    }
};


#endif

