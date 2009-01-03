#ifndef __COMMON_H
#define __COMMON_H

#include <string.h>


#include "port.h"


// --- STRING -------------------------------------------------------------- //


struct _strrec 
{
    // TODO: make this struct more compact maybe?
    int capacity;
    int length;
    int refcount; // this should be the last, i.e. *(obj - sizeof(int))
};
typedef _strrec* _pstrrec;
const int strrecsize = sizeof(_strrec);


#define STR_BASE(x)      (_pstrrec(x)-1)
#define STR_REFCOUNT(x)  (STR_BASE(x)->refcount)
#define STR_LENGTH(x)    (STR_BASE(x)->length)
#define STR_CAPACITY(x)  (STR_BASE(x)->capacity)
#define STR_LEFT(x)      (STR_BASE(x)->left)
#define STR_RIGHT(x)     (STR_BASE(x)->right)

#define PTR_TO_PSTRING(p)   ((string*)(void*)&(p))
#define PTR_TO_STRING(p)    (*PTR_TO_PSTRING(p))


extern char* emptystr;

class string 
{
protected:
    char* data;

    static void idxerror();
    static void stringoverflow();

    void initialize()  { data = emptystr; }
    void initialize(const char*, int);
    void initialize(const char*);
    void initialize(char);
    void initialize(const char*, int, const char*, int);

    static void _freedata(char*);

#ifdef SMALLER_SLOWER
    void initialize(const string& s);
    void finalize();
#else
    void initialize(const string& s)
            { pincrement(&STR_REFCOUNT(data = s.data)); }
    void finalize()
            { if (STR_LENGTH(data) != 0) { if (_unlock() == 0) _freedata(data); data = emptystr; } }
#endif

#ifdef CHECK_BOUNDS
    void idx(int index) const  { if (unsigned(index) >= unsigned(STR_LENGTH(data))) idxerror(); }
    void idxa(int index) const { if (unsigned(index) > unsigned(STR_LENGTH(data))) idxerror(); }
#else
    void idx(int) const        { }
    void idxa(int) const       { }
#endif

    string(const char* s1, int len1, const char* s2, int len2)  { initialize(s1, len1, s2, len2); }

public:
    string()                                      { initialize(); }
    string(const char* sc, int initlen)           { initialize(sc, initlen); }
    string(const char* sc)                        { initialize(sc); }
    string(char c)                                { initialize(c); }
    string(const string& s)                       { initialize(s); }
    ~string()                                     { finalize(); }

    void assign(const char*, int);
    void assign(const char*);
    void assign(const string&);
    void assign(char);
    string& operator= (const char* sc)            { assign(sc); return *this; }
    string& operator= (char c)                    { assign(c); return *this; }
    string& operator= (const string& s)           { assign(s); return *this; }

    int    size() const                           { return STR_LENGTH(data); }
    int    capacity() const                       { return STR_CAPACITY(data); }
    int    bytesize() const                       { return STR_LENGTH(data); }
    int    refcount() const                       { return STR_REFCOUNT(data); }
    void   clear()                                { finalize(); }
    bool   empty() const                          { return STR_LENGTH(data) == 0; }

    char*  resize(int);
    char*  unique();
    string dup() const                            { return string(data); }
    // char&  operator[] (int i)                     { idx(i); return unique()[i]; }
    const char& operator[] (int i) const          { idx(i); return data[i]; }
    const char* _at(int i) const                  { return data + i; }
    const char* c_str() const;
    const char* c_bytes() const                   { return data; }

    void append(const char* sc, int catlen);
    void append(const char* s);
    void append(char c);
    void append(const string& s);
    char* appendn(int cnt);
    string& operator+= (const char* sc)           { append(sc); return *this; }
    string& operator+= (char c)                   { append(c); return *this; }
    string& operator+= (const string& s)          { append(s); return *this; }

    void del(int from, int cnt);
    char* ins(int where, int len);
    void ins(int where, const char* what, int len);
    void ins(int where, const char* what);
    void ins(int where, const string& what);

    string copy(int from, int cnt) const;
    string operator+ (const char* sc) const;
    string operator+ (char c) const;
    string operator+ (const string& s) const;
    friend string operator+ (const char* sc, const string& s);
    friend string operator+ (char c, const string& s);

    bool operator== (const char* sc) const;
    bool operator== (char) const;
    bool operator== (const string& s) const       { return equal(s); }
    bool operator!= (const char* sc) const        { return !(*this == sc); }
    bool operator!= (char c) const                { return !(*this == c); }
    bool operator!= (const string& s) const       { return !(*this == s); }
    
    int compare(const string& s) const;
    bool equal(const string& s) const;

    friend bool operator== (const char*, const string&);
    friend bool operator== (char, const string&);
    friend bool operator!= (const char*, const string&);
    friend bool operator!= (char, const string&);
    
    // internal stuff, use with care!
    void _alloc(int);
    void _realloc(int);
    void _empty()  { data = emptystr; }
    void _free();
    ptr _initialize() const
            { pincrement(&STR_REFCOUNT(data)); return ptr(data); }
    int  _unlock()
        {
#ifdef DEBUG
            if (STR_REFCOUNT(data) <= 0)
                stringoverflow();
#endif
            return pdecrement(&STR_REFCOUNT(data));
        }
    static ptr _initialize(ptr p)
            { PTR_TO_STRING(p)._initialize(); return p; }
    static ptr _new(int size)
            { ptr p; PTR_TO_STRING(p)._alloc(size); return p; }
    static ptr _grow(ptr p, int size)
            { PTR_TO_STRING(p)._realloc(STR_LENGTH(p) + size); return p; }
    static void _finalize(ptr p)
            { if (p != NULL) PTR_TO_PSTRING(p)->finalize(); }
};


string operator+ (const char* sc, const string& s);
string operator+ (char c, const string& s);
inline bool operator== (const char* sc, const string& s) { return s == sc; }
inline bool operator== (char c, const string& s)         { return s == c; }
inline bool operator!= (const char* sc, const string& s) { return s != sc; }
inline bool operator!= (char c, const string& s)         { return s != c; }

inline int hstrlen(const char* p) // some Unix systems do not accept NULL
    { return p == NULL ? 0 : (int)strlen(p); }


extern int stralloc;

extern string nullstring;

typedef string* pstring;


string itostring(large value, int base, int width = 0, char padchar = 0);
string itostring(ularge value, int base, int width = 0, char padchar = 0);
string itostring(int value, int base, int width = 0, char padchar = 0);
string itostring(unsigned value, int base, int width = 0, char padchar = 0);
string itostring(large v);
string itostring(ularge v);
string itostring(int v);
string itostring(unsigned v);

ularge stringtou(const char* str, bool* error, bool* overflow, int base = 10);


// --- CHARACTER SET ------------------------------------------------------- //


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
    charset  operator~  () const                   { charset t = *this; t.invert(); return t; }
    bool operator== (const charset& s) const       { return eq(s); }
    bool operator!= (const charset& s) const       { return !eq(s); }
    bool operator<= (const charset& s) const       { return le(s); }
    bool operator>= (const charset& s) const       { return s.le(*this); }
    bool operator[] (char b) const                 { return contains(b); }
};


inline charset operator+ (char b, const charset& s)  { return s + b; }


// --- ARRAY --------------------------------------------------------------- //


// Dynamic ref-counted array of chars, based on string

class arrayimpl: public string
{
public:
    char* add(int cnt)              { return string::appendn(cnt); }
    char* ins(int where, int cnt)   { return string::ins(where, cnt); }
    void del(int where, int cnt)    { string::del(where, cnt); }
    void pop(int cnt)               { string::resize(size() - cnt); }
    const char* operator[] (int i) const  { return &string::operator[] (i); }
};



template <class T>
class PodArray: public arrayimpl
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
    void operator= (const PodArray<T>& a)   { arrayimpl::assign(a); }
    void append(const PodArray<T>& a)       { arrayimpl::append(a); }

    int size() const                { return arrayimpl::size() / Tsize; }
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
            old._unlock();
        }
    }

public:
    Array(): PodArray<T>()          { }
    Array(const Array& a): PodArray<T>(a)  { }
    ~Array()                        { clear(); }
    void operator= (const Array& a) { PodArray<T>::operator= (a); }
    void append(const Array<T>& a); // not implemented
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


// --- FIFO ---------------------------------------------------------------- //


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


// --- STACK --------------------------------------------------------------- //


class stackimpl: noncopyable
{
protected:

#ifdef DEBUG
    void idx(int i) const  { if (unsigned(~i) >= unsigned(end - begin)) invstackop(); }
#else
    void idx(int) const  { }
#endif
    
    char* end;
    char* begin;
    char* capend;

    void stackunderflow() const;
    void invstackop() const;
    void grow();

public:
    stackimpl();
    ~stackimpl()  { clear(); }

    void clear();

    bool empty() const  { return end == begin; }
    int size() const    { return end - begin; }

    void* advance(int len)
    {
        end += len;
        if (end > capend)
            grow();
        return end - len;
    }
    
    void* withdraw(int len)
    {
        end -= len;
#ifdef DEBUG
        if (begin == NULL || end < begin) invstackop();
#endif
        return end;
    }
    
    void reserve(int len);
    
    void* _at(int i)        { return end + i; }
    void* _at(int i) const  { return end + i; }
    void* at(int i)         { idx(i); return _at(i); }
    void* at(int i) const   { idx(i); return _at(i); }

    static int stackAlloc;
};


template <class T>
class PodStack: protected stackimpl
{
protected:
    typedef T* Tptr;
    enum { Tsize = int(sizeof(T)) };
public:
    PodStack(): stackimpl()   { }
    ~PodStack()               { }
    bool empty() const        { return stackimpl::empty(); }
    int size() const          { return stackimpl::size() / Tsize; }
    int bytesize() const      { return stackimpl::size(); }
    void clear()              { stackimpl::clear(); }
    void reservebytes(int size)  { stackimpl::reserve(size); }
    T& push()                 { return *Tptr(stackimpl::advance(Tsize)); }
    void push(const T& t)     { ::new(&push()) T(t); }
    T& _at(int i)             { return *Tptr(stackimpl::_at(i * Tsize)); }
    T& at(int i)              { return *Tptr(stackimpl::at(i * Tsize)); }
    T& top()                  { return *Tptr(stackimpl::at(-Tsize)); }
    const T& _at(int i) const { return *Tptr(stackimpl::_at(i * Tsize)); }
    const T& at(int i) const  { return *Tptr(stackimpl::at(i * Tsize)); }
    const T& top() const      { return *Tptr(stackimpl::at(-Tsize)); }
    const T& pop()            { return *Tptr(stackimpl::withdraw(Tsize)); }
    T& pushr()
    {
        void* saveend = end;
        end += Tsize;
#ifdef DEBUG
        if (end > capend)
            invstackop();
#endif
        return *Tptr(saveend);
    }
    char* pushrbytes(int len)
    {
        char* saveend = end;
        end += len;
#ifdef DEBUG
        if (end > capend)
            invstackop();
#endif
        return saveend;
    }
    void restoreendr(char* e)
    {
        end = e;
#ifdef DEBUG
        if (end < begin || end > capend)
            invstackop();
#endif
    }
    void pushbytes(int len)
    {
        advance(len);
    }
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


// --- EXCEPTIONS ---------------------------------------------------------- //


struct Exception
{
    string message;
    Exception(const string& msg);
    ~Exception();
    string what() const { return message; }
};


struct EDuplicate: public Exception
{
    string entry;
    EDuplicate(const string& ientry);
    ~EDuplicate();
};


struct ESysError: public Exception
{
    ESysError(int icode, const string& iArg);
};


void internal(int code);
void internal(int code, const char*);


#endif
