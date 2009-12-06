#ifndef __RUNTIME_H
#define __RUNTIME_H


#include "charset.h"
#include "common.h"


// --- object & objptr ----------------------------------------------------- //


// Basic reference-counted object with a virtual destructor. Also a basic smart
// pointer is defined, objptr.

class object: public noncopyable
{
protected:
    int refcount;

    static object* _realloc(object* p, size_t self, memint extra);
    void _assignto(object*& p);

public:
    object()                    : refcount(0) { }
    virtual ~object();
    virtual bool empty() const = 0; // abstract

    static int allocated; // used only in DEBUG mode

    void* operator new(size_t);
    void* operator new(size_t, memint extra);
    void  operator delete(void*);

    bool unique() const         { assert(refcount > 0); return refcount == 1; }
    void release();
    object* ref()               { if (this) pincrement(&refcount); return this; }
    object* _refnz()            { pincrement(&refcount); return this; }
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
    friend class contptr;
    friend class str;

protected:
    memint _capacity;
    memint _size;

    static void overflow();
    static void idxerr();
    char* data() const          { return (char*)(this + 1); }
    char* data(memint i) const  { assert(i >= 0 && i <= _size); return data() + i; }
    template <class T>
        T* data(memint i) const { return (T*)data(i * sizeof(T)); }
    char* end() const           { return data() + _size; }
    bool empty() const; // virt. override
    memint size() const         { return _size; }
    memint capacity() const     { return _capacity; }
    bool unique() const         { return _size && object::unique(); }
    bool has_room() const       { return _size < _capacity; }

    static memint calc_prealloc(memint);

    container* new_growing(memint newsize);
    container* new_precise(memint newsize);
    container* realloc(memint newsize);
    void set_size(memint newsize)
        { assert(newsize > 0 && newsize <= _capacity); _size = newsize; }
    void dec_size()             { assert(_size > 0); _size--; }
    container* ref()            { return (container*)_refnz(); }
    bool bsearch(void* key, memint& i, memint count);
    
    virtual container* new_(memint cap, memint siz) = 0;
    virtual container* null_obj() = 0;
    virtual void finalize(void*, memint) = 0;
    virtual void copy(void* dest, const void* src, memint) = 0;
    virtual int compare(memint index, void* key); // aborts

public:
    container(): _capacity(0), _size(0)  { } // for the _null object
    container(memint cap, memint siz)
        : _capacity(cap), _size(siz)  { assert(siz > 0 && cap >= siz); }
};


// A "smart" pointer that holds a reference to a container object; this is
// a base for strings, vectors, maps etc.

class contptr: public noncopyable
{
    friend void test_contptr();
    friend void test_string();

protected:
    container* obj;

    void _init(const contptr& s)        { obj = s.obj->ref(); }
    char* _init(container* factory, memint);
    void _init(container* factory, const char*, memint);
    void _fin()                         { if (!empty()) obj->release(); }
    bool unique() const                 { return obj->unique(); }
    char* mkunique();
    void chkidx(memint i) const         { if (unsigned(i) >= unsigned(obj->size())) container::idxerr(); }
    void chkidxa(memint i) const        { if (unsigned(i) > unsigned(obj->size())) container::idxerr(); }
    void _assign(container* o)          { _fin(); obj = o->ref(); }
    char* _insertnz(memint pos, memint len);
    char* _appendnz(memint len);
    void _erasenz(memint pos, memint len);
    void _popnz(memint len);

public:
    contptr(): obj(NULL)                { }  // must be redefined
    contptr(container* _obj): obj(_obj) { }
    contptr(const contptr& s)           { _init(s); }
    ~contptr()                          { _fin(); }

    void operator= (const contptr& s);
    void assign(const char*, memint);
    void clear();

    bool empty() const                  { return !obj->size(); }
    memint size() const                 { return obj->size(); }
    memint capacity() const             { return obj->capacity(); }
    const char* data() const            { return obj->data(); }
    const char* data(memint pos) const  { return obj->data(pos); }
    const char* at(memint i) const      { chkidx(i); return data(i); }
    char* atw(memint i)                 { chkidx(i); return mkunique() + i; }
    const char* back(memint i) const;

    void insert(memint pos, const char* buf, memint len);
    void insert(memint pos, const contptr& s);
    void append(const char* buf, memint len);
    void append(const contptr& s);
    void erase(memint pos, memint len)  { if (len) _erasenz(pos, len); }
    void push_back(char c)              { *_appendnz(1) = c; }
    void pop_back(memint len)           { if (len) _popnz(len); }

    char* resize(memint);
    void resize(memint, char);

    template <class T>
        const T* data(memint pos) const { return (T*)data(pos * sizeof(T)); }
    template <class T>
        const T* at(memint pos) const   { return (T*)at(pos * sizeof(T)); }
    template <class T>
        T* atw(memint pos)              { return (T*)atw(pos * sizeof(T)); }
    template <class T>
        const T* back() const           { return (T*)back(sizeof(T)); }
    template <class T>
        bool bsearch(const T& key, memint& index) const
            { return obj->bsearch((void*)&key, index, size() / sizeof(T)); }
};


// --- string -------------------------------------------------------------- //


// The string class

class str: public contptr
{
    friend void test_contptr();
    friend void test_string();

protected:

    void _init()                            { obj = &null; }
    void _init(const char*, memint);
    void _init(const char*);

public:

    class cont: public container
    {
    protected:
        container* new_(memint cap, memint siz);
        container* null_obj();
        void finalize(void*, memint);
        void copy(void* dest, const void* src, memint size);
    public:
        cont(): container()  { }
        cont(memint cap, memint siz): container(cap, siz)  { }
    };

    str()                                   { _init(); }
    str(const char* buf, memint len)        { _init(buf, len); }
    str(const char* s)                      { _init(s); }
    str(const str& s): contptr(s)           { }

    const char* c_str(); // can actually modify the object
    char operator[] (memint i) const        { return *obj->data(i); }
    char at(memint i) const                 { return *contptr::at(i); }
    char back() const                       { return *contptr::back(1); }
    void replace(memint pos, char c)        { *contptr::atw(pos) = c; }
    void operator= (const char* c);

    enum { npos = -1 };
    memint find(char c) const;
    memint rfind(char c) const;

    int compare(const char*, memint) const;
    int compare(const str& s) const         { return compare(s.data(), s.size()); }
    bool operator== (const char* s) const   { return compare(s, pstrlen(s)) == 0; }
    bool operator== (const str& s) const    { return compare(s.data(), s.size()) == 0; }
    bool operator!= (const char* s) const   { return !(*this == s); }
    bool operator!= (const str& s) const    { return !(*this == s); }

    void operator+= (const char* s);
    void operator+= (const str& s)          { append(s); }
    str  substr(memint pos, memint len) const;
    str  substr(memint pos) const;

    static cont null;
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



// --- podvec -------------------------------------------------------------- //


// Vector template for POD elements (int, pointers, et al). Used internally
// by the compiler itself.

template <class T>
class podvec: protected contptr
{
    friend void test_podvec();
    friend void test_vector();

protected:
    enum { Tsize = sizeof(T) };

    static str::cont null;

    podvec(container* _obj): contptr(_obj)  { }

public:
    podvec(): contptr(&null)                { }
    podvec(const podvec& v): contptr(v)     { }
    bool empty() const                      { return contptr::empty(); }
    memint size() const                     { return contptr::size() / Tsize; }
    const T& operator[] (memint i) const    { return *contptr::data<T>(i); }
    const T& at(memint i) const             { return *contptr::at<T>(i); }
    const T& back() const                   { return *contptr::back<T>(); }
    void clear()                            { contptr::clear(); }
    void operator= (const podvec& v)        { contptr::operator= (v); }
    void push_back(const T& t)              { new(contptr::_appendnz(Tsize)) T(t); }
    void pop_back()                         { contptr::pop_back(Tsize); }
    void insert(memint pos, const T& t)     { new(contptr::_insertnz(pos * Tsize, Tsize)) T(t); }
    void replace(memint pos, const T& t)    { *contptr::atw<T>(pos) = t; }
    void erase(memint pos)                  { contptr::erase(pos * Tsize, Tsize); }

    template <class U>
        void push_back(const U& u)          { new(contptr::_appendnz(Tsize)) T(u); }
    template <class U>
        void insert(memint pos, const U& u) { new(contptr::_insertnz(pos * Tsize, Tsize)) T(u); }
    template <class U>
        void replace(memint pos, const U& u) { *contptr::atw<T>(pos) = u; }
};


template <class T>
    str::cont podvec<T>::null;


// --- vector -------------------------------------------------------------- //


template <class T>
class vector: public podvec<T>
{
    friend void test_vector();

    enum { Tsize = sizeof(T) };

protected:
    typedef podvec<T> parent;
    typedef T* Tptr;
    typedef Tptr& Tref;

    class cont: public container
    {

    protected:
        container* new_(memint cap, memint siz)
            { return new(cap) cont(cap, siz); }

        container* null_obj()
            { return &vector::null; }

        void finalize(void* p, memint count)
        {
            for ( ; count; count -= Tsize, Tref(p)++)
                Tptr(p)->~T();
        }
        
        void copy(void* dest, const void* src, memint count)
        {
            for ( ; count; count -= Tsize, Tref(dest)++, Tref(src)++)
                new(dest) T(*Tptr(src));
        }

    public:
        cont(): container()  { }
        cont(memint cap, memint siz): container(cap, siz)  { }
        ~cont()
        {
            if (_size)
                { finalize(data(), _size); _size = 0; }
        }
    };

    static cont null;

    vector(container* o): parent(o)  { }

public:
    vector(): parent(&null)  { }
};


template <class T>
    typename vector<T>::cont vector<T>::null;


#endif // __RUNTIME_H
