#ifndef __VARIANT_H
#define __VARIANT_H

#include <exception>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <iostream>

#include "common.h"

#if (defined(DEBUG) || defined(_DEBUG)) && !defined(RANGE_CHECKING)
#  define RANGE_CHECKING
#endif


#define foreach(type,iter,cont) \
    for (type iter = (cont).begin(); iter != (cont).end(); iter++)

#define vforeach(type,iter,cont) \
    for (type##_iterator iter = (cont).type##_begin(); iter != (cont).type##_end(); iter++)

#ifdef BOOL
#  error "BOOL defined somewhere conflicts with internal definitions in variant class"
#endif


class variant;
class object;
class tuple;
class dict;
class set;
class range;


typedef std::vector<variant> tuple_impl;
typedef std::map<variant, variant> dict_impl;
typedef dict_impl::const_iterator dict_iterator;
typedef std::set<variant> set_impl;
typedef set_impl::const_iterator set_iterator;


DEF_EXCEPTION(evarianttype,  "Variant type mismatch")
DEF_EXCEPTION(evariantrange, "Variant range check error")
DEF_EXCEPTION(evariantindex, "Variant array index out of bounds")


class None { int dummy; };


class variant
{
public:
    // Note: the order is important, especially after STR
    enum Type { NONE, BOOL, CHAR, INT, REAL, STR, RANGE, TUPLE, DICT, SET, OBJECT,
        NONPOD = STR, REFCNT = RANGE, ANYOBJ = OBJECT };

protected:
    Type type;
    union
    {
        bool    _bool;
        char    _char;
        integer _int;
        real    _real;
        char    _str[sizeof(str)];
        object* _obj;
        tuple*  _tuple;
        dict*   _dict;
        set*    _set;
        range*  _range;
    } val;

    str& _str_write()               { return *(str*)val._str; }
    const str& _str_read()    const { return *(str*)val._str; }
    range& _range_write();
    const range& _range_read() const { return *val._range; }
    tuple& _tuple_write();
    const tuple& _tuple_read() const { return *val._tuple; }
    dict& _dict_write();
    const dict& _dict_read()  const { return *val._dict; }
    set& _set_write();
    const set& _set_read()    const { return *val._set; }

    // Initializers/finalizers: used in constructors/destructors and assigments
    void _init()                    { type = NONE; }
    void _init(bool b)              { type = BOOL; val._bool = b; }
    void _init(char c)              { type = CHAR; val._char = c; }
    template<class T>
        void _init(T i)             { type = INT; val._int = i; }
    void _init(double r)            { type = REAL; val._real = r; }
    void _init(const str&);
    void _init(const char*);
    void _init(integer left, integer right);
    void _init(range*);
    void _init(tuple*);
    void _init(dict*);
    void _init(set*);
    void _init(object*);
    void _init(const variant&);
    void _fin()                     { if (is_nonpod()) _fin2(); }
    void _fin2();

    // Errors: type mismatch, out of INT range
    static void _type_err();
    static void _range_err();
    static void _index_err();
    void _req(Type t) const         { if (type != t) _type_err(); }
    void _req_obj() const           { if (!is_object()) _type_err(); }

    // Range checking
#ifdef RANGE_CHECKING
    integer _in_signed(size_t) const;
    integer _in_unsigned(size_t) const;
#else
    integer _in_signed(size_t) const { return val._int; }
    integer _in_unsigned(size_t) const { return val._int; }
#endif

public:
    variant()                       { _init(); }
    variant(None)                   { _init(); }
    template<class T>
        variant(const T& v)         { _init(v); }
    variant(integer l, integer r)   { _init(l, r); }
    variant(const variant& v)       { _init(v); }
    ~variant()                      { _fin(); }

    void operator=(None)            { _fin(); _init(); }
    template<class T>
    // TODO: check cases when the same value is assigned (e.g. v = v)
    void operator= (const T& v)     { _fin(); _init(v); }
    void operator= (const variant& v)   { _fin(); _init(v); }
    bool operator== (const variant& v) const;
    bool operator!= (const variant& v)
                              const { return !(this->operator==(v)); }

    void dump(std::ostream&) const;
    str  to_string() const;
    bool operator< (const variant& v) const;

    // TODO: is_int(int), is_real(real) ... or operator == maybe?
    bool is_null()            const { return type == NONE; }
    bool is_bool()            const { return type == BOOL; }
    bool is_char()            const { return type == CHAR; }
    bool is_int()             const { return type == INT; }
    bool is_real()            const { return type == REAL; }
    bool is_str()             const { return type == STR; }
    bool is_range()           const { return type == RANGE; }
    bool is_tuple()           const { return type == TUPLE; }
    bool is_dict()            const { return type == DICT; }
    bool is_set()             const { return type == SET; }
    bool is_nonpod()          const { return type >= NONPOD; }
    bool is_refcnt()          const { return type >= REFCNT; }
    bool is_object()          const { return type >= ANYOBJ; }

    // Type conversions
    // TODO: as_xxx(defualt)
    bool as_bool()            const { _req(BOOL); return val._bool; }
    char as_char()            const { _req(CHAR); return val._char; }
    integer as_int()          const { _req(INT); return val._int; }
    template<class T>
        T as_signed()         const { _req(INT); return (T)_in_signed(sizeof(T)); }
    template<class T>
        T as_unsigned()       const { _req(INT); return (T)_in_unsigned(sizeof(T)); }
    real as_real()            const { _req(REAL); return val._real; }
    const str& as_str()       const { _req(STR); return _str_read(); }
    const range& as_range() const;
    const tuple& as_tuple() const;
    const dict& as_dict() const;
    const set& as_set() const;
    object* as_object()       const { _req_obj(); return val._obj; }

    // Container operations
    mem  size() const;                                      // str, tuple, dict, set
    bool empty() const;                                     // str, tuple, dict, set
    void resize(mem);                                       // str, tuple
    void resize(mem, char);                                 // str
    void append(const variant& v);                          // str
    void append(const str&);                                // str
    void append(const char*);                               // str
    void append(char c);                                    // str
    str  substr(mem start, mem count = mem(-1)) const;      // str
    char getch(mem) const;                                  // str
    void push_back(const variant&);                         // tuple
    void insert(mem index, const variant&);                 // tuple
    void put(mem index, const variant&);                    // tuple
    void tie(const variant& key, const variant&);           // dict
    void tie(const variant&);                               // set
    void assign(integer left, integer right);               // range
    void erase(mem index);                                  // str, tuple, dict[int], set[int]
    void erase(mem index, mem count);                       // str, tuple
    void untie(const variant& key);                         // set, dict
    bool has(const variant& index) const;                   // dict, set
    integer left() const;                                   // range
    integer right() const;                                  // range
    const variant& operator[] (mem index) const;            // tuple, dict[int]
    const variant& operator[] (const variant& key) const;   // dict

    dict_iterator dict_begin() const;
    dict_iterator dict_end() const;
    set_iterator set_begin() const;
    set_iterator set_end() const;
    
    // for unit tests
    bool is_null_ptr() const { return val._obj == NULL; }
};


inline std::ostream& operator<< (std::ostream& s, const variant& v)
    { v.dump(s); return s; }


class object: public noncopyable
{
    friend class variant;
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
    virtual object* clone() const; // calls fatal()
public:
    object();
    virtual ~object();
    bool is_unique() const  { return refcount == 1; }
    virtual void dump(std::ostream&) const;
    virtual bool less_than(object* o) const;
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
    friend class variant;
    friend range* new_range(integer, integer);
protected:
    integer left;
    integer right;

    range(): left(0), right(-1)  { }
    range(integer l, integer r): left(l), right(r)  { }
    range(const range& other): left(other.left), right(other.right)  { }
    ~range();
    virtual object* clone() const;
    void assign(integer l, integer r)  { left = l; right = r; }
    bool empty() const  { return left > right; }
    bool has(integer i) const  { return i >= left && i <= right; }
    virtual void dump(std::ostream&) const;
    bool equals(const range& other) const;
    virtual bool less_than(object* o) const;
};


class tuple: public object
{
    friend class variant;

    tuple_impl impl;

protected:
    tuple();
    tuple(const tuple& other): impl(other.impl)  { }
    ~tuple();

    virtual object* clone() const;
    mem size()                          const { return impl.size(); }
    bool empty()                        const { return impl.empty(); }
    void resize(mem n)                        { impl.resize(n); }
    void push_back(const variant& v)          { impl.push_back(v); }
    void insert(mem i, const variant& v)      { impl.insert(impl.begin() + i, v); }
    void put(mem i, const variant& v)         { impl[i] = v; }
    void erase(mem i)                         { impl.erase(impl.begin() + i); }
    void erase(mem index, mem count);
    const variant& operator[] (mem i)   const { return impl[i]; }
    virtual void dump(std::ostream&) const;
};


class dict: public object
{
    friend class variant;

    dict_impl impl;

protected:
    dict();
    dict(const dict& other): impl(other.impl)  { }
    ~dict();

    virtual object* clone() const;
    mem size()                          const { return impl.size(); }
    bool empty()                        const { return impl.empty(); }
    void erase(const variant& v)              { impl.erase(v); }
    variant& operator[] (const variant& v)    { return impl[v]; }
    dict_iterator find(const variant& v)const { return impl.find(v); }
    virtual void dump(std::ostream&) const;
    dict_iterator begin()               const { return impl.begin(); }
    dict_iterator end()                 const { return impl.end(); }
};


// TODO: optimize implementation of set for ordinals within 0..63 and 0..255
class set: public object
{
    friend class variant;

    set_impl impl;

protected:
    set();
    set(const set& other): impl(other.impl)  { }
    ~set();

    virtual object* clone() const;
    mem size()                          const { return impl.size(); }
    bool empty()                        const { return impl.empty(); }
    void insert(const variant& v)             { impl.insert(v); }
    void erase(const variant& v)              { impl.erase(v); }
    set_iterator find(const variant& v) const { return impl.find(v); }
    virtual void dump(std::ostream&) const;
    set_iterator begin()                const { return impl.begin(); }
    set_iterator end()                  const { return impl.end(); }
};


class varstack: protected tuple_impl, public noncopyable
{
public:
    varstack() { }
    ~varstack() { }
    void push(const variant& v)     { push_back(v); }
    void pushn(mem n)               { resize(size() + n); }
    variant& top()                  { return back(); }
    variant& top(mem n)             { return *(end() - n); }
    void pop()                      { pop_back(); }
    void popn(mem n)                { resize(size() - n); }
};


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
    operator T*() const                 { return obj; }
    bool operator== (T* o) const        { return obj == o; }
    bool operator== (const objptr<T>& p) const { return obj == p.obj; }
    bool operator!= (T* p) const        { return obj != p; }

    friend inline
    bool operator== (T* o, const objptr<T>& p) { return o == p.obj; }
};


inline range* new_range()   { return NULL; }
inline tuple* new_tuple()   { return NULL; }
inline dict* new_dict()     { return NULL; }
inline set* new_set()       { return NULL; }
range* new_range(integer l, integer r);

inline dict_iterator variant::dict_begin() const  { _req(DICT); return _dict_read().begin(); }
inline dict_iterator variant::dict_end() const    { _req(DICT); return _dict_read().end(); }

inline set_iterator variant::set_begin() const    { _req(SET); return _set_read().begin(); }
inline set_iterator variant::set_end() const      { _req(SET); return _set_read().end(); }

extern const variant null;

#endif // __VARIANT_H

