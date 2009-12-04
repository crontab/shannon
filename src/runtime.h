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
    char* end() const           { return data() + _size; }
    bool empty() const; // virt. override
    memint size() const         { return _size; }
    memint capacity() const     { return _capacity; }
    bool unique() const         { return _size && object::unique(); }
    bool has_room() const       { return _size < _capacity; }

    static memint calc_prealloc(memint);

    virtual container* new_(memint cap, memint siz);
    virtual container* null_obj();
    virtual void finalize(void*, memint);
    virtual void copy(void* dest, const void* src, memint size);

    container* new_growing(memint newsize);
    container* new_precise(memint newsize);
    container* realloc(memint newsize); // Make this virtual if sizeof(*this) > sizeof(container)
    void set_size(memint newsize)
        { assert(newsize > 0 && newsize <= _capacity); _size = newsize; }
    void dec_size()             { assert(_size > 0); _size--; }
    
    container* ref()            { return (container*)_refnz(); }

public:
    container(): _capacity(0), _size(0)  { } // for the _null object
    container(memint cap, memint siz)
        : _capacity(cap), _size(siz)  { assert(siz > 0 && cap >= siz); }
    ~container();
};


extern container _null_container;


class contptr: public noncopyable
{
    friend void test_contptr();
    friend void test_string();

protected:
    container* obj;

    void _init()                        { obj = &_null_container; }
    void _init(const contptr& s)        { obj = s.obj->ref(); }
    char* _init(container* factory, memint);
    void _init(container* factory, const char*, memint);
    void _dofin();
    void _fin()                         { if (!empty()) _dofin(); }
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
    contptr()                           { _init(); }                        // *
    contptr(const contptr& s)           { _init(s); }
    contptr(const char* s, memint len)  { _init(&_null_container, s, len); } // *
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
};
// * - must be redefiend in descendant templates because of the null object


// --- string -------------------------------------------------------------- //


// String container: returns the proper runtime type

class strcont: public container
{
protected:
    virtual Type* type() const;
    virtual container* new_(memint cap, memint siz);
    virtual container* null_obj();

public:
    strcont(): container()  { }
    strcont(memint cap, memint siz): container(cap, siz)  { }
};

extern strcont _null_strcont;


// The string class

class str: public contptr
{
protected:
    void _init()                            { obj = &_null_strcont; }
    void _init(const char*, memint);

public:
    str()                                   { _init(); }
    str(const char* buf, memint len)        { _init(buf, len); }
    str(const char*);
    str(const str& s): contptr(s)           { }
    
    const char* c_str() const; // can actually modify the object
    char operator[] (memint i) const        { return *obj->data(i); }
    char at(memint i) const                 { return *contptr::at(i); }
    char back() const                       { return *contptr::back(1); }
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


#endif // __RUNTIME_H
