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


#if !defined(foreach)
# define foreach(type,var,cont) \
    for (type var = (cont).begin(); var != (cont).end(); var++)
#endif


typedef std::string str;
typedef int64_t integer;
typedef double real;


class variant;
class object;
class tuple;
class dict;
class set;


typedef std::vector<variant> tuple_impl;
typedef std::map<variant, variant> dict_impl;
typedef std::set<variant> set_impl;


DEF_EXCEPTION(evarianttype, "Variant type mismatch")
DEF_EXCEPTION(evariantrange, "Variant range check error")
DEF_EXCEPTION(evariantindex, "Variant array index out of bounds")


class None { int dummy; };


class variant
{
public:
    // Note: the order is important, especially after STR
    enum Type { NONE, INT, REAL, BOOL, CHAR, STR, TUPLE, DICT, SET, OBJECT,
        NONPOD = STR, REFCNT = TUPLE, ANYOBJ = OBJECT };

protected:
    Type type;
    union
    {
        integer _int;
        real    _real;
        bool    _bool;
        char    _char;
        char    _str[sizeof(str)];
        object* _obj;
        tuple*  _tuple;
        dict*   _dict;
        set*    _set;
    } val;

    str& _str_impl()                { return *(str*)val._str; }
    const str& _str_impl()    const { return *(str*)val._str; }
    tuple_impl& _tuple_impl();
    dict_impl& _dict_impl();
    set_impl& _set_impl();

    // Initializers/finalizers: used in constructors/destructors and assigments
    void _init()                    { type = NONE; }
    template<class T>
        void _init(T i)             { type = INT; val._int = i; }
    void _init(double r)            { type = REAL; val._real = r; }
    void _init(bool b)              { type = BOOL; val._bool = b; }
    void _init(char c)              { type = CHAR; val._char = c; }
    void _init(const str&);
    void _init(const char*);
    void _init(tuple*);
    void _init(dict*);
    void _init(set*);
    void _init(object*);
    void _init(const variant&);
    void _fin()                     { if (is_nonpod()) _fin2(); }
    void _fin2();

    // Unrecoverable errors: type mismatch, out of INT range
    static const char* _type_name(Type);
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
    variant(const variant& v)       { _init(v); }
    ~variant()                      { _fin(); }

    void operator=(None)            { _fin(); _init(); }
    template<class T>
    void operator= (const T& v)     { _fin(); _init(v); }
    void operator= (const variant& v)  { _fin(); _init(v); }

    void dump(std::ostream&) const;
    str  to_string() const;
    bool operator< (const variant& v) const;

    bool is_none()            const { return type == NONE; }
    bool is_int()             const { return type == INT; }
    bool is_real()            const { return type == REAL; }
    bool is_bool()            const { return type == BOOL; }
    bool is_char()            const { return type == CHAR; }
    bool is_str()             const { return type == STR; }
    bool is_tuple()           const { return type == TUPLE; }
    bool is_dict()            const { return type == DICT; }
    bool is_set()             const { return type == SET; }
    bool is_object()          const { return type >= ANYOBJ; }
    bool is_nonpod()          const { return type >= NONPOD; }

    // Type conversions
    integer as_int()          const { _req(INT); return val._int; }
    template<class T>
        T as_signed()         const { _req(INT); return (T)_in_signed(sizeof(T)); }
    template<class T>
        T as_unsigned()       const { _req(INT); return (T)_in_unsigned(sizeof(T)); }
    real as_real()            const { _req(REAL); return val._real; }
    bool as_bool()            const { _req(BOOL); return val._bool; }
    char as_char()            const { _req(CHAR); return val._char; }
    const str& as_str()       const { _req(STR); return _str_impl(); }
    const tuple& as_tuple() const;
    const dict& as_dict() const;
    const set& as_set() const;
    object* as_object()       const { _req_obj(); return val._obj; }

    // Container operations
    int  size() const;                                  // str, tuple, dict, set
    bool empty() const;                                 // str, tuple, dict, set
    void cat(const variant& v);                         // str
    void cat(const str&);                               // str
    void cat(const char*);                              // str
    void cat(char c);                                   // str
    str  sub(int start, int count = -1) const;          // str
    void add(const variant&);                           // tuple, set
    void ins(const variant&);                           // tuple, set
    void ins(int index, const variant&);                // tuple
    void put(const variant& key, const variant&);       // dict
    void del(int index);                                // str, tuple
    void del(int index, int count);                     // str, tuple
    void del(const variant& key);                       // dict, set
    const variant& get(int index) const;                // tuple
    const variant& get(const variant& key) const;       // dict
    bool has(const variant& index) const;               // dict, set
    
    const variant& operator[] (int index) const { return get(index); }          // tuple
    const variant& operator[] (const variant& key) const { return get(key); }   // dict
};


inline tuple* new_tuple()   { return NULL; }
inline dict* new_dict()     { return NULL; }
inline set* new_set()       { return NULL; }


inline std::ostream& operator<< (std::ostream& s, const variant& v)
    { v.dump(s); return s; }


class object
{
    friend class variant;
    friend void release(object*&);
    friend object* _grab(object*);

public:
#ifdef DEBUG
    static int alloc;
#endif

protected:
    int refcount;
    
    virtual void dump(std::ostream&) const;
    virtual bool less_than(object* o) const;

public:
    object();
    virtual ~object();
    int get_refcount() const  { return refcount; }
};


void release(object*&);
inline object* _grab(object* o) { if (o) pincrement(&o->refcount); return o; }
template<class T>
    T* grab(T* o) { return (T*)_grab(o); }


class tuple: public object
{
    friend class variant;

    tuple(const tuple&);
    void operator= (const tuple&);
protected:
    tuple_impl impl;
    virtual void dump(std::ostream&) const;
public:
    tuple();
    ~tuple();

    typedef tuple_impl::const_iterator iterator;
    iterator begin() const  { return impl.begin(); }
    iterator end() const  { return impl.end(); }
};


class dict: public object
{
    friend class variant;

    dict(const dict&);
    void operator= (const dict&);
protected:
    dict_impl impl;
    virtual void dump(std::ostream&) const;
public:
    dict();
    ~dict();
    typedef dict_impl::const_iterator iterator;
    iterator begin() const  { return impl.begin(); }
    iterator end() const  { return impl.end(); }
};


class set: public object
{
public:
    friend class variant;

    set(const set&);
    void operator= (const set&);
protected:
    set_impl impl;
    virtual void dump(std::ostream&) const;
public:
    set();
    ~set();
    typedef set_impl::const_iterator iterator;
    iterator begin() const  { return impl.begin(); }
    iterator end() const  { return impl.end(); }
};


extern const variant none;
extern const tuple null_tuple;
extern const dict null_dict;
extern const set null_set;

#endif // __VARIANT_H
