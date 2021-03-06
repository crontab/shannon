#ifndef __RUNTIME_H
#define __RUNTIME_H


#include "common.h"


// --- charset ------------------------------------------------------------- //


class charset
{
public:
    typedef uinteger word;
    enum
    {
        BITS = 256,
        BYTES = BITS / 8,
        WORDS = BYTES / int(sizeof(word))
    };

protected:
    typedef uint8_t uchar;

    uchar data[BYTES];

public:
    charset() throw()                              { clear(); }
    charset(const charset& s) throw()              { assign(s); }
    charset(const char* setinit) throw()           { assign(setinit); }

    void assign(const charset& s) throw();
    void assign(const char* setinit) throw();
    bool empty() const throw();
    void clear() throw()                           { memset(data, 0, BYTES); }
    void fill()                                    { memset(data, -1, BYTES); }
    void include(int b) throw()                    { data[uchar(b) / 8] |= uchar(1 << (uchar(b) % 8)); }
    void include(int min, int max) throw(); 
    void exclude(int b)                            { data[uchar(b) / 8] &= uchar(~(1 << (uchar(b) % 8))); }
    void unite(const charset& s);
    void subtract(const charset& s);
    void intersect(const charset& s);
    void invert();
    bool contains(int b) const                     { return (data[uchar(b) / 8] & (1 << (uchar(b) % 8))) != 0; }
    bool compare(const charset& s) const           { return memcmp(data, s.data, BYTES); }
    bool eq(const charset& s) const                { return compare(s) == 0; }
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
    bool operator== (const charset& s) const       { return compare(s) == 0; }
    bool operator!= (const charset& s) const       { return compare(s) != 0; }
    bool operator<= (const charset& s) const       { return le(s); }
    bool operator>= (const charset& s) const       { return s.le(*this); }
    bool operator[] (int b) const                  { return contains(b); }
};


// --- object -------------------------------------------------------------- //


// object: reference-counted memory block with a virtual destructor

class object
{
    object(const object&) throw();
    void operator= (const object&) throw();

protected:
    atomicint _refcount;

    bool _release();

public:

    void _mkstatic()
    {
        // Prevent this object from being free'd by release() and also from
        // being counted against memory leaks.
        _refcount = 1;
#ifdef DEBUG
        pdecrement(&object::allocated);
#endif
    }

    void* operator new(size_t self);
    void* operator new(size_t self, memint extra);
    void  operator delete(void*);

    // Dirty trick that duplicates an object and hopefully preserves the
    // dynamic type (actually the VMT). Only 'self' bytes is copied; 'extra'
    // remains uninitialized.
    object* _dup(size_t self, memint extra);

    void _assignto(object*& p) throw();
    static object* reallocate(object* p, size_t self, memint extra);

    static atomicint allocated; // used only in DEBUG mode

    bool isunique() const       { return _refcount == 1; }
    atomicint release() throw();
    object* grab() throw()      { pincrement(&_refcount); return this; }
    template <class T>
        T* grab()               { object::grab(); return (T*)(this); }
    template <class T>
        void assignto(T*& p) throw() { _assignto((object*&)p); }

    object() throw(): _refcount(0)  { }
    virtual ~object() throw();
};


void _del_obj(object* o);


#ifdef SHN_FASTER
inline atomicint object::release()
{
    if (this == NULL)
        return 0;
    assert(_refcount > 0);
    atomicint r = pdecrement(&_refcount);
    if (r == 0)
        _del_obj(this);
    return r;
}
#endif


// objptr: "smart" pointer

template <class T>
class objptr
{
protected:
    T* obj;
public:
    objptr()                            : obj(NULL) { }
    objptr(const objptr& p)             : obj(p.obj) { if (obj) obj->grab(); }
    objptr(T* o)                        : obj(o) { if (o) o->grab(); }
    ~objptr()                           { obj->release(); }
    void clear()                        { obj->release(); obj = NULL; }
    bool empty() const                  { return obj == NULL; }
    bool isunique() const               { return empty() || obj->isunique(); }
    bool operator== (const objptr& p)   { return obj == p.obj; }
    bool operator!= (const objptr& p)   { return obj != p.obj; }
    bool operator== (T* o)              { return obj == o; }
    bool operator!= (T* o)              { return obj != o; }
    void operator= (const objptr& p) throw()   { p.obj->assignto(obj); }
    void operator= (T* o) throw()              { o->assignto(obj); }
    T& operator* ()                     { return *obj; }
    const T& operator* () const         { return *obj; }
    T* operator-> () const              { return obj; }
    operator T*() const                 { return obj; }
    T* get() const                      { return obj; }

    // Internal
    void _init()                        { obj = NULL; }
    void _init(T* o)                    { obj = o; if (o) o->grab(); }
    void _fin()                         { obj->release(); }
    void _reinit(T* o)                  { obj = o; }
};


class Type;    // defined in typesys.h
class fifo;

// rtobject: a ref-counted object with runtime type information

class rtobject: public object
{
private:
    Type* _type;
public:
    rtobject(Type* t) throw(): _type(t)  { }
    ~rtobject() throw();
    Type* getType() const   { return _type; }
    void setType(Type* t)   { assert(_type == NULL); _type = t; }
    void clearType() throw()    { _type = NULL; }
    virtual bool empty() const = 0;
    virtual void dump(fifo&) const = 0;
};


// --- container ----------------------------------------------------------- //


// container: resizable ref-counted container of POD data; also base for
// non-POD containers that override finalize() and copy() (and the dtor)

class container: public object
{
protected:
    memint _capacity;
    memint _size;
    // char _data[0];

public:
    // Note: allocate() creates an instance of 'container' while reallocate()
    // never does that and thus it can be used for descendant classes too.
    static container* allocate(memint cap, memint siz) throw();  // (*)
    static container* reallocate(container* p, memint newsize);

    // Creates a duplicate of a given container without copying the data;
    // leaves actual copying to the virtual method copy()
    container* _dup(memint cap, memint siz);

    // TODO: compact()

    static memint _calc_prealloc(memint);
    container(memint cap, memint siz) throw()
        : object(), _capacity(cap), _size(siz)  { }

    static void overflow();
    static void idxerr();
    static void keyerr();

    ~container() throw();
    virtual void finalize(void*, memint) throw();
    virtual void copy(void* dest, const void* src, memint) throw();

    char* data() const              { return (char*)(this + 1); }
    char* data(memint i) const      { return data() + i; }
    char* end() const               { return data(_size); }
    static container* cont(char* d) { return ((container*)d) - 1; }
    memint size() const             { return _size; }
    void set_size(memint newsize)
        { assert(newsize > 0 && newsize <= _capacity); _size = newsize; }
    void dec_size()                 { assert(_size > 0); _size--; }
    memint capacity() const         { return _capacity; }
};


// --- bytevec ------------------------------------------------------------- //


// bytevec: byte vector, implements copy-on-write; the structure itself
// occupies only sizeof(void*); base class for strings and vectors

class bytevec
{
    friend class variant;
    friend class CodeGen;

    friend void test_bytevec();
    friend void test_podvec();

protected:
    objptr<container> obj;

    typedef container* (*alloc_func)(memint cap, memint siz);

    void chkidx(memint i) const         { if (umemint(i) >= umemint(size())) container::idxerr(); }
    void chkidxa(memint i) const        { if (umemint(i) > umemint(size())) container::idxerr(); }
    static void chknonneg(memint v)     { if (v < 0) container::overflow(); } 
    void chknz() const                  { if (empty()) container::idxerr(); }
    bool _isunique() const              { return empty() || obj->isunique(); }
    void _dounique();
    char* mkunique()                    { if (!obj->isunique()) _dounique(); return obj->data(); }
    char* _init(memint len) throw();  // (*)
    void _init(memint len, char fill) throw();  // (*)
    void _init(const char*, memint) throw();  // (*)
    void _init(const bytevec& v) throw() { obj._init(v.obj); }
    char* _init(memint pos, memint len, alloc_func) throw();
    void _init(const bytevec& v, memint pos, memint len, alloc_func) throw();

    char* _insert(memint pos, memint len, alloc_func);
    void _insert(memint pos, const bytevec&, alloc_func);
    char* _append(memint len, alloc_func);
    void _erase(memint pos, memint len);
    void _pop(memint len);
    char* _resize(memint newsize, alloc_func);

public:
    bytevec() throw(): obj()            { }
    bytevec(const bytevec& v) throw()   { _init(v); }
    bytevec(const char* buf, memint len) throw() { _init(buf, len); }  // (*)
    bytevec(memint len, char fill) throw() { _init(len, fill); }  // (*)
    ~bytevec() throw()                  { }

    void operator= (const bytevec& v) throw()  { obj = v.obj; }
    bool operator== (const bytevec& v) const { return obj == v.obj; }
    void assign(const char*, memint);
    void clear();

    bool empty() const                  { return obj.empty(); }
    memint size() const                 { return empty() ? 0 : obj->size(); }
    memint capacity() const             { return empty() ? 0 : obj->capacity(); }
    const char* data() const            { return obj->data(); }
    const char* data(memint i) const    { return obj->data(i); }
    const char* at(memint i) const      { chkidx(i); return obj->data(i); }
    char* atw(memint i)                 { chkidx(i); return mkunique() + i; }
    const char* begin() const           { return empty() ? NULL : obj->data(); }
    const char* end() const             { return empty() ? NULL : obj->end(); }
    const char* back(memint i) const    { chkidxa(i); return obj->end() - i; }
    const char* back() const            { return back(1); }
    char* backw(memint i)               { chkidxa(i); return obj->end() - i; }
    char* backw()                       { return backw(1); }

    void insert(memint pos, const char* buf, memint len);  // (*)
    void insert(memint pos, const bytevec& s)  // (*)
        { _insert(pos, s, container::allocate); }
    void append(const char* buf, memint len);  // (*)
    void append(const bytevec& s);
    void erase(memint pos, memint len);
    void pop(memint len)                { if (len > 0) _pop(len); }
    char* resize(memint newsize)        { return _resize(newsize, container::allocate); } // (*)
    void resize(memint, char);  // (*)

    // Mostly used internally
    template <class T>
        const T* data(memint i) const   { return (T*)data(i * sizeof(T)); }
    template <class T>
        const T* at(memint i) const     { return (T*)at(i * sizeof(T)); }
    template <class T>
        T* atw(memint i)                { return (T*)atw(i * sizeof(T)); }
    template <class T>
        const T* begin() const          { return (T*)begin(); }
    template <class T>
        const T* end() const            { return (T*)end(); }
    template <class T>
        const T* back() const           { return (T*)back(sizeof(T)); }
    template <class T>
        const T* back(memint i) const   { return (T*)back(sizeof(T) * i); }
    template <class T>
        T* backw()                      { return (T*)backw(sizeof(T)); }
    template <class T>
        T* backw(memint i)              { return (T*)backw(sizeof(T) * i); }
    template <class T>
        void pop_back()                 { pop(sizeof(T)); }
    template <class T>
        void pop_back(T& t)             { t = *back<T>(); pop_back<T>(); }
};

// (*) -- works only with the POD container; should be overridden or hidden in
//        descendant classes. The rest works magically on any descendant of
//        'container'. Pure magic. I like this!


// --- str ----------------------------------------------------------------- //


class str: public bytevec
{
protected:
    friend void test_string();

    void _init(const char*) throw();
    void _init(char c) throw()              { bytevec::_init(&c, 1); }

public:
    str() throw(): bytevec()                { }
    str(const str& s)throw(): bytevec(s)    { }
    str(const char* buf, memint len) throw(): bytevec(buf, len)  { }
    str(const char* s) throw()              { _init(s); }
    str(memint len, char fill) throw()      { bytevec::_init(len, fill); }
    str(char c) throw()                     { _init(c); }

    const char* c_str(); // can actually modify the object
    void push_back(char c)                  { *_append(1, container::allocate) = c; }
    void push_front(char c)                 { *_insert(0, 1, container::allocate) = c; }
    char operator[] (memint i) const        { return *data(i); }
    char at(memint i) const                 { return *bytevec::at(i); }
    char back() const                       { return *bytevec::back(); }
    void replace(memint pos, char c)        { *bytevec::atw(pos) = c; }
    void insert(memint pos, char c)         { *_insert(pos, 1, container::allocate) = c; }
    void insert(memint pos, const str& s)   { bytevec::insert(pos, s); }
    void insert(memint pos, const char* s);
    void operator= (const char* c);
    void operator= (char c);
    void replace(memint pos, memint len, const str& s);

    enum { npos = -1 };
    memint find(char c) const;
    memint rfind(char c) const;

    memint compare(const char*, memint) const;
    memint compare(const str& s) const      { return compare(s.data(), s.size()); }
    bool operator== (const char* s) const;
    bool operator== (const str& s) const    { return compare(s.data(), s.size()) == 0; }
    bool operator== (char c) const          { return size() == 1 && *data() == c; }
    bool operator!= (const char* s) const   { return !(*this == s); }
    bool operator!= (const str& s) const    { return !(*this == s); }
    bool operator!= (char c) const          { return !(*this == c); }

    void operator+= (const char* s);
    void operator+= (const str& s)          { append(s); }
    void operator+= (char c)                { push_back(c); }
    str  operator+ (const char* s) const    { str r = *this; r += s; return r; }
    str  operator+ (const str& s) const     { str r = *this; r += s; return r; }
    str  operator+ (char c) const           { str r = *this; r += c; return r; }
    str  substr(memint pos, memint len) const;
    str  substr(memint pos) const;
};


inline str operator+ (const char* s1, const str& s2)
    { str r = s1; r += s2; return r; }

inline str operator+ (char c, const str& s2)
    { str r = c; r += s2; return r; }


// --- string utilities ---------------------------------------------------- //


str _to_string(large value, int base, int width, char fill);
str _to_string(large);
template<class T>
    inline str to_string(const T& value, int base, int width = 0, char fill = '0')
        { return _to_string(large(value), base, width, fill); }
template<class T>
    inline str to_string(const T& value)
        { return _to_string(large(value)); }

ularge from_string(const char*, bool* error, bool* overflow, int base = 10);

str remove_filename_path(const str&);
str remove_filename_ext(const str&);
str to_printable(char);
str to_printable(const str&);
str to_quoted(char c);
str to_quoted(const str&);
str to_displayable(const str&);  // shortens to 40 chars + "..."


// --- podvec -------------------------------------------------------------- //


template <class T>
    struct comparator
        { memint operator() (const T& a, const T& b) { return memint(a - b); } };

template <>
    struct comparator<str>
        { memint operator() (const str& a, const str& b) { return a.compare(b); } };

template <>
    struct comparator<const char*>
        { memint operator() (const char* a, const char* b) { return strcmp(a, b); } };


// Vector template for POD elements (int, pointers, et al). Used internally
// by the compiler itself. Also podvec is a basis for the universal vector.
// This hopefully generates minimal static code.

template <class T>
class podvec: protected bytevec
{
    friend void test_podvec();

protected:
    enum { Tsize = int(sizeof(T)) };
    typedef bytevec parent;

public:
    podvec() throw(): parent()              { }
    podvec(const podvec& v) throw(): parent(v) { }

    bool empty() const                      { return parent::empty(); }
    memint size() const                     { return parent::size() / Tsize; }
    bool operator== (const podvec& v) const { return parent::operator==(v); }
    const T& operator[] (memint i) const    { return *parent::data<T>(i); }
    const T& at(memint i) const             { return *parent::at<T>(i); }
    T& atw(memint i)                        { return *parent::atw<T>(i); }
    const T& back() const                   { return *parent::back<T>(); }
    const T& back(memint i) const           { return *parent::back<T>(i); }
    T& backw()                              { return *parent::backw<T>(); }
    T& backw(memint i)                      { return *parent::backw<T>(i); }
    const T* begin() const                  { return parent::begin<T>(); }
    const T* end() const                    { return parent::end<T>(); }
    void clear()                            { parent::clear(); }
    void operator= (const podvec& v) throw() { parent::operator= (v); }
    void push_back(const T& t)              { new(_append(Tsize, container::allocate)) T(t); }  // (*)
    void pop_back()                         { parent::pop_back<T>(); }
    void pop_back(T& t)                     { parent::pop_back<T>(t); }
    void append(const podvec& v)            { parent::append(v); }
    void insert(memint pos, const T& t)     { new(parent::_insert(pos * Tsize, Tsize, container::allocate)) T(t); }  // (*)
    void insert(memint pos, const podvec& v) { parent::_insert(pos * Tsize, v, container::allocate); }  // (*)
    void replace(memint pos, const T& t)    { *parent::atw<T>(pos) = t; }
    void erase(memint pos, memint len)      { parent::_erase(pos * Tsize, len * Tsize); }
    void erase(memint pos)                  { parent::_erase(pos * Tsize, Tsize); }

    // If you keep the vector sorted, the following will provide set-like
    // functionality:
    bool find(const T& item) const
    {
        memint index;
        return bsearch(item, index);
    }

    bool find_insert(const T& item)  // (*)
    {
        memint index;
        if (!bsearch(item, index))
        {
            insert(index, item);
            return true;
        }
        else
            return false;
    }

    void find_erase(const T& item)
    {
        memint index;
        if (bsearch(item, index))
            erase(index);
        else
            container::keyerr();
    }

    // Internal method, but should be public for technical reasons
    bool bsearch(const T& elem, memint& idx) const
    {
        comparator<T> comp;
        idx = 0;
        memint low = 0;
        memint high = size() - 1;
        while (low <= high) 
        {
            idx = (low + high) / 2;
            memint c = comp(operator[](idx), elem);
            if (c < 0)
                low = idx + 1;
            else if (c > 0)
                high = idx - 1;
            else
                return true;
        }
        idx = low;
        return false;
    }
};


// --- vector -------------------------------------------------------------- //


template <class T>
class vector: public podvec<T>
{
protected:
    enum { Tsize = int(sizeof(T)) };
    typedef podvec<T> parent;
    typedef T* Tptr;
    typedef Tptr& Tref;

    class cont: public container
    {
    protected:

        void finalize(void* p, memint len) throw()
        {
            (char*&)p += len - Tsize;
            for ( ; len; len -= Tsize, Tref(p)--)
                Tptr(p)->~T();
        }

        void copy(void* dest, const void* src, memint len) throw()
        {
            for ( ; len; len -= Tsize, Tref(dest)++, Tref(src)++)
                new(dest) T(*Tptr(src));
        }

        cont(memint cap, memint siz) throw(): container(cap, siz)  { }

    public:
        static container* allocate(memint cap, memint siz) throw()
            { return new(cap) cont(cap, siz); }

        ~cont() throw()
            { if (_size) { finalize(data(), _size); _size = 0; } }
    };

    vector(const vector& v, memint pos, memint len)
        { parent::_init(v, pos * Tsize, len * Tsize, cont::allocate); }

public:
    vector() throw(): parent()  { }

    // Override stuff that requires allocation of 'vector::cont'
    void insert(memint pos, const T& t)
        { new(bytevec::_insert(pos * Tsize, Tsize, cont::allocate)) T(t); }
    void insert(memint pos, const vector& v)
        { bytevec::_insert(pos * Tsize, v, cont::allocate); }
    void push_back(const T& t)
        { new(bytevec::_append(Tsize, cont::allocate)) T(t); }
    void resize(memint); // not implemented

    void replace(memint pos, memint len, const vector& v)
    {
        parent::erase(pos, len);
        insert(pos, v);
    }

    void grow(memint extra_items)
    {
        memint extra_mem = extra_items * Tsize;
        char* p = bytevec::_resize(bytevec::size() + extra_mem, cont::allocate);
        memset(p, 0, extra_mem);
    }

    vector subvec(memint pos, memint len) const
    {
        if (pos == 0 && len == parent::size())
            return *this;
        return vector(*this, pos, len);
    }

    // Give a chance to alternative constructors, e.g. str can be constructed
    // from (const char*). Without these templates below temp objects are
    // created and then copied into the vector. Though these are somewhat
    // dangerous too.
    template <class U>
        void insert(memint pos, const U& u)
            { new(bytevec::_insert(pos * Tsize, Tsize, cont::allocate)) T(u); }
    template <class U>
        void push_back(const U& u)
            { new(bytevec::_append(Tsize, cont::allocate)) T(u); }
    template <class U>
        void replace(memint i, const U& u)
            { parent::atw(i) = u; }

    bool find_insert(const T& item)
    {
        if (parent::empty())
            { push_back(item); return true; }
        else
            return parent::find_insert(item);
    }
};


// This is a clone of vector<> but declared separately for overloaded variant
// constructors. (Is there a better way?)
template <class T>
class set: public vector<T>
{
protected:
    enum { Tsize = int(sizeof(T)) };
    typedef vector<T> parent;
    typedef T* Tptr;
    typedef Tptr& Tref;
public:
    set() throw(): parent()  { }
};


// --- dict ---------------------------------------------------------------- //

// dict: internally a dict variable is a pointer to dictobj which in its turn
// contains two separate vectors for keys and for values. This way we 
// (1) re-use the existing instances of certain templates
// (2) more importantly, we simplify methods of getting the keys or values 
//     as vectors and reusing them on the ref-count basis

template <class Tkey, class Tval>
class dict
{
    friend class variant;

protected:

    void chkidx(memint i) const     { if (umemint(i) >= umemint(size())) container::idxerr(); }

    class dictobj: public object
    {
    public:
        vector<Tkey> keys;
        vector<Tval> values;
        dictobj(): keys(), values()  { }
        dictobj(const dictobj& d): object(), keys(d.keys), values(d.values)  { }
    };

    objptr<dictobj> obj;

    void _mkunique()
        { if (!obj.empty() && !obj.isunique()) obj = new dictobj(*obj); }

    bool _bsearch(const Tkey& k, memint& i) const
        { i = 0; return !empty() && obj->keys.bsearch(k, i); }

public:
    dict() throw()                          : obj()  { }
    dict(const dict& d) throw()             : obj(d.obj)  { }
    ~dict() throw()                         { }

    dict(const Tkey& k, const Tval& v) throw()
        : obj(new dictobj())
    {
        obj->keys.push_back(k);
        obj->values.push_back(v);
    }

    bool empty() const                      { return obj.empty(); }
    memint size() const                     { return !empty() ? obj->keys.size() : 0; }
    bool operator== (const dict& d) const   { return obj == d.obj; }
    bool operator!= (const dict& d) const   { return obj != d.obj; }

    void clear()                            { obj.clear(); }
    void operator= (const dict& d)          { obj = d.obj; }

    const Tkey& key(memint i) const         { chkidx(i); return obj->keys[i];  }
    const Tval& value(memint i) const       { chkidx(i); return obj->values[i];  }

    const vector<Tkey>& keys() const        { return empty() ? vector<Tkey>() : obj->keys; }
    const vector<Tval>& values() const      { return empty() ? vector<Tval>() : obj->values; }

    void replace(memint i, const Tval& v)
    {
        chkidx(i);
        _mkunique();
        obj->values.replace(i, v);
    }

    void erase(memint i)
    {
        chkidx(i);
        _mkunique();
        obj->keys.erase(i);
        obj->values.erase(i);
        if (obj->keys.empty())
            clear();
    }

    const Tval* find(const Tkey& k) const
    {
        memint i;
        if (_bsearch(k, i))
            return &obj->values[i];
        else
            return NULL;
    }

    bool find_key(const Tkey& k) const
        { memint i; return _bsearch(k, i); }

    void find_replace(const Tkey& k, const Tval& v)
    {
        memint i;
        if (!_bsearch(k, i))
        {
            if (empty())
                obj = new dictobj();
            else
                _mkunique();
            obj->keys.insert(i, k);
            obj->values.insert(i, v);
        }
        else
            replace(i, v);
        assert(obj->keys.size() == obj->values.size());
    }

    void find_erase(const Tkey& k)
    {
        memint i;
        if (_bsearch(k, i))
            erase(i);
        else
            container::keyerr();
    }

#ifdef DEBUG
    // for unit tests only
    struct item_type
    {
        const Tkey& key;
        Tval& value;
        item_type(const Tkey& k, Tval& v): key(k), value(v)  { }
    };

    item_type at(memint i) const
        { chkidx(i); return item_type(obj->keys[i], obj->values.atw(i)); }
#endif
};


// --- ordset -------------------------------------------------------------- //


class ordset
{
    friend class variant;
protected:
    static charset empty_charset;
    struct setobj: public object
    {
        charset set;
        setobj(): set()  { }
        setobj(const setobj& s): object(), set(s.set)  { }
    };
    objptr<setobj> obj;
    charset& _getunique();
public:
    ordset() throw()                        : obj()  { }
    ordset(const ordset& s) throw()         : obj(s.obj)  { }
    ordset(integer v) throw();
    ordset(integer l, integer r) throw();
    ~ordset() throw()                       { }
    bool empty() const                      { return obj.empty() || obj->set.empty(); }
    memint compare(const ordset& s) const;
    bool operator== (const ordset& s) const { return compare(s) == 0; }
    bool operator!= (const ordset& s) const { return compare(s) != 0; }
    void clear()                            { obj.clear(); }
    void operator= (const ordset& s)        { obj = s.obj; }
    bool find(integer v) const              { return !obj.empty() && obj->set[int(v)]; }
    void find_insert(integer v);
    void find_insert(integer l, integer h);
    void find_erase(integer v);
    const charset& get_charset() const      { return obj.empty() ? empty_charset : obj->set; }
};


// --- range --------------------------------------------------------------- //


class range
{
    friend class variant;
protected:
    struct rangeobj: public object
    {
        integer left;
        integer right;
        rangeobj(integer l, integer r): left(l), right(r)  { }
    };
    objptr<rangeobj> obj;
public:
    range(integer, integer) throw();
    ~range() throw();
    bool empty() const
        { return obj.empty(); }
    integer left() const
        { return obj->left; }
    integer right() const
        { return obj->right; }
    bool contains(integer v)
        { return !obj.empty() && (v >= obj->left && v <= obj->right); }
    memint compare(const range& r) const;
    bool operator ==(const range& r) const;
};


// --- object collections -------------------------------------------------- //


// extern template class podvec<object*>;


class objvec_impl: public podvec<object*>
{
protected:
    typedef podvec<object*> parent;
public:
    objvec_impl() throw(): parent()  { }
    objvec_impl(const objvec_impl& s) throw(): parent(s)  { }
    void release_all() throw();
};


template <class T>
class objvec: public objvec_impl
{
protected:
    typedef objvec_impl parent;
public:
    objvec() throw(): parent()              { }
    objvec(const objvec& s) throw(): parent(s) { }
    T* operator[] (memint i) const          { return cast<T*>(parent::operator[](i)); }
    T* at(memint i) const                   { return cast<T*>(parent::at(i)); }
    T* back() const                         { return cast<T*>(parent::back()); }
    T* back(memint i) const                 { return cast<T*>(parent::back(i)); }
    T* push_back(T* t)                      { parent::push_back(t); return t; }
    void insert(memint pos, T* t)           { parent::insert(pos, t); }
    void replace(memint pos, T* t)          { parent::replace(pos, t); }
};


class symbol: public object
{
public:
    str const name;
    symbol(const str& s) throw(): name(s)  { }
    symbol(const char* s) throw(): name(s)  { }
    ~symbol() throw();
};


class symtbl_impl: public objvec<symbol>
{
protected:
    typedef objvec<symbol> parent;
    bool bsearch(const str& key, memint& index) const;
public:
    symtbl_impl() throw(): parent()  { }
    symtbl_impl(const symtbl_impl& s) throw();
    symbol* find(const str& name) const; // NULL or symbol*
    bool add(symbol*);
    bool replace(symbol*);
};


template <class T>
class symtbl: public symtbl_impl
{
protected:
    typedef symtbl_impl parent;
public:
    symtbl() throw(): parent()          { }
    memint size() const                 { return parent::size(); }
    T* find(const str& name) const      { return cast<T*>(parent::find(name)); }
    bool add(T* t)                      { return parent::add(t); }
    bool replace(T* t)                  { return parent::replace(t); }
    void release_all() throw()          { parent::release_all(); }
};


// --- Exceptions ---------------------------------------------------------- //


// For dynamically generated strings
class emessage: public exception
{
public:
    str msg;
    emessage(const emessage&) throw(); // not defined
    emessage(const str& _msg) throw();
    emessage(const char* _msg) throw();
    ~emessage() throw();
    const char* what() throw();
};


// TODO: define these as separate classes
typedef emessage econtainer;
typedef emessage evariant;
typedef emessage efifo;


// UNIX system errors
class esyserr: public emessage
{
public:
    esyserr(int icode, const str& iArg = "") throw();
    ~esyserr() throw();
};


void nullptrerr();

template <class T>
    inline T* CHKPTR(T* p)
        { if (p == NULL) nullptrerr(); return p; }


// --- variant ------------------------------------------------------------- //


class variant;
class reference;
class funcptr;
class stateobj;
struct podvar;

typedef vector<variant> varvec;
typedef set<variant> varset;
typedef dict<variant, variant> vardict;


class variant
{
    friend void test_variant();
    friend void initRuntime();

private:
    void _init(void*);   // compiler traps
    void _init(const void*);
    void _init(object*);
    void _init(bool);

public:
    enum Type
        { VOID, ORD, REAL, VARPTR,
          STR, RANGE, VEC, SET, ORDSET, DICT, REF, RTOBJ,
          ANYOBJ = STR };

    struct _Void { int dummy; }; 
    static _Void null;

protected:
    Type type;
    union _val_union
    {
        integer     _all;       // should be the biggest in this union
        integer     _ord;       // int, char and bool
        real        _real;      // not implemented in the VM yet
        variant*    _ptr;       // POD pointer to a variant
        object*     _obj;       // str, vector, set, map and their variants
        reference*  _ref;       // reference object
        rtobject*   _rtobj;     // runtime objects with the "type" field
    } val;

    void _req(Type t) const             { if (type != t) _type_err(); }
    void _req_anyobj() const            { if (!is_anyobj()) _type_err(); }
#ifdef DEBUG
    void _dbg(Type t) const             { _req(t); }
    void _dbg_anyobj() const            { _req_anyobj(); }
#else
    void _dbg(Type) const               { }
    void _dbg_anyobj() const            { }
#endif
    void _init() throw()                { type = VOID; val._all = 0; }
    void _init(_Void) throw()           { _init(); }
    void _init(Type t) throw()          { type = t; val._all = 0; }
    void _init(char v) throw()          { type = ORD; val._ord = uchar(v); }
    void _init(uchar v) throw()         { type = ORD; val._ord = v; }
    void _init(int v) throw()           { type = ORD; val._ord = v; }
#ifdef SHN_64
    void _init(large v) throw()         { type = ORD; val._ord = v; }
#endif
    void _init(real v) throw()          { type = REAL; val._real = v; }
    void _init(variant* v) throw()      { type = VARPTR; val._ptr = v; }
    void _init(Type t, object* o) throw() { type = t; val._obj = o; if (o) o->grab(); }
    void _init(const str& v) throw()    { _init(STR, v.obj); }
    void _init(const char* s) throw()   { type = STR; ::new(&val._obj) str(s); }
    void _init(const range& v) throw()  { _init(RANGE, v.obj); }
    void _init(integer l, integer r) throw() { type = RANGE; ::new(&val._obj) range(l, r); }
    void _init(const varvec& v) throw() { _init(VEC, v.obj); }
    void _init(const varset& v) throw() { _init(SET, v.obj); }
    void _init(const ordset& v) throw() { _init(ORDSET, v.obj); }
    void _init(const vardict& v) throw() { _init(DICT, v.obj); }
    void _init(reference* o) throw();
    void _init(rtobject* o) throw()     { _init(RTOBJ, o); }
    void _init(fifo* o) throw();
    void _init(stateobj* o) throw();
    void _init(const variant& v) throw();
    void _init(const podvar* v) throw();

    void _fin() throw()                 { if (is_anyobj()) val._obj->release(); }

public:
    variant() throw()                   { _init(); }
    variant(Type t) throw()             { _init(t); }
    variant(integer l, integer r) throw() { _init(l, r); }
    variant(const variant& v) throw()   { _init(v); }
    template <class T>
        variant(const T& v) throw()     { _init(v); }
    variant(Type t, object* o) throw()  { _init(t, o); }
    ~variant() throw()                  { _fin(); }

    template <class T>
        void operator= (const T& v) throw() { _fin(); _init(v); }
    void operator= (const variant& v) throw();
    void clear() throw()                { _fin(); _init(); }
    bool empty() const;

    memint compare(const variant&) const;
    bool operator== (const variant&) const;
    bool operator!= (const variant& v) const { return !(operator==(v)); }

    Type getType() const                { return Type(type); }
    bool is(Type t) const               { return type == t; }
    bool is_str() const                 { return type == STR; }
    bool is_vec() const                 { return type == VEC; }
    bool is_null() const                { return type == VOID; }
    bool is_anyobj() const throw()      { return type >= ANYOBJ; }
    bool is_null_obj() const            { return is_anyobj() && val._obj == NULL; }

    // Fast "unsafe" access methods; checked for correctness only in DEBUG mode
    bool        _bool()           const { _dbg(ORD); return val._ord; }
    uchar       _uchar()          const { _dbg(ORD); return (uchar)val._ord; }
    integer     _int()            const { _dbg(ORD); return val._ord; }
    variant*    _ptr()            const { _dbg(VARPTR); return val._ptr; }
    const str&  _str()            const { _dbg(STR); return *(str*)&val._obj; }
    const range& _range()         const { _dbg(RANGE); return *(range*)&val._obj; }
    const varvec& _vec()          const { _dbg(VEC); return *(varvec*)&val._obj; }
    const varset& _set()          const { _dbg(SET); return *(varset*)&val._obj; }
    const ordset& _ordset()       const { _dbg(ORDSET); return *(ordset*)&val._obj; }
    const vardict& _dict()        const { _dbg(DICT); return *(vardict*)&val._obj; }
    reference*  _ref()            const { _dbg(REF); return CHKPTR(val._ref); }
    rtobject*   _rtobj()          const { _dbg(RTOBJ); return val._rtobj; }
    stateobj*   _stateobj() const;
    funcptr*    _funcptr() const;
    fifo*       _fifo() const; // checks for NULL
    object*     _anyobj()         const { _dbg_anyobj(); return val._obj; }
    integer&    _int()                  { _dbg(ORD); return val._ord; }
    str&        _str()                  { _dbg(STR); return *(str*)&val._obj; }
    range&      _range()                { _dbg(RANGE); return *(range*)&val._obj; }
    varvec&     _vec()                  { _dbg(VEC); return *(varvec*)&val._obj; }
    varset&     _set()                  { _dbg(SET); return *(varset*)&val._obj; }
    ordset&     _ordset()               { _dbg(ORDSET); return *(ordset*)&val._obj; }
    vardict&    _dict()                 { _dbg(DICT); return *(vardict*)&val._obj; }

    // Safer access methods; may throw
    bool        as_bool()         const { _req(ORD); return _bool(); }
    uchar       as_uchar()        const { _req(ORD); return _uchar(); }
    integer     as_ord()          const { _req(ORD); return _int(); }
    variant*    as_ptr()          const { _req(VARPTR); return val._ptr; }
    const str&  as_str()          const { _req(STR); return _str(); }
    const range&  as_range()      const { _req(RANGE); return _range(); }
    const varvec& as_vec()        const { _req(VEC); return _vec(); }
    const varset& as_set()        const { _req(SET); return _set(); }
    const ordset& as_ordset()     const { _req(ORDSET); return _ordset(); }
    const vardict& as_dict()      const { _req(DICT); return _dict(); }
    reference*  as_ref()          const { _req(REF); return val._ref; }
    rtobject*   as_rtobj()        const { _req(RTOBJ); return _rtobj(); }
    object*     as_anyobj()       const { _req_anyobj(); return val._obj; }
    integer&    as_ord()                { _req(ORD); return _int(); }
    str&        as_str()                { _req(STR); return _str(); }
    range&      as_range()              { _req(RANGE); return _range(); }
    varvec&     as_vec()                { _req(VEC); return _vec(); }
    varset&     as_set()                { _req(SET); return _set(); }
    ordset&     as_ordset()             { _req(ORDSET); return _ordset(); }
    vardict&    as_dict()               { _req(DICT); return _dict(); }

    static void _type_err();
    static void _range_err();
};


#ifdef SHN_FASTER
inline void variant::_init(const variant& v) throw()
{
    type = v.type;
    val = v.val;
    if (is_anyobj() && val._obj)
        val._obj->grab();
}


inline void variant::operator= (const variant& v) throw()
{
    if (type != v.type || val._all != v.val._all)
        { _fin(); _init(v); }
}
#endif


struct podvar { char data[sizeof(variant)]; };

inline void variant::_init(const podvar* v) throw()
    { *(podvar*)this = *v; }

template <>
    struct comparator<variant>
        { memint operator() (const variant& a, const variant& b) { return a.compare(b); } };

/*
extern template class vector<variant>;
extern template class set<variant>;
extern template class dict<variant, variant>;
extern template class podvec<variant>;
*/

// --- runtime objects ----------------------------------------------------- //


// reference: is a ref-counted object that encapsulates a variant; this way a
// variant can be shared between multiple other variables and mimic
// "references" as commonly defined in other languages

class reference: public object
{
public:
    variant var;
    reference() throw() { }
    reference(const variant& v) throw(): var(v)  { }
    reference(const podvar* v) throw(): var(v)  { }
    ~reference() throw();
};


inline void variant::_init(reference* o) throw() { _init(REF, o); }


class State;  // defined in typesys.h
class CodeSeg;  // defined in vm.h


// sateobj: a run-time ref-counted object, actually a structure with variant
// member fields. The actual size is passed to operator new() defined below.

class stateobj: public rtobject
{
    friend class State;
    typedef rtobject parent;
    friend void runRabbitRun(variant*, stateobj*, stateobj*, variant*, CodeSeg*);
    
protected:
#ifdef DEBUG
    memint varcount;
    static void idxerr();
#endif
    stateobj(State*) throw(); // defined in typesys.h as an inline function

    // Get zeroed memory so that the destructor works correctly even if the
    // constructor failed in the middle. A zeroed variant is a null variant.
    void* operator new(size_t s, memint extra)
    {
#ifdef DEBUG
        pincrement(&object::allocated);
#endif
        return pmemcalloc(s + extra * sizeof(variant));
    }

    // In place operator new for stateobj: for creating pseudo-objects on the stack
    void* operator new(size_t, void* p)
    {
#ifdef DEBUG
        pincrement(&object::allocated);
#endif
        // memset(pchar(p) + s, 0, extra * sizeof(variant));
        return p;
    }

public:
    ~stateobj() throw();
    State* getType() const  { return (State*)parent::getType(); }

    bool empty() const;  // override
    void dump(fifo&) const;  // override

    variant* member(memint index)
    {
#ifdef DEBUG
        if (umemint(index) >= umemint(varcount))
            idxerr();
#endif
        return (variant*)(this + 1) + index;
    }

    void collapse();
};


inline void variant::_init(stateobj* o) throw() { _init(RTOBJ, o); }
inline stateobj* variant::_stateobj() const { return cast<stateobj*>(_rtobj()); }


class funcptr: public rtobject
{
public:
    stateobj* dataseg;
    objptr<stateobj> outer;
    State* state;
    funcptr(stateobj* dataseg, stateobj* outer, State* state) throw();
    ~funcptr() throw();
    bool empty() const;
    void dump(fifo&) const;
};

inline funcptr* variant::_funcptr() const  { return cast<funcptr*>(_rtobj()); }


class rtstack: protected bytevec
{
public:
    rtstack(memint maxSize) throw();
    variant* base()
        { return (variant*)begin(); }
};


// --- FIFO ---------------------------------------------------------------- //


const int _varsize = int(sizeof(variant));


// The abstract FIFO interface. There are 2 modes of operation: variant FIFO
// and character FIFO. Destruction of variants is basically not handled by
// this class to give more flexibility to implementations (e.g. there may be
// buffers shared between 2 fifos or other container objects). If you implement,
// say, only input methods, the default output methods will throw an exception
// with a message "FIFO is read-only", and vice versa. Iterators may be
// implemented in descendant classes but are not supported by default.
// Powerful text parsing methods are provided that work on any derived FIFO
// implementation (see "Characetr FIFO operations" below).
class fifo: public rtobject
{
    friend void runRabbitRun(variant*, stateobj*, stateobj*, variant*, CodeSeg*);

    fifo& operator<< (bool);   // compiler traps
    fifo& operator<< (void*);
    fifo& operator<< (object*);
    fifo& operator<< (rtobject* o); //      { o->dump(*this); return *this; }

protected:
    memint max_token;  // 4096
    bool _is_char_fifo;

    static void _empty_err();
    static void _full_err();
    static void _wronly_err();
    static void _rdonly_err();
    static void _fifo_type_err();
    static void _token_err();
    void _req(bool req_char) const      { if (req_char != _is_char_fifo) _fifo_type_err(); }
    void _req_non_empty() const;
    void _req_non_empty(bool _char) const;

    // Minimal set of methods required for both character and variant FIFO
    // operations. Implementations should guarantee variants will never be
    // fragmented, so that a buffer returned by get_tail() always contains at
    // least sizeof(variant) bytes (8, 12 or 16 bytes depending on the config.
    // and platform) in variant mode, or at least 1 byte in character mode.
    virtual const char* get_tail();          // Get a pointer to tail data
    virtual const char* get_tail(memint*);   // ... also return the length
    virtual void deq_bytes(memint);          // Discard n consecutive bytes returned by get_tail()
    virtual variant* enq_var();              // Reserve uninitialized space for a variant
    virtual void enq_char(char);             // Push one char, char fifo only
    virtual memint enq_chars(const char*, memint); // Push arbitrary number of bytes, return actual number, char fifo only

    void _token(const charset& chars, str* result);
    void deq_var(variant*);  // dequeue variant to uninitialized area, for internal use

public:
    fifo(Type*, bool is_char) throw();
    ~fifo() throw();

    enum { CHAR_ALL = MEMINT_MAX - 2, CHAR_SOME = MEMINT_MAX - 1 };

    void dump(fifo&) const;

    bool empty() const;                 // override, throws
    virtual void flush();               // empty, overridden in file fifos
    virtual str get_name() const = 0;

    // Main FIFO operations, work on both char and variant fifos; for char
    // fifos the variant is read as either a char or a string.
    void var_enq(const variant&);
    void var_preview(variant&);
    void var_deq(variant&);
    void var_eat();

    // Character FIFO operations
    bool is_char_fifo() const           { return _is_char_fifo; }
    int preview(); // returns -1 on eof
    char look()
        { int c = preview(); if (c == -1) _empty_err(); return c; }
    uchar get();
    bool get_if(char c);
    str  deq(memint);  // CHAR_ALL, CHAR_SOME can be specified
    str  deq(const charset& c)          { str s; _token(c, &s); return s; }
    str  token(const charset& c)        { return deq(c); } // alias
    void eat(const charset& c)          { _token(c, NULL); }
    void skip(const charset& c)         { eat(c); } // alias
    str  line();
    bool eol();
    void skip_eol();
    bool eof() const                    { return empty(); }

    memint enq(const char* p, memint count)  { return enq_chars(p, count); }
    void enq(const char* s);
    void enq(const str& s);
    void enq(char c)                    { enq_char(c); }
    void enq(uchar c)                   { enq_char(c); }
    void enq(large i);
    void enq(const varvec&);

    fifo& operator<< (const char* s)    { enq(s); return *this; }
    fifo& operator<< (const str& s)     { enq(s); return *this; }
    fifo& operator<< (char c)           { enq(c); return *this; }
    fifo& operator<< (uchar c)          { enq(c); return *this; }
    fifo& operator<< (large i)          { enq(large(i)); return *this; }
    fifo& operator<< (int i)            { enq(large(i)); return *this; }
    fifo& operator<< (long i)           { enq(large(i)); return *this; }
    fifo& operator<< (size_t i)         { enq(large(i)); return *this; }
};

const char endl = '\n';
extern charset non_eol_chars;

inline void variant::_init(fifo* f) throw() { _init(RTOBJ, f); }
inline fifo* variant::_fifo() const  { return cast<fifo*>(CHKPTR(_rtobj())); }


// The memfifo class implements a linked list of "chunks" in memory, where
// each chunk is the size of 32 * sizeof(variant). Both enqueue and deqeue
// operations are O(1), and memory usage is better than that of a plain linked 
// list of elements, as "next" pointers are kept for bigger chunks of elements
// rather than for each element. Can be used both for variants and chars. This
// class "owns" variants, i.e. proper construction and desrtuction is done.
class memfifo: public fifo
{
public:
#ifdef DEBUG
    static int CHUNK_SIZE; // settable from unit tests
#else
    enum { CHUNK_SIZE = 32 * _varsize };
#endif

protected:
    struct chunk: noncopyable
    {
        chunk* next;
        char data[0];
#ifdef DEBUG
        chunk() throw(): next(NULL)     { pincrement(&object::allocated); }
        ~chunk() throw()                { pdecrement(&object::allocated); }
#else
        chunk() throw(): next(NULL)     { }
#endif
        void* operator new(size_t)      { return ::pmemalloc(sizeof(chunk) + CHUNK_SIZE); }
        void operator delete(void* p)   { ::pmemfree(p); }
    };

    chunk* head;    // in
    chunk* tail;    // out
    int head_offs;
    int tail_offs;

    void enq_chunk();
    void deq_chunk();

    // Overrides
    const char* get_tail();
    const char* get_tail(memint*);
    void deq_bytes(memint);
    variant* enq_var();
    void enq_char(char);
    memint enq_chars(const char*, memint);

    char* enq_space(memint);
    memint enq_avail();

public:
    memfifo(Type*, bool is_char) throw();
    ~memfifo() throw();

    void clear();
    bool empty() const;     // override
    str get_name() const;   // override
};


// Buffer read event handler (write events aren't implemented yet)
class bufevent: public object
{
public:
    virtual void event(char* buf, memint tail, memint head) = 0;
};


// This is an abstract buffered fifo class. Implementations should validate the
// buffer in the overridden empty() and flush() methods, for input and output
// fifos respectively. To simplify things, buffifo objects are not supposed to
// be reusable, i.e. once the end of file is reached, the implementation is not
// required to reset its state. Variant fifo implementations should guarantee
// at least sizeof(variant) bytes in calls to get_tail() and enq_var().
class buffifo: public fifo
{
protected:
    char*  buffer;
    memint bufsize;
    memint bufhead;
    memint buftail;
    memint buforig;

    bufevent* event;

    const char* get_tail();
    const char* get_tail(memint*);
    void deq_bytes(memint);
    variant* enq_var();
    void enq_char(char);
    memint enq_chars(const char*, memint);

    char* enq_space(memint);
    memint enq_avail();
    
    void call_bufevent() const;

public:
    buffifo(Type*, bool is_char) throw();
    ~buffifo() throw();

    bool empty() const; // throws efifowronly
    void flush(); // throws efifordonly

    memint tellg() const { return buforig + buftail; }
    memint tellp() const { return buforig + bufhead; }
    bufevent* set_bufevent(bufevent*);
};


// Analog of std::strstream; a buffifo-based implementation that uses
// a 'str' object for storing data.
class strfifo: public buffifo
{
protected:
    str string;
    void clear();
public:
    strfifo(Type*) throw();
    strfifo(Type*, const str&) throw();
    ~strfifo() throw();
    bool empty() const;     // override
    void flush();           // override
    str get_name() const;   // override
    str all() const;
};


// TODO: varfifo, a variant vector wrapper based on buffifo

class intext: public buffifo
{
public:
#ifdef DEBUG
    static int BUF_SIZE; // settable from unit tests
#else
    enum { BUF_SIZE = 4096 * sizeof(integer) };
#endif

protected:
    str file_name;
    str  filebuf;
    int  _fd;
    bool _eof;

    void error(int code); // throws esyserr
    void doopen();
    void doread();

public:
    intext(Type*, const str& fn) throw();
    ~intext() throw();

    bool empty() const;     // override
    str get_name() const;   // override
    void open()             { empty(); /* attempt to open */ }
};


class outtext: public buffifo
{
protected:
    enum { BUF_SIZE = 2048 * sizeof(integer) };

    str file_name;
    str  filebuf;
    int  _fd;
    bool _err;

    void error(int code); // throws esyserr

public:
    outtext(Type*, const str& fn) throw();
    ~outtext() throw();

    void flush();           // override
    str get_name() const;   // override
    void open()             { flush(); /* attempt to open */ }
};


// Standard input/output object, a two-way fifo. In case of stderr it is write-only.
class stdfile: public intext
{
protected:
    int _ofd;
    void enq_char(char);
    memint enq_chars(const char*, memint);
public:
    stdfile(int infd, int outfd) throw();
    ~stdfile() throw();
};


extern stdfile sio;
extern stdfile serr;


// System utilities


bool isFile(const char*);


// ------------------------------------------------------------------------- //


typedef vector<str> strvec;
// extern template class vector<str>;


void initRuntime();
void doneRuntime();


#endif // __RUNTIME_H
