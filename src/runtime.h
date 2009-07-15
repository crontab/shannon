#ifndef __RUNTIME_H
#define __RUNTIME_H

#include "common.h"
#include "charset.h"

#include <exception>
#include <string>
#include <vector>
#include <map>
#include <set>



#define foreach(type,iter,cont) \
    for (type iter = (cont).begin(); iter != (cont).end(); iter++)

#define vforeach(type,iter,cont) \
    for (type##_iterator iter = (cont).type##_begin(); iter != (cont).type##_end(); iter++)

#ifdef BOOL
#  error "BOOL defined somewhere conflicts with internal definitions in variant class"
#endif

// Implementation is in variant.cpp and fifo.cpp.

// TODO: at least the implementation of tuple should be rewritten with
// realloc(), because we always have a vector of variants and we never
// hold pointers to tuple members, so the pedantic pointer keeping
// of the STL's vector<> template is overkill for us.

class variant;
class object;
class fifo_intf;


typedef std::map<variant, variant> dict_impl;
typedef dict_impl::const_iterator dict_iterator;
typedef std::set<variant> set_impl;
typedef set_impl::const_iterator set_iterator;


class _None { int dummy; };


class variant
{
    friend class CodeSeg;
    friend class Ordinal; // for hacking the runtime typecasts

public:
    // Note: the order is important, especially after STR
    enum Type
      { NONE, BOOL, CHAR, INT, REAL, STR, OBJECT,
        NONPOD = STR, REFCNT = OBJECT, ANYOBJ = OBJECT };

protected:
    Type type;
    union
    {
        integer  _int;      // int, char and bool
        real     _real;
        char     _str[sizeof(str)];
        char*    _str_ptr;  // for debugging, with the hope it points to a string in _str
        object*  _obj;
    } val;

#ifdef DEBUG
    void _dbg(Type t) const         { if (type != t) _type_err(); }
    void _dbg_ord() const           { if (!is_ordinal()) _type_err(); }
#else
    void _dbg(Type t) const         { }
    void _dbg_ord() const           { }
#endif

    // Fast "unsafe" access methods; checked for correctness in DEBUG mode
    bool     _bool()            const { _dbg(BOOL); return val._int; }
    uchar    _uchar()           const { _dbg(CHAR); return val._int; }
    integer  _int()             const { _dbg(INT); return val._int; }
    integer& _int_write()             { _dbg(INT); return val._int; }
    integer  _ord()             const { _dbg_ord(); return val._int; }
    str& _str_write()                 { _dbg(STR); return *(str*)val._str; }
    const str& _str_read()      const { _dbg(STR); return *(str*)val._str; }
    object* _object()           const { _dbg(OBJECT); return val._obj; }

    // Initializers/finalizers: used in constructors/destructors and assigments
    void _init()                    { type = NONE; }
    void _init(bool b)              { type = BOOL; val._int = b; }
    void _init(char c)              { type = CHAR; val._int = uchar(c); }
    void _init(uchar c)             { type = CHAR; val._int = c; }
    void _init(int i)               { type = INT; val._int = i; }
    void _init(long long i)         { type = INT; val._int = i; }
    void _init(mem i)               { type = INT; val._int = i; }
    void _init(double r)            { type = REAL; val._real = r; }
    void _init(const str&);
    void _init(const char*);
    void _init(object*);
    void _init(const variant&);

    void _fin()                     { if (is_nonpod()) _fin2(); }
    void _fin2();

    // Errors: type mismatch, out of INT range
    static void _type_err();
    static void _range_err();
    static void _index_err();
    void _req(Type t) const         { if (type != t) _type_err(); }
    void _req_refcnt() const        { if (!is_refcnt()) _type_err(); }
    void _req_obj() const           { if (!is_object()) _type_err(); }

    unsigned as_char_int() const;

public:
    variant()                       { _init(); }
    variant(_None)                  { _init(); }
    template<class T>
        variant(const T& v)         { _init(v); }
    variant(const variant& v)       { _init(v); }
    ~variant()                      { _fin(); }

    void clear()                    { _fin(); _init(); }
    void operator=(_None)           { _fin(); _init(); }
    // TODO: check cases when the same value is assigned (e.g. v = v)
    template<class T>
        void operator= (const T& v)     { _fin(); _init(v); }
    void operator= (const variant& v)   { _fin(); _init(v); }
    bool operator== (const variant& v) const;
    bool operator!= (const variant& v)
                              const { return !(this->operator==(v)); }

    void dump(fifo_intf&) const;
    bool to_bool()                  { return !empty(); }
    str  to_string() const;
    bool operator< (const variant& v) const;

    // TODO: is_int(int), is_real(real) ... or operator == maybe?
    Type getType()            const { return type; }
    bool is(Type t)           const { return type == t; }
    bool is_null()            const { return type == NONE; }
    bool is_ordinal()         const { return type >= BOOL && type <= INT; }
    bool is_nonpod()          const { return type >= NONPOD; }
    bool is_refcnt()          const { return type >= REFCNT; }
    bool is_object()          const { return type >= ANYOBJ; }

    bool is_unique() const;
    void unique();

    // Type conversions
    bool as_bool()            const { _req(BOOL); return val._int; }
    uchar as_char()           const { _req(CHAR); return val._int; }
    integer as_int()          const { _req(INT); return val._int; }
    integer as_ordinal()      const { if (!is_ordinal()) _type_err(); return val._int; }
    real as_real()            const { _req(REAL); return val._real; }
    const str& as_str()       const { _req(STR); return _str_read(); }
    object* as_object()       const { _req_obj(); return val._obj; }

    bool empty() const;
};


extern const variant null;
extern const str null_str;


fifo_intf& operator<< (fifo_intf&, const variant&);


class Type; // see typesys.h

class object: public noncopyable
{
    friend void _release(object*);
    friend void _replace(object*&);
    friend object* _grab(object*);
    friend void _unique(object*&);

public:
#ifdef DEBUG
    static int alloc;
#endif

protected:
    int refcount;
    Type* runtime_type;
    virtual object* clone() const; // calls fatal()
public:
    object(Type*);
    virtual ~object();
    bool is_unique() const  { return refcount == 1; }
    virtual bool empty();   // non-const because fifo's can be modified when getting the eof status
    virtual void dump(fifo_intf&) const;
    virtual bool less_than(object* o) const;
    Type* get_rt() const    { return runtime_type; }
    void set_rt(Type* rt)   { assert(runtime_type == NULL); runtime_type = rt; }
};


inline object* _grab(object* o)  { if (o) pincrement(&o->refcount); return o; }
template<class T>
    T* grab(T* o)  { return (T*)_grab(o); }

void _release(object*);
template<class T>
    void release(T* o)  { _release(o); }

void _replace(object*&, object*);
template<class T>
    void replace(T*& p, T* o)  { _replace((object*&)p, o); }

void _unique(object*&);
template<class T>
    void unique(T*& o)  { if (!o->is_unique()) _unique((object*&)o); }


class range: public object
{
protected:
    range(const range& other);
public:
    integer left;
    integer right;
    range(Type*, integer l, integer r);
    ~range();
    virtual object* clone() const;
    void assign(integer l, integer r)   { left = l; right = r; }
    bool empty()                        { return left > right; }
    bool has(integer i) const           { return i >= left && i <= right; }
    bool equals(integer l, integer r) const;
    bool equals(const range& other) const;
    virtual void dump(fifo_intf&) const;
    virtual bool less_than(object* o) const;
};

typedef range* prange;


class varlist
{
protected:
    std::vector<variant> impl;
    varlist(const varlist&);
    void operator= (const varlist&); // not implemented
public:
    varlist();
    ~varlist()                                { clear(); }
    mem size()                          const { return impl.size(); }
    bool empty()                              { return impl.empty(); }
    void resize(mem);
    void clear();
    void push_back(const variant& v);
    void append(const varlist&);
    variant& back()                           { return impl.back(); }
    const variant& back()               const { return impl.back(); }
    void pop_back();
    void insert(mem, const variant&);
    void put(mem i, const variant& v)         { impl[i] = v; }
    void erase(mem i);
    void erase(mem i, mem count);
    const variant& operator[] (mem i)   const { return impl[i]; }
};


// Fast uninitialized storage for variants

struct podvar { char data[sizeof(variant)]; };
typedef std::vector<podvar> podvar_impl;

class varstack: protected podvar_impl
{
protected:
    void _err();
public:
    varstack();
    ~varstack();

    variant* reserve(mem);  // returns a stack base ptr for fast operation
    void free(mem);
    
    mem size()            const { return podvar_impl::size(); }
    void push(variant);
    variant& top()              { return (variant&)back(); }
    const variant& top()  const { return (variant&)back(); }
    void pop();
};


class tuple: public object, public varlist
{
protected:
    tuple(const tuple& other);
    void operator=(const tuple& other)  { varlist::operator=(other); }
public:
    tuple(Type*);
    tuple(Type*, mem, const variant&);
    ~tuple();
    virtual object* clone() const;
    bool empty()  { return varlist::empty(); }
    virtual void dump(fifo_intf&) const;
};

typedef tuple* ptuple;


class dict: public object
{
    dict_impl impl;
protected:
    dict(const dict& other);
public:
    dict(Type*);
    ~dict();
    virtual object* clone() const;
    mem size()                          const { return impl.size(); }
    bool empty()                              { return impl.empty(); }
    void untie(const variant& v)              { impl.erase(v); }
    variant& operator[] (const variant& v)    { return impl[v]; }
    dict_iterator find(const variant& v)const { return impl.find(v); }
    virtual void dump(fifo_intf&) const;
    dict_iterator begin()               const { return impl.begin(); }
    dict_iterator end()                 const { return impl.end(); }
};


class set: public object
{
    set_impl impl;
protected:
    set(const set& other);
public:
    set(Type*);
    ~set();
    virtual object* clone() const;
    virtual void dump(fifo_intf&) const;
    mem size()                          const { return impl.size(); }
    bool empty()                              { return impl.empty(); }
    void tie(const variant& v)                { impl.insert(v); }
    void untie(const variant& v)              { impl.erase(v); }
    set_iterator find(const variant& v) const { return impl.find(v); }
    set_iterator begin()                const { return impl.begin(); }
    set_iterator end()                  const { return impl.end(); }
};

typedef set* pset;


class ordset: public object
{
    charset impl;

protected:
    ordset(const ordset& other);
public:
    ordset(Type*);
    ~ordset();
    virtual object* clone() const;
    virtual void dump(fifo_intf&) const;
    bool empty()                              { return impl.empty(); }
    void tie(int v)                           { impl.include(v); }
    void untie(int v)                         { impl.exclude(v); }
    bool has(int v) const                     { return impl[v]; }
    bool equals (const ordset& other)   const { return impl.eq(other.impl); }
};

typedef ordset* pordset;


template<class T>
class objptr
{
protected:
    T* obj;
public:
    objptr(): obj(NULL)                 { }
    objptr(const objptr<T>& p)          { obj = grab(p.obj); }
    objptr(T* o)                        { obj = grab(o); }
    ~objptr()                           { release(obj); }
    void operator= (const objptr<T>& p) { replace(obj, p.obj); }
    void operator= (T* o)               { replace(obj, o); }
    T* operator* () const               { return obj; }
    T* operator-> () const              { return obj; }
    T* get() const                      { return obj; }
    operator T*() const                 { return obj; }
    bool operator== (T* o) const        { return obj == o; }
    bool operator== (const objptr<T>& p) const { return obj == p.obj; }
    bool operator!= (T* p) const        { return obj != p; }

    friend inline
    bool operator== (T* o, const objptr<T>& p) { return o == p.obj; }
};


inline void variant::unique()  { if (is_refcnt()) _unique(val._obj); }


// --- FIFO ---------------------------------------------------------------- //


// *BSD/Darwin hack
#ifndef O_LARGEFILE
#  define O_LARGEFILE 0
#endif


// The abstract FIFO interface. There are 2 modes of operation: variant FIFO
// and character FIFO. Destruction of variants is basically not handled by
// this class to give more flexibility to implementations (e.g. there may be
// buffers shared between 2 fifos or other container objects). If you implement,
// say, only input methods, the default output methods will throw an exception
// with a message "FIFO is read-only", and vice versa. Iterators may be
// implemented in descendant classes but are not supported by default.
class fifo_intf: public object
{
protected:
    enum { TAB_SIZE = 8 };

    bool _char;

    static void _empty_err();
    static void _wronly_err();
    static void _rdonly_err();
    static void _fifo_type_err();
    // TODO: _req() can be empty in RELEASE build
    void _req(bool req_char) const      { if (req_char != _char) _fifo_type_err(); }
    void _req_non_empty();
    void _req_non_empty(bool _char);

    // Minimal set of methods required for both character and variant FIFO
    // operations. Implementations should guarantee variants will never be
    // fragmented, so that a buffer returned by get_tail() always contains at
    // least sizeof(variant) bytes (8, 12 or 16 bytes depending on the host
    // platform) in variant mode, or at least 1 byte in character mode.
    virtual const char* get_tail();          // Get a pointer to tail data
    virtual const char* get_tail(mem*);      // ... also return the length
    virtual void deq_bytes(mem);             // Discard n consecutive bytes returned by get_tail()
    virtual variant* enq_var();              // Reserve uninitialized space for a variant
    virtual mem enq_chars(const char*, mem); // Push arbitrary number of bytes, return actual number, char fifo only

    void _token(const charset& chars, str* result);

public:
    fifo_intf(Type*, bool is_char);
    ~fifo_intf();

    enum { CHAR_ALL = mem(-2), CHAR_SOME = mem(-1) };

    virtual void dump(fifo_intf&) const; // just displays <fifo>
    virtual bool empty();   // throws efifowronly

    // Main FIFO operations, work on both char and variant fifos
    void var_enq(const variant&);
    void var_preview(variant&);
    void var_deq(variant&);
    void var_eat();

    // Character FIFO operations
    bool is_char_fifo() const           { return _char; }
    int preview(); // returns -1 on eof
    uchar get();
    bool get_if(char c);
    str  deq(mem);  // CHAR_ALL, CHAR_SOME can be specified
    str  deq(const charset& c)          { str s; _token(c, &s); return s; }
    str  token(const charset& c)        { return deq(c); } // alias
    void eat(const charset& c)          { _token(c, NULL); }
    void skip(const charset& c)         { eat(c); } // alias
    bool eol();
    void skip_eol();
    bool is_eol_char(char c)            { return c == '\r' || c == '\n'; }
    int  skip_indent(); // spaces and tabs, tab lenghts are properly calculated
    bool eof()                          { return empty(); }

    mem  enq(const char* p, mem count)  { return enq_chars(p, count); }
    void enq(const char* s);
    void enq(const str& s);
    void enq(char c);
    void enq(uchar c);
    void enq(long long i);

    fifo_intf& operator<< (const char* s)   { enq(s); return *this; }
    fifo_intf& operator<< (const str& s)    { enq(s); return *this; }
    fifo_intf& operator<< (char c)          { enq(c); return *this; }
    fifo_intf& operator<< (uchar c)         { enq(c); return *this; }
    fifo_intf& operator<< (long long i)     { enq((long long)i); return *this; }
    fifo_intf& operator<< (int i)           { enq((long long)i); return *this; }
    fifo_intf& operator<< (mem i)           { enq((long long)i); return *this; }
};

const char endl = '\n';


// The fifo class implements a linked list of "chunks" in memory, where
// each chunk is the size of 32 * sizeof(variant). Both enqueue and deqeue
// operations are O(1), and memory usage is better than that of a plain linked 
// list of elements, as "next" pointers are kept for bigger chunks of elements
// rather than for each element. Can be used both for variants and chars. This
// class "owns" variants, i.e. proper construction and desrtuction is done.
class fifo: public fifo_intf
{
public:
#ifdef DEBUG
    static mem CHUNK_SIZE; // settable from unit tests
#else
    enum { CHUNK_SIZE = 32 * sizeof(variant) };
#endif

protected:
    struct chunk: noncopyable
    {
        chunk* next;
        char data[0];
        
#ifdef DEBUG
        chunk(): next(NULL)         { pincrement(&object::alloc); }
        ~chunk()                    { pdecrement(&object::alloc); }
#else
        chunk(): next(NULL) { }
#endif
        void* operator new(size_t)  { return new char[sizeof(chunk) + CHUNK_SIZE]; }
    };

    chunk* head;    // in
    chunk* tail;    // out
    unsigned head_offs;
    unsigned tail_offs;

    void enq_chunk();
    void deq_chunk();

    // Overrides
    const char* get_tail();
    const char* get_tail(mem*);
    void deq_bytes(mem);
    variant* enq_var();
    mem enq_chars(const char*, mem);

    char* enq_space(mem);
    mem enq_avail();

public:
    fifo(Type*, bool is_char);
    ~fifo();

    void clear();

    virtual bool empty();
    // virtual void dump(fifo_intf&) const;
};


// This is an abstract buffered fifo class. Implementations should validate the
// buffer in the overridden empty() and flush() methods, for input and output
// fifos respectively. To simplify things, buf_fifo objects are not supposed to
// be reusable, i.e. once the end of file is reached, the implementation is not
// required to reset its state. Variant fifo implementations should guarantee
// at least sizeof(variant) bytes in calls to get_tail() and enq_var().
class buf_fifo: public fifo_intf
{
protected:
    char* buffer;
    mem   bufsize;
    mem   bufhead;
    mem   buftail;

    const char* get_tail();
    const char* get_tail(mem*);
    void deq_bytes(mem);
    variant* enq_var();
    mem enq_chars(const char*, mem);

    char* enq_space(mem);
    mem enq_avail();

public:
    buf_fifo(Type*, bool is_char);
    ~buf_fifo();

    virtual bool empty(); // throws efifowronly
    virtual void flush(); // throws efifordonly
};


class str_fifo: public buf_fifo
{
protected:
    str string;

public:
    str_fifo(Type*);
    str_fifo(Type*, const str&);
    ~str_fifo();

    bool empty();
    void flush();

    str all() const;
};


// Non-buffered file output, suitable for standard out/err, should be static
// in the program.
class std_file: public fifo_intf
{
protected:
    int _fd;
    virtual mem enq_chars(const char*, mem);
public:
    std_file(Type*, int fd);
    ~std_file();
};


extern std_file fout;
extern std_file ferr;


class in_text: public buf_fifo
{
protected:
    enum { BUF_SIZE = 1024 * sizeof(integer) };

    const str file_name;
    str  filebuf;
    int  _fd;
    bool _eof;

    void error(int code); // throws esyserr

public:
    in_text(Type*, const str& fn);
    ~in_text();
    
    bool empty(); //override
    str  get_file_name() const { return file_name; }
    void open()                { empty(); /* attempt to fill the buffer */ }
};


// --- LANGUAGE OBJECT ----------------------------------------------------- //


class State; // see typesys.h

class langobj: public object
{
    friend class State;

protected:

public:
    variant vars[0];

    ~langobj(); // TODO: finalize vars

    void* operator new(size_t);
    void operator delete (void *p);
};


langobj* new_langobj(State*);


#endif // __RUNTIME_H
