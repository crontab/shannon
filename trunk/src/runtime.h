#ifndef __RUNTIME_H
#define __RUNTIME_H


#include "charset.h"
#include "common.h"


// --- object & objptr ----------------------------------------------------- //


// Basic reference-counted object with a virtual destructor. Also a basic smart
// pointer is defined, objptr.

class Type; // see typesys.h

class object: public noncopyable
{
protected:
    int refcount;

    static object* _realloc(object* p, size_t self, memint extra);
    void _assignto(object*& p);

public:
    object()                    : refcount(0) { }
    virtual ~object();
    virtual bool empty() const; // abstract
    virtual Type* type() const;

    static int allocated; // used only in DEBUG mode

    void* operator new(size_t);
    void* operator new(size_t, memint extra);
    void  operator delete(void* p);
//    template <class T>
//        static void realloc(T*& p, memint extra)
//            { p = (T*)_realloc(p, sizeof(T), extra); }

    bool unique() const         { assert(refcount > 0); return refcount == 1; }
    void release();
    object* ref()               { if (this) pincrement(&refcount); return this; }
    object* refnz()             { pincrement(&refcount); return this; }
    template <class T>
        T* ref()                { object::ref(); return cast<T*>(this); }
    template <class T>
        void assignto(T*& p)    { _assignto((object*&)p); }
};


template <class T>
class objptr
{
protected:
    T* obj;
    
public:
    objptr()                            : obj(NULL) { }
    objptr(const objptr& p)             { obj = (T*)p.obj->ref(); }
    objptr(T* o)                        { obj = (T*)o->ref(); }
    ~objptr()                           { obj->release(); }
    void clear()                        { obj->release(); obj = NULL; }
    bool empty() const                  { return obj == NULL; }
    void operator= (const objptr& p)    { p.obj->assignto(obj); }
    void operator= (T* o)               { o->assignto(obj); }
    T& operator* ()                     { return *obj; }
    const T& operator* () const         { return *obj; }
    T* operator-> () const              { return obj; }
    operator T*() const                 { return obj; }
};



// --- container & contptr ------------------------------------------------- //


// Basic resizable container, for internal use

class container: public object
{
protected:
    memint _capacity;
    memint _size;

public:
    container(): _capacity(0), _size(0)  { } // for the _null object
    container(memint cap, memint siz)
        : _capacity(cap), _size(siz)  { assert(siz > 0 && cap >= siz); }
    ~container();

    static void overflow();
    static void idxerr();
    void clear();
    char* data() const          { return (char*)(this + 1); }
    char* data(memint i) const  { assert(i >= 0 && i <= _size); return data() + i; }
    char* end() const           { return data() + _size; }
    bool empty() const; // virt. override
    memint size() const         { return _size; }
    memint capacity() const     { return _capacity; }
    bool unique() const         { return _size && object::unique(); }
    bool has_room() const       { return _size < _capacity; }

    static memint calc_prealloc(memint);

    virtual container* new_(memint cap, memint siz);
    virtual void finalize(void*, memint);
    virtual void copy(void* dest, const void* src, memint size);
    
    container* new_growing(memint newsize);
    container* new_precise(memint newsize);
    container* realloc(memint newsize); // Make this virtual if sizeof(*this) > sizeof(container)
    void set_size(memint newsize)
        { assert(newsize > 0 && newsize <= _capacity); _size = newsize; }
    void dec_size()             { assert(_size > 0); _size--; }
};


class contptr: public noncopyable
{
    friend void test_contptr();
    friend void test_string();

protected:
    container* obj;

    void _init()                        { obj = &null; }
    void _init(const contptr& s)        { obj = (container*)s.obj->refnz(); }
    char* _init(memint);
    void _init(const char*, memint);
    void _fin()                         { if (!empty()) obj->release(); }
    bool unique() const                 { return obj->unique(); }
    char* mkunique();
    void chkidx(memint i) const         { if (unsigned(i) >= unsigned(obj->size())) container::idxerr(); }
    void chkidxa(memint i) const        { if (unsigned(i) > unsigned(obj->size())) container::idxerr(); }
    void _assign(container* o)          { _fin(); obj = (container*)o->refnz(); }
    char* _insertnz(memint pos, memint len);
    char* _appendnz(memint len);
    void _erasenz(memint pos, memint len);
    void _popnz(memint len);

public:
    static container null;

    contptr()                           { _init(); }                // *
    contptr(const contptr& s)           { _init(s); }
    contptr(const char* s, memint len)  { _init(s, len); }          // *
    ~contptr()                          { _fin(); }

    void operator= (const contptr& s);
    void assign(const char*, memint);                               // *
    void clear();                                                   // *

    bool empty() const                  { return !obj->size(); }
    memint size() const                 { return obj->size(); }
    memint capacity() const             { return obj->capacity(); }
    const char* data() const            { return obj->data(); }
    const char* data(memint pos) const  { return obj->data(pos); }
    const char* at(memint i) const;
    const char* back() const;

    void insert(memint pos, const char* buf, memint len);
    void insert(memint pos, const contptr& s);
    void append(const char* buf, memint len);
    void append(const contptr& s);
    void erase(memint pos, memint len);                             // *
    void push_back(char c)              { *_appendnz(1) = c; }
    void pop_back(memint len);                                      // *

    char* resize(memint);                                           // *
    void resize(memint, char);                                      // *
};
// * - must be redefiend in descendant templates because of the null object


// --- str ----------------------------------------------------------------- //


class str: public contptr
{
protected:
    void _init(const char*, memint len);

public:
    str(): contptr()  { }
    str(const char* buf, memint len) { _init(buf, len); }
    str(const char*);
    str(const str& s): contptr(s)  { }
    
    const char* c_str() const; // can actually modify the object
    char operator[] (memint i) const        { return *obj->data(i); }
    char at(memint i) const                 { return *contptr::at(i); }
    char back() const                       { return *contptr::back(); }
    void put(memint pos, char c);

    enum { npos = -1 };
    memint find(char c) const;
    memint rfind(char c) const;

    typedef char* const_iterator;
    const char* begin() const               { return obj->data(); }
    const char* end() const                 { return obj->end(); }

    int cmp(const char*, memint) const;
    bool operator== (const char* s) const   { return cmp(s, pstrlen(s)) == 0; }
    bool operator== (const str& s) const    { return cmp(s.data(), s.size()) == 0; }
    bool operator!= (const char* s) const   { return !(*this == s); }
    bool operator!= (const str& s) const    { return !(*this == s); }

    void operator+= (const char* s);
    void operator+= (const str& s)          { append(s); }
    str  substr(memint pos, memint len) const;
    str  substr(memint pos) const;
};



// --- string utilities ---------------------------------------------------- //


str _to_string(long long value, int base, int width, char fill);
str _to_string(long long);
template<class T>
    inline str to_string(const T& value, int base, int width = 0, char fill = '0')
        { return _to_string((long long)value, base, width, fill); }
template<class T>
    inline str to_string(const T& value)
        { return _to_string((long long)value); }

unsigned long long from_string(const char*, bool* error, bool* overflow, int base = 10);

str remove_filename_path(const str&);
str remove_filename_ext(const str&);


/*
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
    podvec(const podvec& v): container(v)   { }
    ~podvec()                               { }
    bool empty() const                      { return container::empty(); }
    memint size() const                     { return container::size() / Tsize; }
    const T& operator[] (memint i) const    { return *(T*)container::data(i * Tsize); }
    const T& at(memint i) const             { return *(T*)container::at(i * Tsize); }
    const T& back() const                   { return container::back<T>(); }
    void clear()                            { container::clear(); }
    void operator= (const podvec& v)        { container::assign(v); }
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
    void _fin();
    void _mkunique();
    void mkunique()                         { if (!vector::unique()) _mkunique(); }

public:
    vector(): podvec<T>()                   { }
    vector(const vector& v): podvec<T>(v)   { }
    ~vector()                               { _fin(); }
    void clear()                            { _fin(); vector::_init(); }
    void operator= (const vector& v);
    void push_back(const T& t);
    void pop_back();
    void insert(memint pos, const T& t);
    void erase(memint pos);
};


template <class T>
void vector<T>::_mkunique()
{
    rcdynamic* d = container::_precise_alloc(container::size());
    for (memint i = 0; i < vector::size(); i++)
        ::new(d->data<T>(i)) T(vector::operator[](i));
    vector::_replace(d);
}


template <class T>
void vector<T>::_fin()
{
     if (!vector::empty())
     {
        if (vector::dyn->deref())
        {
            for (memint i = vector::size() - 1; i >= 0; i--)
                vector::operator[](i).~T();
            delete vector::dyn;
        }
        container::_init(); // make the base dtors happy
     }
}


template <class T>
void vector<T>::operator= (const vector& v)
{
    if (vector::dyn != v.dyn)
    {
        _fin();
        container::_init(v);
    }
}


template <class T>
void vector<T>::push_back(const T& t)
{
    mkunique();
    new((T*)container::append(Tsize)) T(t);
}


// The 3 methods below can be improved in terms of performance but will be
// bigger in that case
template <class T>
void vector<T>::pop_back()
{
    mkunique();
    container::back<T>().~T();
    vector::dyn->dec_size(Tsize);
}


template <class T>
void vector<T>::insert(memint pos, const T& t)
{
    mkunique();
    new((T*)container::insert(pos * Tsize, Tsize)) T(t);
}


template <class T>
void vector<T>::erase(memint pos)
{
    mkunique();
    vector::at(pos).~T();
    podvec<T>::erase(pos);
}

*/
#endif // __RUNTIME_H
