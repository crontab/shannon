#ifndef __RUNTIME_H
#define __RUNTIME_H


#include "common.h"


// --- charset ------------------------------------------------------------- //


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
    object(const object&);
    void operator= (const object&);

protected:
    atomicint _refcount;

public:

    void* operator new(size_t self);
    void* operator new(size_t self, memint extra);
    void  operator delete(void*);
    
    // Dirty trick that duplicates an object and (hopefully) preserves the
    // dynamic type (actually the VMT). Only 'self' bytes is copied; 'extra'
    // remains uninitialized.
    object* dup(size_t self, memint extra);

    void _assignto(object*& p);
    static object* reallocate(object* p, size_t self, memint extra);

    static atomicint allocated; // used only in DEBUG mode

    bool unique() const         { return _refcount == 1; }
    void release();
    object* grab()              { pincrement(&_refcount); return this; }
    template <class T>
        T* grab()               { object::grab(); return (T*)(this); }
    template <class T>
        void assignto(T*& p)    { _assignto((object*&)p); }

    object(): _refcount(0)  { }
    virtual ~object();
};


// objptr: "smart" pointer

template <class T>
class objptr
{
protected:
    T* obj;
public:
    objptr()                            : obj(NULL) { }
    objptr(const objptr& p)             : obj(p.obj) { if (obj) obj->grab(); }
    objptr(T* o)                        : obj(o) { if (obj) obj->grab(); }
    ~objptr()                           { if (obj) obj->release(); }
    void clear()                        { if (obj) { obj->release(); obj = NULL; } }
    bool empty() const                  { return obj == NULL; }
    bool unique() const                 { return empty() || obj->unique(); }
    bool operator== (const objptr& p)   { return obj == p.obj; }
    bool operator!= (const objptr& p)   { return obj != p.obj; }
    void operator= (const objptr& p)    { p.obj->assignto(obj); }
    void operator= (T* o)               { o->assignto(obj); }
    T& operator* ()                     { return *obj; }
    const T& operator* () const         { return *obj; }
    T* operator-> () const              { return obj; }
    operator T*() const                 { return obj; }
    T* get() const                      { return obj; }
};


class Type;    // defined in typesys.h

class rtobject: public object
{
    Type* _type;
protected:
public:
    rtobject(Type* t) throw(): _type(t)  { }
    ~rtobject() throw();
    Type* getType() const   { return _type; }
    void setType(Type* t)   { assert(_type == NULL); _type = t; }
    void clearType()        { _type = NULL; }
    virtual bool empty() const = 0;
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
    static container* allocate(memint cap, memint siz);  // (*)
    static container* reallocate(container* p, memint newsize);

    // Creates a duplicate of a given container without copying the data
    container* dup(memint cap, memint siz);

    // TODO: compact()

    static memint _calc_prealloc(memint);
    container(memint cap, memint siz)
        : object(), _capacity(cap), _size(siz)  { }

    static void overflow();
    static void idxerr();

    ~container();
    virtual void finalize(void*, memint);
    virtual void copy(void* dest, const void* src, memint);

    char* data() const              { return (char*)(this + 1); }
    char* end() const               { return data() + _size; }
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
    friend void test_bytevec();
    friend void test_podvec();

protected:
    char* _data;

    typedef container* (*alloc_func)(memint cap, memint siz);

    void chkidx(memint i) const     { if (umemint(i) >= umemint(size())) container::idxerr(); }
    void chkidxa(memint i) const    { if (umemint(i) > umemint(size())) container::idxerr(); }
    static void chknonneg(memint v) { if (v < 0) container::overflow(); } 
    void chknz() const              { if (empty()) container::idxerr(); }
    container* _cont() const        { return container::cont(_data); }
    bool _unique() const            { return !_data || _cont()->unique(); }
    char* _mkunique();
    void _init()                    { _data = NULL; }
    char* _init(memint len);
    void _init(memint len, char fill);
    void _init(const char*, memint);  // (*)
    void _init(const bytevec& v);
    void _fin()                         { if (_data) _cont()->release(); }
    void _assign(container* c)          { _fin(); _data = c->data(); c->grab(); }

    char* _insert(memint pos, memint len, alloc_func);
    char* _append(memint len, alloc_func);
    void _erase(memint pos, memint len);
    void _pop(memint len);
    char* _resize(memint newsize, alloc_func);

    bytevec(container* c)               { _init(c); }

public:
    bytevec()                           { _init(); }
    bytevec(const bytevec& v)           { _init(v); }
    bytevec(const char* buf, memint len)    { _init(buf, len); }  // (*)
    bytevec(memint len, char fill)      { _init(len, fill); }  // (*)
    ~bytevec()                          { _fin(); }

    void operator= (const bytevec& v);
    bool operator== (const bytevec& v) const { return _data == v._data; }
    void assign(const char*, memint);
    void clear();

    bool empty() const                  { return _data == NULL; }
    memint size() const                 { return _data ? _cont()->size() : 0; }
    memint capacity() const             { return _data ? _cont()->capacity() : 0; }
    const char* data() const            { return _data; }
    const char* data(memint i) const    { return _data + i; }
    const char* at(memint i) const      { chkidx(i); return data(i); }
    char* atw(memint i)                 { chkidx(i); return _mkunique() + i; }
    const char* end() const             { return _data + size(); }
    const char* back(memint i) const    { chkidxa(i); return end() - i; }
    const char* back() const            { return back(1); }

    void insert(memint pos, const char* buf, memint len);  // (*)
    void insert(memint pos, const bytevec& s);
    void append(const char* buf, memint len);  // (*)
    void append(const bytevec& s);
    void erase(memint pos, memint len);
    void pop_back(memint len)           { if (len > 0) _pop(len); }
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
        const T* begin() const          { return (T*)data(); }
    template <class T>
        const T* end() const            { return (T*)end(); }
    template <class T>
        const T* back() const           { return (T*)back(sizeof(T)); }
    template <class T>
        const T* back(memint i) const   { return (T*)back(sizeof(T) * i); }
};

// (*) -- works only with the POD container; should be overridden or hidden in
//        descendant classes. The rest works magically on any descendant of
//        'container'. Pure magic. I like this!


// --- str ----------------------------------------------------------------- //


class str: public bytevec
{
protected:
    friend void test_string();

    void _init(const char*);
    void _init(char c)                      { bytevec::_init(&c, 1); }

public:
    str(): bytevec()                        { }
    str(const str& s): bytevec(s)           { }
    str(const char* buf, memint len)        : bytevec(buf, len)  { }
    str(const char* s)                      { _init(s); }
    str(memint len, char fill)              { bytevec::_init(len, fill); }
    str(char c)                             { _init(c); }

    const char* c_str(); // can actually modify the object
    void push_back(char c)                  { *_append(1, container::allocate) = c; }
    char operator[] (memint i) const        { return *data(i); }
    char at(memint i) const                 { return *bytevec::at(i); }
    char back() const                       { return *bytevec::back(); }
    void replace(memint pos, char c)        { *bytevec::atw(pos) = c; }
    void operator= (const char* c);
    void operator= (char c);

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
    void insert(memint pos, const char* s);
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
    enum { Tsize = sizeof(T) };
    typedef bytevec parent;

    podvec(container* c): parent(c)  { }

public:
    podvec(): parent()                      { }
    podvec(const podvec& v): parent(v)      { }

    bool empty() const                      { return parent::empty(); }
    memint size() const                     { return parent::size() / Tsize; }
    bool operator== (const podvec& v) const { return parent::operator==(v); }
    const T& operator[] (memint i) const    { return *parent::data<T>(i); }
    const T& at(memint i) const             { return *parent::at<T>(i); }
    T& atw(memint i)                        { return *parent::atw<T>(i); }
    const T& back() const                   { return *parent::back<T>(); }
    const T& back(memint i) const           { return *parent::back<T>(i); }
    const T* begin() const                  { return parent::begin<T>(); }
    const T* end() const                    { return parent::end<T>(); }
    void clear()                            { parent::clear(); }
    void operator= (const podvec& v)        { parent::operator= (v); }
    void push_back(const T& t)              { new(_append(Tsize, container::allocate)) T(t); }  // (*)
    void pop_back()                         { parent::pop_back(Tsize); }
    void append(const podvec& v)            { parent::append(v); }
    void insert(memint pos, const T& t)     { new(_insert(pos * Tsize, Tsize, container::allocate)) T(t); }  // (*)
    void replace(memint pos, const T& t)    { *parent::atw<T>(pos) = t; }
    void erase(memint pos)                  { parent::_erase(pos * Tsize, Tsize); }

    // If you keep the vector sorted, the following will provide a set-like
    // functionality:
    bool has(const T& item) const
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
    }

    // Internal methods, but should be public for technical reasons
    bool bsearch(const T& elem, memint& index) const
        { return ::bsearch(*this, size() - 1, elem, index); }

    memint compare(memint i, const T& elem) const
        { comparator<T> comp; return comp(operator[](i), elem); }
};


// --- vector -------------------------------------------------------------- //


template <class T>
class vector: public podvec<T>
{
protected:
    enum { Tsize = sizeof(T) };
    typedef podvec<T> parent;
    typedef T* Tptr;
    typedef Tptr& Tref;

    class cont: public container
    {
    protected:

        void finalize(void* p, memint len)
        {
            (char*&)p += len - Tsize;
            for ( ; len; len -= Tsize, Tref(p)--)
                Tptr(p)->~T();
        }

        void copy(void* dest, const void* src, memint len)
        {
            for ( ; len; len -= Tsize, Tref(dest)++, Tref(src)++)
                new(dest) T(*Tptr(src));
        }

        cont(memint cap, memint siz): container(cap, siz)  { }

    public:
        static container* allocate(memint cap, memint siz)
            { return new(cap) cont(cap, siz); }

        ~cont()
            { if (_size) { finalize(data(), _size); _size = 0; } }
    };

public:
    vector(): parent()  { }

    // Override stuff that requires allocation of 'vector::cont'
    void insert(memint pos, const T& t)
        { new(bytevec::_insert(pos * Tsize, Tsize, cont::allocate)) T(t); }
    void push_back(const T& t)
        { new(bytevec::_append(Tsize, cont::allocate)) T(t); }
    void resize(memint newsize)
        { notimpl(); /* bytevec::_resize(newsize, cont::allocate); */ }

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


// --- dict ---------------------------------------------------------------- //


template <class Tkey, class Tval>
class dict
{
protected:

    void chkidx(memint i) const     { if (umemint(i) >= umemint(size())) container::idxerr(); }

    class dictobj: public object
    {
    public:
        vector<Tkey> keys;
        vector<Tval> values;
        dictobj(): keys(), values()  { }
        dictobj(const dictobj& d): keys(d.keys), values(d.values)  { }
    };

    objptr<dictobj> obj;

    void _mkunique()
        { if (!obj.empty() && !obj.unique()) obj = new dictobj(*obj); }

public:
    dict()                                  : obj()  { }
    dict(const dict& d)                     : obj(d.obj)  { }
    ~dict()                                 { }

    bool empty() const                      { return obj.empty(); }
    memint size() const                     { return !empty() ? obj->keys.size() : 0; }
    bool operator== (const dict& d) const   { return obj == d.obj; }
    bool operator!= (const dict& d) const   { return obj != d.obj; }

    void clear()                            { obj.clear(); }
    void operator= (const dict& d)          { obj = d.obj; }

    const Tkey& key(memint i) const         { chkidx(i); return obj->keys[i];  }
    const Tval& value(memint i) const       { chkidx(i); return obj->values[i];  }

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

    struct item_type
    {
        const Tkey& key;
        Tval& value;
        item_type(const Tkey& k, Tval& v): key(k), value(v)  { }
    };

    item_type operator[] (memint i) const
        { chkidx(i); return item_type(obj->keys[i], obj->values.atw(i)); }

    const Tval* find(const Tkey& k) const
    {
        memint i;
        if (bsearch(k, i))
            return &obj->values[i];
        else
            return NULL;
    }

    void find_replace(const Tkey& k, const Tval& v)
    {
        memint i;
        if (!bsearch(k, i))
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
        { memint i; if (bsearch(k, i)) erase(i); }

    // These are public 
    memint compare(memint i, const Tkey& k) const
        { comparator<Tkey> comp; return comp(obj->keys[i], k); }

    bool bsearch(const Tkey& k, memint& i) const
        { return ::bsearch(*this, size() - 1, k, i); }
};


// --- ordset -------------------------------------------------------------- //


class ordset
{
protected:
    struct setobj: public object
    {
        charset set;
        setobj(): set()  { }
        setobj(const setobj& s): set(s.set)  { }
    };
    objptr<setobj> obj;
    charset& _getunique();
public:
    ordset()                                : obj()  { }
    ordset(const ordset& s)                 : obj(s.obj)  { }
    ~ordset()                               { }
    bool empty() const                      { return obj.empty() || obj->set.empty(); }
    memint compare(const ordset& s) const;
    bool operator== (const ordset& s) const { return compare(s) == 0; }
    bool operator!= (const ordset& s) const { return compare(s) != 0; }
    void clear()                            { obj.clear(); }
    void operator= (const ordset& s)        { obj = s.obj; }
    bool has(integer v) const               { return !obj.empty() && obj->set[int(v)]; }
    void insert(integer v);
    void insert(integer l, integer h);
    void erase(integer v);
};


// --- object collections -------------------------------------------------- //


// extern template class podvec<object*>;


class objvec_impl: public podvec<object*>
{
protected:
    typedef podvec<object*> parent;
public:
    objvec_impl(): parent()  { }
    objvec_impl(const objvec_impl& s): parent(s)  { }
    void release_all();
};


template <class T>
class objvec: public objvec_impl
{
protected:
    typedef objvec_impl parent;
public:
    objvec(): parent()                      { }
    objvec(const objvec& s): parent(s)      { }
    T* operator[] (memint i) const          { return cast<T*>(parent::operator[](i)); }
    T* at(memint i) const                   { return cast<T*>(parent::at(i)); }
    T* back() const                         { return cast<T*>(parent::back()); }
    T* back(memint i) const                 { return cast<T*>(parent::back(i)); }
    void push_back(T* t)                    { parent::push_back(t); }
    void insert(memint pos, T* t)           { parent::insert(pos, t); }
};


class symbol: public object
{
public:
    str const name;
    symbol(const str& s): name(s)  { }
    symbol(const char* s): name(s)  { }
    ~symbol();
};


class symtbl: public objvec<symbol>
{
protected:
    typedef objvec<symbol> parent;

public:
    symtbl(): parent()  { }
    symtbl(const symtbl& s): parent(s)  { }

    memint compare(memint i, const str& key) const;
    bool bsearch(const str& key, memint& index) const;
};


// --- Exceptions ---------------------------------------------------------- //


// This is for static C-style string constants
class ecmessage: public exception
{
public:
    const char* msg;
    ecmessage(const ecmessage&) throw(); // not defined
    ecmessage(const char* _msg) throw();
    ~ecmessage() throw();
    const char* what() throw();
};


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


// UNIX system errors
class esyserr: public emessage
{
public:
    esyserr(int icode, const str& iArg = "") throw();
    ~esyserr() throw();
};


// ------------------------------------------------------------------------- //

void initRuntime();
void doneRuntime();


#endif // __RUNTIME_H
