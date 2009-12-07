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
    virtual bool empty() const;

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
//    operator T*() const                 { return obj; }
    T* get() const                      { return obj; }
};


// --- container & contptr ------------------------------------------------- //


// Basic resizable container, for internal use

class container: public object
{
    friend class contptr;
    friend class str;
    friend class variant;

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
    bool empty() const;  // virt. override
    bool _empty() const         { return _size == 0; } // actual fast empty test
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
    friend class variant;

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

    bool empty() const                  { return obj->_empty(); }
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
    podvec(): parent(&str::null)           { }
    podvec(const podvec& v): parent(v)     { }
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
        { int operator() (const T& a, const T& b) { return int(a - b); } };

template <>
    struct comparator<str>
        { int operator() (const str& a, const str& b) { return a.compare(b); } };

template <>
    struct comparator<const char*>
        { int operator() (const char* a, const char* b) { return strcmp(a, b); } };


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
        int compare(memint index, void* key)
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
        int compare(memint index, void* key)
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
        int compare(memint index, void* key)
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


// --- exceptions ---------------------------------------------------------- //


struct ecmessage: public exception
{
    const char* msg;
    ecmessage(const ecmessage&); // not defined
    ecmessage(const char* _msg);
    ~ecmessage();
    const char* what() const;
};


struct emessage: public exception
{
    str msg;
    emessage(const emessage&); // not defined
    emessage(const str& _msg);
    emessage(const char* _msg);
    ~emessage();
    const char* what() const;
};


#endif // __RUNTIME_H
