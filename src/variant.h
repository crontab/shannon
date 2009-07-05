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


class variant;
class object;
class tuple;
class dict;
class set;


DEF_EXCEPTION(evarianttype,  "Variant type mismatch")
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

    str& _str_write()                   { return *(str*)val._str; }
    const str& _str_read()      const { return *(str*)val._str; }
    tuple& _tuple_write();
    const tuple& _tuple_read()  const { return *val._tuple; }
    dict& _dict_write();
    const dict& _dict_read()    const { return *val._dict; }
    set& _set_write();
    const set& _set_read()      const { return *val._set; }

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
    void operator= (const variant& v)   { _fin(); _init(v); }
    bool operator== (const variant& v) const;
    bool operator!= (const variant& v)
                              const { return !(this->operator==(v)); }

    void dump(std::ostream&) const;
    str  to_string() const;
    bool operator< (const variant& v) const;

    bool is_null()            const { return type == NONE; }
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
    const str& as_str()       const { _req(STR); return _str_read(); }
    const tuple& as_tuple() const;
    const dict& as_dict() const;
    const set& as_set() const;
    object* as_object()       const { _req_obj(); return val._obj; }

    // Container operations
    int  size() const;                                  // str, tuple, dict, set
    bool empty() const;                                 // str, tuple, dict, set
    void append(const variant& v);                      // str
    void append(const str&);                            // str
    void append(const char*);                           // str
    void append(char c);                                // str
    str  substr(int start, int count = -1) const;       // str
    char getch(int) const;                              // str
    void push_back(const variant&);                     // tuple
    void insert(const variant&);                        // set
    void insert(int index, const variant&);             // tuple
    void put(const variant& key, const variant&);       // dict
    void erase(int index);                              // str, tuple, dict[int], set[int]
    void erase(int index, int count);                   // str, tuple
    void erase(const variant& key);                     // dict, set
    bool has(const variant& index) const;               // dict, set
    const variant& operator[] (int index) const;        // tuple, dict[int]
    const variant& operator[] (const variant& key) const;   // dict
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
    friend object* _clone(object*);

public:
#ifdef DEBUG
    static int alloc;
#endif

protected:
    int refcount;
    virtual object* clone() const = 0;
public:
    object();
    virtual ~object();
    bool is_unique() const  { return refcount == 1; }
    virtual void dump(std::ostream&) const;
    virtual bool less_than(object* o) const;
};


void release(object*&);

inline object* _grab(object* o)     { if (o) pincrement(&o->refcount); return o; }
template<class T>
    T* grab(T* o) { return (T*)_grab(o); }

object* _clone(object*);
template<class T>
    T* unique(T* o) { if (o->is_unique()) return (T*)o; return (T*)_clone(o); }


class tuple: public object
{
protected:
    typedef std::vector<variant> tuple_impl;

    tuple_impl impl;

    tuple(const tuple& other): impl(other.impl)  { }
    void operator= (const tuple&);
    virtual object* clone() const;
public:
    tuple();
    ~tuple();
    int size() const { return impl.size(); }
    bool empty() const { return impl.empty(); }
    void push_back(const variant&);
    void insert(int, const variant&);
    void erase(int);
    void erase(int index, int count);
    const variant& operator[] (int) const;
    virtual void dump(std::ostream&) const;
};


class dict: public object
{
protected:
    typedef std::map<variant, variant> dict_impl;

    dict_impl impl;

    dict(const dict& other): impl(other.impl)  { }
    void operator= (const dict&);
    virtual object* clone() const;
public:
    typedef dict_impl::const_iterator iterator;

    dict();
    ~dict();
    int size() const { return impl.size(); }
    bool empty() const { return impl.empty(); }
    void erase(const variant&);
    variant& operator[] (const variant&);
    iterator find(const variant&) const;
    virtual void dump(std::ostream&) const;
    iterator begin() const  { return impl.begin(); }
    iterator end() const  { return impl.end(); }
};


class set: public object
{
protected:
    typedef std::set<variant> set_impl;

    set_impl impl;

    set(const set& other): impl(other.impl)  { }
    void operator= (const set&);
    virtual object* clone() const;
public:
    typedef set_impl::const_iterator iterator;

    set();
    ~set();
    int size() const { return impl.size(); }
    bool empty() const { return impl.empty(); }
    void insert(const variant&);
    void erase(const variant&);
    iterator find(const variant&) const;
    virtual void dump(std::ostream&) const;
    iterator begin() const  { return impl.begin(); }
    iterator end() const  { return impl.end(); }
};


extern const variant null;
extern const tuple null_tuple;
extern const dict null_dict;
extern const set null_set;

#endif // __VARIANT_H
