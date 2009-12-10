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
    object* _refnz()            { pincrement(&refcount); return this; }

public:
    object()                    : refcount(0) { }
    virtual ~object();

    static int allocated; // used only in DEBUG mode

    void* operator new(size_t);
    void* operator new(size_t, memint extra);
    void  operator delete(void*);

    bool unique() const         { assert(refcount > 0); return refcount == 1; }
    void release();
    object* ref()               { pincrement(&refcount); return this; }
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
    T* get() const                      { return obj; }
};


// --- range --------------------------------------------------------------- //


class range: public noncopyable
{
    friend class variant;
protected:
    struct cont: public object
    {
        integer left;
        integer right;
        cont(): left(0), right(-1)  { }
        cont(integer l, integer r): left(l), right(r)  { }
    };
    cont* obj;
    void _fin()                         { if (obj != &null) obj->release(); }
    void _init(const range& r)          { obj = r.obj->ref<cont>(); }
    void _init(integer l, integer r)    { obj = (new cont(l, r))->ref<cont>(); }
public:
    range()                             : obj(&null) { }
    range(const range& r)               { _init(r); }
    range(integer l, integer r)         { _init(l, r); }
    ~range()                            { _fin(); }
    bool empty() const                  { return obj->left > obj->right; }
    void operator= (const range& r);
    void assign(integer l, integer r);
    uinteger diff() const               { return obj->right - obj->left; }
    bool has(integer i) const           { return i >= obj->left && i <= obj->right; }
    bool equals(integer l, integer r) const
        { return obj->left == l && obj->right == r; }
    bool operator== (const range& other) const;
    memint compare(const range&) const;

    static cont null;
};


// --- ordset -------------------------------------------------------------- //


class ordset: public noncopyable
{
    friend void test_ordset();
    friend class variant;
protected:
    struct cont: public object
    {
        charset cset;
        bool empty() const  { return this == &ordset::null || cset.empty(); }
    };
    cont* obj;
    void _fin()                         { if (obj != &null) obj->release(); }
    void _init(const ordset& r)         { obj = r.obj->ref<cont>(); }
    bool unique()                       { return obj != &null && obj->unique(); }
    void _mkunique();
    charset& mkunique()                 { if (!unique()) _mkunique(); return obj->cset; }
public:
    ordset()                            : obj(&null)  { }
    ordset(const ordset& s)             : obj(s.obj->ref<cont>())  { }
    ~ordset()                           { _fin(); }
    bool empty() const                  { return obj == &null || obj->cset.empty(); }
    void operator= (const ordset&);
    bool operator== (const ordset& s) const   { return this == &s || obj->cset == s.obj->cset; }
    memint compare(const ordset& s) const { return memcmp(&obj->cset, &s.obj->cset, sizeof(obj->cset)); }
    bool has(integer v) const           { return obj->cset[int(v)]; }
    void insert(integer v)              { mkunique().include(int(v)); }
    void insert(integer l, integer h)   { mkunique().include(int(l), int(h)); }
    void erase(integer v)               { mkunique().exclude(int(v)); }
    static cont null;
};


// --- container & contptr ------------------------------------------------- //


// Basic resizable container and a class factory at the same time; for internal
// use. Defines virtual methods for object instantiation and for container 
// functionality in derived classes.

class container: public object
{
    friend class contptr;
    friend class str;
    friend class variant;

protected:
    memint _capacity;
    memint _size;
    char _data[0];

    static void overflow();
    static void idxerr();
    char* data() const          { return (char*)_data; }
    char* data(memint i) const  { assert(i >= 0 && i <= _size); return (char*)_data + i; }
    template <class T>
        T* data(memint i) const { return (T*)data(i * sizeof(T)); }
    char* end() const           { return data() + _size; }
    bool empty() const          { return _size == 0; }
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
    virtual memint compare(memint index, void* key); // aborts

public:
    container(): _capacity(0), _size(0)  { } // for the _null object
    container(memint cap, memint siz)
        : _capacity(cap), _size(siz)  { assert(siz > 0 && cap >= siz); }
};


// A "smart" pointer that holds a reference to a container object; this is
// a base for strings, vectors, maps etc. Individual classes maintain their
// null objects, which serve a purpose of a class factory and an empty object
// at the same time; i.e. the object pointer "obj" is never NULL and always 
// points to an object of a correct class with instantiation methods, as well
// as other virtual methods needed for container functionality (see class
// container). Contptr can't be used directly.

class contptr: public noncopyable
{
    friend class variant;

protected:
    container* obj;

    void _init(const contptr& s)        { obj = s.obj->ref(); }
    char* _init(container* factory, memint);
    void _init(container* factory, const char*, memint);
    void _fin()                         { if (!empty()) obj->release(); }
    bool unique() const                 { return obj->unique(); }
    char* mkunique();
    void chkidx(memint i) const         { if (umemint(i) >= umemint(obj->size())) container::idxerr(); }
    void chkidxa(memint i) const        { if (umemint(i) > umemint(obj->size())) container::idxerr(); }
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
    bool operator== (const contptr& s) const  { return obj == s.obj; }
    void assign(const char*, memint);
    void clear();

    bool empty() const                  { return obj->empty(); }
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

    const char* c_str() const; // can actually modify the object
    char operator[] (memint i) const        { return *obj->data(i); }
    char at(memint i) const                 { return *contptr::at(i); }
    char back() const                       { return *contptr::back(1); }
    void replace(memint pos, char c)        { *contptr::atw(pos) = c; }
    void operator= (const char* c);

    enum { npos = -1 };
    memint find(char c) const;
    memint rfind(char c) const;

    memint compare(const char*, memint) const;
    memint compare(const str& s) const      { return compare(s.data(), s.size()); }
    bool operator== (const char* s) const   { return compare(s, pstrlen(s)) == 0; }
    bool operator== (const str& s) const    { return compare(s.data(), s.size()) == 0; }
    bool operator!= (const char* s) const   { return !(*this == s); }
    bool operator!= (const str& s) const    { return !(*this == s); }

    void operator+= (const char* s);
    void operator+= (const str& s)          { append(s); }
    str  operator+ (const char* s) const    { str r = *this; r += s; return r; }
    str  operator+ (const str& s) const     { str r = *this; r += s; return r; }
    str  substr(memint pos, memint len) const;
    str  substr(memint pos) const;

    static cont null;
};


inline str operator+ (const char* s1, const str& s2)
    { str r = s1; r += s2; return r; }


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
// by the compiler itself. Also podvec is a basis for the universal vector.
// This hopefully generates minimal static code.

template <class T>
class podvec: protected contptr
{
    friend void test_podvec();

protected:
    enum { Tsize = sizeof(T) };
    typedef contptr parent;

    podvec(container* _obj): parent(_obj)  { }

public:
    podvec(): parent(&str::null)            { }
    podvec(const podvec& v): parent(v)      { }
    bool empty() const                      { return parent::empty(); }
    memint size() const                     { return parent::size() / Tsize; }
    const T& operator[] (memint i) const    { return *parent::data<T>(i); }
    const T& at(memint i) const             { return *parent::at<T>(i); }
    const T& back() const                   { return *parent::back<T>(); }
    void clear()                            { parent::clear(); }
    void operator= (const podvec& v)        { parent::operator= (v); }
    void push_back(const T& t)              { new(parent::_appendnz(Tsize)) T(t); }
    void pop_back()                         { parent::pop_back(Tsize); }
    void insert(memint pos, const T& t)     { new(parent::_insertnz(pos * Tsize, Tsize)) T(t); }
    void replace(memint pos, const T& t)    { *parent::atw<T>(pos) = t; }
    void erase(memint pos)                  { parent::_erasenz(pos * Tsize, Tsize); }

    // Give a chance to alternative constructors, e.g. str can be constructed
    // from (const char*). Without these templates below temp objects are
    // created and then copied into the vector. Though these are somewhat
    // dangerous too.
    template <class U>
        void push_back(const U& u)          { new(parent::_appendnz(Tsize)) T(u); }
    template <class U>
        void insert(memint pos, const U& u) { new(parent::_insertnz(pos * Tsize, Tsize)) T(u); }
    template <class U>
        void replace(memint pos, const U& u) { *parent::atw<T>(pos) = u; }
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
        // Virtual overrides
        container* new_(memint cap, memint siz)  { return new(cap) cont(cap, siz); }
        container* null_obj()                    { return &vector::null; }
        void finalize(void* p, memint len)
        {
            // Finalization goes backwards
            (char*&)p += len - Tsize;
            for ( ; len; len -= Tsize, Tref(p)--)
                Tptr(p)->~T();
        }
        void copy(void* dest, const void* src, memint len)
        {
            for ( ; len; len -= Tsize, Tref(dest)++, Tref(src)++)
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

    vector(container* o): parent(o)  { }

public:
    vector(): parent(&null)  { }

    static cont null;
};


template <class T>
    typename vector<T>::cont vector<T>::null;


// --- set ----------------------------------------------------------------- //


template <class T>
    struct comparator
        { memint operator() (const T& a, const T& b) { return memint(a - b); } };

template <>
    struct comparator<str>
        { memint operator() (const str& a, const str& b) { return a.compare(b); } };

template <>
    struct comparator<const char*>
        { memint operator() (const char* a, const char* b) { return strcmp(a, b); } };


template <class T, class Comp = comparator<T> >
class set: protected vector<T>
{
protected:
    typedef vector<T> parent;
    typedef T* Tptr;
    typedef Tptr& Tref;
    
    class cont: public parent::cont
    {
        typedef typename parent::cont parent;
        container* new_(memint cap, memint siz) { return new(cap) cont(cap, siz); }
        container* null_obj()                   { return &set::null; }
        memint compare(memint index, void* key)
        {
            static Comp comp;
            return comp(*container::data<T>(index), *Tptr(key));
        }
    public:
        cont(): parent()  { }
        cont(memint cap, memint siz): parent(cap, siz)  { }
    };

    static cont null;

public:
    set(): parent(&null)            { }
    set(const set& s): parent(s)    { }

    bool empty() const              { return parent::empty(); }
    memint size() const             { return parent::size(); }
    const T& operator[] (memint index) const
        { return parent::operator[] (index); }

    bool has(const T& item) const
    {
        memint index;
        return contptr::bsearch<T>(item, index);
    }

    bool insert(const T& item)
    {
        memint index;
        if (!contptr::bsearch<T>(item, index))
        {
            parent::insert(index, item);
            return true;
        }
        else
            return false;
    }

    void erase(const T& item)
    {
        memint index;
        if (contptr::bsearch<T>(item, index))
            parent::erase(index);
    }
};


template <class T, class Comp>
    typename set<T, Comp>::cont set<T, Comp>::null;


// --- dict ---------------------------------------------------------------- //


template <class Tkey, class Tval>
struct dictitem: public object
{
    const Tkey key;
    Tval val;
    dictitem(const Tkey& _key, const Tval& _val)
        : key(_key), val(_val) { }
};


template <class Tkey, class Tval, class Comp = comparator<Tkey> >
class dict: protected vector<objptr<dictitem<Tkey, Tval> > >
{
protected:
    typedef dictitem<Tkey, Tval> Titem;
    typedef objptr<Titem> T;
    typedef vector<T> parent;
    typedef T* Tptr;
    typedef Tptr& Tref;
    enum { Tsize = sizeof(T) };

    class cont: public parent::cont
    {
        typedef typename parent::cont parent;
        container* new_(memint cap, memint siz) { return new(cap) cont(cap, siz); }
        container* null_obj()                   { return &dict::null; }
        memint compare(memint index, void* key)
        {
            static Comp comp;
            return comp((*container::data<T>(index))->key, *(Tkey*)key);
        }
    public:
        cont(): parent()  { }
        cont(memint cap, memint siz): parent(cap, siz)  { }
    };

    static cont null;

public:
    dict(): parent(&null)           { }
    dict(const dict& s): parent(s)  { }
    
    bool empty() const              { return parent::empty(); }
    memint size() const             { return parent::size(); }

    typedef Titem item_type;
    const item_type& operator[] (memint index) const
        { return *parent::operator[] (index); }

    const Tval* find(const Tkey& key) const
    {
        memint index;
        if (contptr::bsearch<Tkey>(key, index))
            return &parent::operator[] (index)->val;
        else
            return NULL;
    }

    void replace(const Tkey& key, const Tval& val)
    {
        memint index;
        if (!contptr::bsearch<Tkey>(key, index))
            parent::insert(index, new Titem(key, val));
        else if (parent::unique())
            (parent::operator[] (index))->val = val;
        else
            parent::replace(index, new Titem(key, val));
    }

    void erase(const Tkey& key)
    {
        memint index;
        if (contptr::bsearch<Tkey>(key, index))
            parent::erase(index);
    }
};


template <class Tkey, class Tval, class Comp>
    typename dict<Tkey, Tval, Comp>::cont dict<Tkey, Tval, Comp>::null;


// --- object collection --------------------------------------------------- //


class symbol: public object
{
public:
    const str name;
    symbol(const str& s): name(s)  { }
    symbol(const char* s): name(s)  { }
    ~symbol();
};


// Collection of pointers to symbol objects; doesn't free the objects

class symvec_impl: public podvec<symbol*>
{
protected:
    typedef podvec<symbol*> parent;

    class cont: public str::cont
    {
        typedef str::cont parent;
        container* new_(memint cap, memint siz) { return new(cap) cont(cap, siz); }
        container* null_obj()                   { return &symvec_impl::null; }
        memint compare(memint index, void* key)
            { return (*container::data<symbol*>(index))->name.compare(*(str*)key); }
    public:
        cont(): parent()  { }
        cont(memint cap, memint siz): parent(cap, siz)  { }
    };

    static cont null;

public:
    symvec_impl(): parent(&null)  { }
    symvec_impl(const symvec_impl& s): parent(s)  { }
};


template <class T>
class symvec: protected symvec_impl
{
    typedef symvec_impl parent;
public:
    symvec(): parent()                      { }
    symvec(const symvec& s): parent(s)      { }
    bool empty() const                      { return parent::empty(); }
    memint size() const                     { return parent::size(); }
    T* operator[] (memint i) const          { return cast<T*>(parent::operator[](i)); }
    T* at(memint i) const                   { return cast<T*>(parent::at(i)); }
    T* back() const                         { return cast<T*>(parent::back()); }
    void clear()                            { parent::clear(); }
    void operator= (const symvec& v)        { parent::operator= (v); }
    void push_back(T* t)                    { parent::push_back(t); }
    void pop_back()                         { parent::pop_back(); }
    void insert(memint pos, T* t)           { parent::insert(pos, t); }
    void erase(memint pos)                  { parent::erase(pos); }
    bool bsearch(const str& n, memint& i)   { return parent::bsearch(n, i); }
};


// --- ecmessag/emessage/esyserr ------------------------------------------- //


// This is for static C-style string constants
struct ecmessage: public exception
{
    const char* msg;
    ecmessage(const ecmessage&); // not defined
    ecmessage(const char* _msg);
    ~ecmessage();
    const char* what() const;
};


// For dynamically generated strings
struct emessage: public exception
{
    str msg;
    emessage(const emessage&); // not defined
    emessage(const str& _msg);
    emessage(const char* _msg);
    ~emessage();
    const char* what() const;
};


// UNIX system errors
struct esyserr: public emessage
{
    esyserr(int icode, const str& iArg = "");
};


// --- variant ------------------------------------------------------------- //


typedef vector<variant> varvec;
typedef set<variant> varset;
typedef dict<variant, variant> vardict;


class variant: public noncopyable
{
    friend void test_variant();

public:

    enum Type
        { NONE, ORD, REAL, STR, RANGE, VEC, SET, ORDSET, DICT, OBJ,
            REFCNT = STR };

    struct _None { int dummy; }; 
    static _None none;

protected:

    Type type;
    union
    {
        integer  _ord;      // int, char and bool
        real     _real;
        object*  _obj;
    } val;

    static void _type_err();
    static void _range_err();
    void _req(Type t) const             { if (type != t) _type_err(); }
#ifdef DEBUG
    void _dbg(Type t) const             { if (type != t) _type_err(); }
#else
    void _dbg(Type t) const             { }
#endif

    void _init()                        { type = NONE; }
    void _init(_None)                   { type = NONE; }
    void _init(bool v)                  { type = ORD; val._ord = v; }
    void _init(char v)                  { type = ORD; val._ord = uchar(v); }
    void _init(uchar v)                 { type = ORD; val._ord = v; }
    void _init(int v)                   { type = ORD; val._ord = v; }
#ifdef SH64
    void _init(long long v)             { type = ORD; val._ord = v; }
#endif
    void _init(real v)                  { type = REAL; val._real = v; }
    void _init(const str& v)            { _init(STR, v.obj); }
    void _init(const char* s)           { type = STR; ::new(&val._obj) str(s); }
    void _init(const range& v)          { _init(RANGE, v.obj); }
    void _init(const varvec& v)         { _init(VEC, v.obj); }
    void _init(const varset& v)         { _init(SET, v.obj); }
    void _init(const ordset& v)         { _init(ORDSET, v.obj); }
    void _init(const vardict& v)        { _init(DICT, v.obj); }
    void _init(Type t, object* o)       { type = t; val._obj = o->ref(); }
    void _fin_refcnt();
    void _fin()                         { if (is_refcnt()) _fin_refcnt(); }

public:
    variant()                           { _init(); }
    variant(const variant& v)
        : type(v.type), val(v.val)      { if (is_refcnt()) v.val._obj->ref(); }
    template <class T>
        variant(const T& v)             { _init(v); }
    ~variant()                          { _fin(); }

    template <class T>
        void operator= (const T& v)     { _fin(); _init(v); }
    void operator= (const variant&);
    void clear()                        { _fin(); _init(); }
    bool empty() const;

    memint compare(const variant&) const;
    bool operator== (const variant&) const;
    bool operator!= (const variant& v) const { return !(operator==(v)); }

    Type getType() const                { return type; }
    bool is(Type t) const               { return type == t; }
    bool is_none() const                { return type == NONE; }
    bool is_refcnt() const              { return type >= REFCNT; }

    // Fast "unsafe" access methods; checked for correctness in DEBUG mode
    bool        _bool()           const { _dbg(ORD); return val._ord; }
    uchar       _uchar()          const { _dbg(ORD); return val._ord; }
    integer     _int()            const { _dbg(ORD); return val._ord; }
    integer     _ord()            const { _dbg(ORD); return val._ord; }
    const str&  _str()            const { _dbg(STR); return *(str*)&val._obj; }
    const range& _range()         const { _dbg(RANGE); return *(range*)&val._obj; }
    const varvec& _vec()          const { _dbg(VEC); return *(varvec*)&val._obj; }
    const varset& _set()          const { _dbg(SET); return *(varset*)&val._obj; }
    const ordset& _ordset()       const { _dbg(ORDSET); return *(ordset*)&val._obj; }
    const vardict& _dict()        const { _dbg(DICT); return *(vardict*)&val._obj; }
    object*     _obj()            const { _dbg(OBJ); return val._obj; }
    integer&    _ord()                  { _dbg(ORD); return val._ord; }
    str&        _str()                  { _dbg(STR); return *(str*)&val._obj; }
    range&      _range()                { _dbg(RANGE); return *(range*)&val._obj; }
    varvec&     _vec()                  { _dbg(VEC); return *(varvec*)&val._obj; }
    varset&     _set()                  { _dbg(SET); return *(varset*)&val._obj; }
    ordset&     _ordset()               { _dbg(ORDSET); return *(ordset*)&val._obj; }
    vardict&    _dict()                 { _dbg(DICT); return *(vardict*)&val._obj; }
    object*&    _obj()                  { _dbg(OBJ); return val._obj; }

    // Safer access methods; may throw an exception
    bool        as_bool()         const { _req(ORD); return _bool(); }
    char        as_char()         const { _req(ORD); return _uchar(); }
    uchar       as_uchar()        const { _req(ORD); return _uchar(); }
    integer     as_int()          const { _req(ORD); return _int(); }
    integer     as_ord()          const { _req(ORD); return _ord(); }
    const str&  as_str()          const { _req(STR); return _str(); }
    const range& as_range()       const { _req(RANGE); return _range(); }
    const varvec& as_vec()        const { _req(VEC); return _vec(); }
    const varset& as_set()        const { _req(SET); return _set(); }
    const ordset& as_ordset()     const { _req(ORDSET); return _ordset(); }
    const vardict& as_dict()      const { _req(DICT); return _dict(); }
    object*     as_obj()          const { _req(OBJ); return _obj(); }
    integer&    as_ord()                { _req(ORD); return _ord(); }
    str&        as_str()                { _req(STR); return _str(); }
    range&      as_range()              { _req(RANGE); return _range(); }
    varvec&     as_vec()                { _req(VEC); return _vec(); }
    varset&     as_set()                { _req(SET); return _set(); }
    ordset&     as_ordset()             { _req(ORDSET); return _ordset(); }
    vardict&    as_dict()               { _req(DICT); return _dict(); }
    object*&    as_obj()                { _req(OBJ); return _obj(); }
};

template <>
    struct comparator<variant>
        { memint operator() (const variant& a, const variant& b) { return a.compare(b); } };


extern template class vector<variant>;
extern template class set<variant>;
extern template class dict<variant, variant>;
extern template class podvec<variant>;


// --- runtime objects ----------------------------------------------------- //


class reference: public object
{
public:
    variant var;
    reference(const variant& v): var(v)  { }
    template <class T>
        reference(const T& v): var(v)  { }
    ~reference();
};


class Type;    // defined in typesys.h
class State;   // same

class rtobject: public object
{
protected:
    Type* _type;
public:
    rtobject(Type* t): _type(t)  { }
    Type* type()  { return _type; }
};


class stateobj: public rtobject
{
protected:
#ifdef DEBUG
    memint varcount;
    static void idxerr();
#endif
    variant vars[0];
    stateobj(State* t);

public:
    static stateobj* new_(State* state);
    ~stateobj();
    State* type()  { return (State*)_type; }
    variant* var(memint index)
    {
#ifdef DEBUG
        if (umemint(index) >= umemint(varcount))
            idxerr();
#endif
        return vars + index;
    }    
};


// --- FIFO ---------------------------------------------------------------- //


// *BSD/Darwin hack
#ifndef O_LARGEFILE
#  define O_LARGEFILE 0
#endif


const memint _varsize = memint(sizeof(variant));


// The abstract FIFO interface. There are 2 modes of operation: variant FIFO
// and character FIFO. Destruction of variants is basically not handled by
// this class to give more flexibility to implementations (e.g. there may be
// buffers shared between 2 fifos or other container objects). If you implement,
// say, only input methods, the default output methods will throw an exception
// with a message "FIFO is read-only", and vice versa. Iterators may be
// implemented in descendant classes but are not supported by default.
class fifo: public rtobject
{
    fifo& operator<< (bool);   // compiler traps
    fifo& operator<< (void*);
    fifo& operator<< (object*);

protected:
    enum { TAB_SIZE = 8 };

    bool _is_char_fifo;

    static void _empty_err();
    static void _wronly_err();
    static void _rdonly_err();
    static void _fifo_type_err();
    void _req(bool req_char) const      { if (req_char != _is_char_fifo) _fifo_type_err(); }
    void _req_non_empty() const;
    void _req_non_empty(bool _char) const;

    // Minimal set of methods required for both character and variant FIFO
    // operations. Implementations should guarantee variants will never be
    // fragmented, so that a buffer returned by get_tail() always contains at
    // least sizeof(variant) bytes (8, 12 or 16 bytes depending on the host
    // platform) in variant mode, or at least 1 byte in character mode.
    virtual const char* get_tail();          // Get a pointer to tail data
    virtual const char* get_tail(memint*);   // ... also return the length
    virtual void deq_bytes(memint);          // Discard n consecutive bytes returned by get_tail()
    virtual variant* enq_var();              // Reserve uninitialized space for a variant
    virtual memint enq_chars(const char*, memint); // Push arbitrary number of bytes, return actual number, char fifo only

    void _token(const charset& chars, str* result);

public:
    fifo(Type*, bool is_char);
    ~fifo();

    enum { CHAR_ALL = MEMINT_MAX - 2, CHAR_SOME = MEMINT_MAX - 1 };

    virtual bool empty() const;     // throws efifowronly
    virtual void flush();           // empty, overridden in file fifos

    // Main FIFO operations, work on both char and variant fifos; for char
    // fifos the variant is read as either a char or a string.
    void var_enq(const variant&);
    void var_preview(variant&);
    void var_deq(variant&);
    void var_eat();

    // Character FIFO operations
    bool is_char_fifo() const           { return _is_char_fifo; }
    int preview(); // returns -1 on eof
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
    static bool is_eol_char(char c)     { return c == '\r' || c == '\n'; }
    int  skip_indent(); // spaces and tabs, tab lenghts are properly calculated
    bool eof() const                    { return empty(); }

    memint  enq(const char* p, memint count)  { return enq_chars(p, count); }
    void enq(const char* s);
    void enq(const str& s);
    void enq(char c);
    void enq(uchar c);
    void enq(long long i);

    fifo& operator<< (const char* s)   { enq(s); return *this; }
    fifo& operator<< (const str& s)    { enq(s); return *this; }
    fifo& operator<< (char c)          { enq(c); return *this; }
    fifo& operator<< (uchar c)         { enq(c); return *this; }
    fifo& operator<< (long long i)     { enq((long long)i); return *this; }
    fifo& operator<< (int i)           { enq((long long)i); return *this; }
    fifo& operator<< (memint i)        { enq((long long)i); return *this; }
};

const char endl = '\n';


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
    static memint CHUNK_SIZE; // settable from unit tests
#else
    enum { CHUNK_SIZE = 32 * _varsize };
#endif

protected:
    struct chunk: noncopyable
    {
        chunk* next;
        char data[0];
        
#ifdef DEBUG
        chunk(): next(NULL)         { pincrement(&object::allocated); }
        ~chunk()                    { pdecrement(&object::allocated); }
#else
        chunk(): next(NULL) { }
#endif
        void* operator new(size_t)  { return ::pmemalloc(sizeof(chunk) + CHUNK_SIZE); }
        void operator delete(void* p)  { ::pmemfree(p); }
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
    memint enq_chars(const char*, memint);

    char* enq_space(memint);
    memint enq_avail();

public:
    memfifo(Type*, bool is_char);
    ~memfifo();

    void clear();
    bool empty() const;
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
    char* buffer;
    memint   bufsize;
    memint   bufhead;
    memint   buftail;

    const char* get_tail();
    const char* get_tail(memint*);
    void deq_bytes(memint);
    variant* enq_var();
    memint enq_chars(const char*, memint);

    char* enq_space(memint);
    memint enq_avail();

public:
    buffifo(Type*, bool is_char);
    ~buffifo();

    bool empty() const; // throws efifowronly
    void flush(); // throws efifordonly
};


class strfifo: public buffifo
{
protected:
    str string;
    void clear();
public:
    strfifo(Type*);
    strfifo(Type*, const str&);
    ~strfifo();
    bool empty() const; // override
    void flush(); // override
    str all() const;
};


class intext: public buffifo
{
protected:
    enum { BUF_SIZE = 2048 * sizeof(integer) };

    const str file_name;
    str  filebuf;
    int  _fd;
    bool _eof;

    void error(int code); // throws esyserr
    void doopen();
    void doread();

public:
    intext(Type*, const str& fn);
    ~intext();
    
    bool empty() const; //override
    str  get_file_name() const { return file_name; }
    void open()                { empty(); /* attempt to fill the buffer */ }
};


class outtext: public buffifo
{
protected:
    enum { BUF_SIZE = 2048 * sizeof(integer) };

    const str file_name;
    str  filebuf;
    int  _fd;
    bool _err;

    void error(int code); // throws esyserr

public:
    outtext(Type*, const str& fn);
    ~outtext();

    str  get_file_name() const { return file_name; }
    void flush(); // override
    void open()                { flush(); }
};


// Standard input/output object, a two-way fifo. In case of sterr it is
// write-only.
class stdfile: public intext
{
protected:
    int _ofd;
    virtual memint enq_chars(const char*, memint);
public:
    stdfile(int infd, int outfd);
    ~stdfile();
};


extern stdfile sio;
extern stdfile serr;


#endif // __RUNTIME_H
