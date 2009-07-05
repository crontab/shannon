
#define __STDC_LIMIT_MACROS

#include <stdint.h>
#include <assert.h>
#include <stdlib.h>
#include <limits.h>

#include <exception>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <sstream>
#include <iostream>

#include "common.h"
#include "variant.h"

const variant null;
const tuple null_tuple;
const dict null_dict;
const set null_set;


#ifdef DEBUG
int object::alloc = -3; // compensate the three static objects null_xxx above
#endif


object::object()
    : refcount(0)
{
#ifdef DEBUG
    pincrement(&object::alloc);
#endif
}


object::~object()
{
#ifdef DEBUG
    pdecrement(&object::alloc);
    assert(refcount == 0);
#endif
}


void object::dump(std::ostream& s) const
{
    s << "object";
}


bool object::less_than(object* other) const
{
    return this < other;
}


void release(object*& o)
{
    if (!o)
        return;
#ifdef DEBUG
    assert(o->refcount >= 1);
#endif
    if (pdecrement(&o->refcount) == 0)
        delete o;
    o = NULL;
}


#define CLONE(t) \
    object* t::clone() const { return new t(*this); }

CLONE(tuple)
CLONE(dict)
CLONE(set)


object* _clone(object* o)
{
    object* p = grab(o->clone());
    release(o);
    return p;
}



tuple::tuple()  { }
tuple::~tuple() { }


void tuple::erase(int i)                        { impl.erase(impl.begin() + i); }
void tuple::erase(int i, int count)             { impl.erase(impl.begin() + i,
                                                    impl.begin() + i + count - 1); }
const variant& tuple::operator[] (int i) const  { return impl[i]; }
void tuple::push_back(const variant& v)         { impl.push_back(v); }
void tuple::insert(int i, const variant& v)     { impl.insert(impl.begin() + i, v); }


void tuple::dump(std::ostream& s) const
{
    foreach(tuple_impl::const_iterator, i, impl)
    {
        if (i != impl.begin())
            s << ", ";
        s << *i;
    }
}


dict::dict()    { }
dict::~dict()   { }

variant& dict::operator[] (const variant& v)        { return impl[v]; }
dict::iterator dict::find(const variant& v) const   { return impl.find(v); }
void dict::erase(const variant& v)                  { impl.erase(v); }

void dict::dump(std::ostream& s) const
{
    foreach(dict_impl::const_iterator, i, impl)
    {
        if (i != impl.begin())
            s << ", ";
        s << i->first << ": " << i->second;
    }
}

set::set()      { }
set::~set()     { }

void set::erase(const variant& v)               { impl.erase(v); }
set::iterator set::find(const variant& v) const { return impl.find(v); }
void set::insert(const variant& v)              { impl.insert(v); }

void set::dump(std::ostream& s) const
{
    foreach(set_impl::const_iterator, i, impl)
    {
        if (i != impl.begin())
            s << ", ";
        s << *i;
    }
}


void variant::_init(const str& s)   { type = STR; ::new(&_str_write()) str(s); }
void variant::_init(const char* s)  { type = STR; ::new(&_str_write()) str(s); }
void variant::_init(tuple* t)       { type = TUPLE; val._obj = grab(t); }
void variant::_init(dict* d)        { type = DICT; val._obj = grab(d); }
void variant::_init(set* s)         { type = SET; val._obj = grab(s); }
void variant::_init(object* o)      { type = OBJECT; val._obj = grab(o); }


void variant::_init(const variant& other)
{
    type = other.type;
    switch (type)
    {
    case NONE:
        break;
    case INT:
    case REAL:
    case BOOL:
    case CHAR:
        val = other.val;
        break;
    case STR:
        ::new(&_str_write()) str(other._str_read());
        break;
    default:    // containers and objects
        val._obj = grab(other.val._obj);
        break;
    }
}


#ifdef RANGE_CHECKING

#define CHK_SIGNED(t) { if (val._int < t##_MIN && val._int > t##_MAX) _range_err(); }
#define CHK_UNSIGNED(t) { if (val._int < 0 && val._int > t##_MAX) _range_err(); }

integer variant::_in_signed(size_t s) const
{
    switch (s)
    {
    case 1: CHK_SIGNED(INT8); break;
    case 2: CHK_SIGNED(INT16); break;
    case 4: CHK_SIGNED(INT32); break;
    }
    return val._int;
}

integer variant::_in_unsigned(size_t s) const
{
    switch (s)
    {
    case 1: CHK_UNSIGNED(UINT8); break;
    case 2: CHK_UNSIGNED(UINT16); break;
    case 4: CHK_UNSIGNED(UINT32); break;
    case 8: CHK_UNSIGNED(INT64); break; // we don't support unsigned 64-bit
    }
    return val._int;
}

#endif // RANGE_CHECKING


void variant::_fin2()
{
    switch (type)
    {
    case NONE:
    case INT:
    case REAL:
    case BOOL:
    case CHAR:
        break;
    case STR:
        _str_write().~str();
        break;
    default:    // containers and objects
        release(val._obj);
        break;
    }
}


void variant::dump(std::ostream& s) const
{
    switch (type)
    {
    case NONE:
        s << "null";
        break;
    case INT:
        s << val._int;
        break;
    case REAL:
        s << val._real;
        break;
    case BOOL:
        s << (val._bool ? "true" : "false");
        break;
    case CHAR:
        s << '\'' << val._char << '\'';
        break;
    case STR:
        s << '"' << _str_read() << '"';
        break;
    default:    // containers and objects
        s << "[";
        if (val._obj != NULL)
            val._obj->dump(s);
        s << "]";
        break;
    }
}


str variant::to_string() const
{
    std::stringstream s;
    dump(s);
    return s.str();
}


bool variant::operator== (const variant& other) const
{
    _req(other.type);
    switch (type)
    {
    case NONE: return true;
    case INT:  return val._int == other.val._int;
    case REAL: return val._real == other.val._real;
    case BOOL: return val._bool == other.val._bool;
    case CHAR: return val._char == other.val._char;
    case STR:  return _str_read() == other._str_read();
    default:   return val._obj == other.val._obj;
    }
}


bool variant::operator< (const variant& other) const
{
    _req(other.type);
    switch (type)
    {
    case NONE: return false;
    case INT:  return val._int < other.val._int;
    case REAL: return val._real < other.val._real;
    case BOOL: return val._bool < other.val._bool;
    case CHAR: return val._char < other.val._char;
    case STR:  return _str_read() < other._str_read();
    default:    // containers and objects
        if (val._obj == NULL)
            return other.val._obj != NULL;
        if (other.val._obj == NULL)
            return false;
        return val._obj->less_than(other.val._obj);
    }
}


void variant::_type_err() { throw evarianttype(); }
void variant::_range_err() { throw evariantrange(); }
void variant::_index_err() { throw evariantindex(); }


const char* variant::_type_name(Type type)
{
    static const char* names[] = {"null", "integer", "real", "boolean",
        "string", "tuple", "dict", "set", "object"};
    if (type > OBJECT)
        type = OBJECT;
    return names[type];
}


// as_xxx(): return a const implementation of a container, may be one of the
// statuc null_xxx objects

#define ASX(t,n) const t& variant::as_##t() const \
    { _req(n); if (val._##t == NULL) return null_##t; return *val._##t; }
    
ASX(tuple, TUPLE)
ASX(dict, DICT)
ASX(set, SET)


// _xxx_write(): return a unique container implementation, create if necessary

#define XIMPL(t) t& variant::_##t##_write() \
    { if (val._##t == NULL) \
          val._##t = grab(new t()); \
      else \
          val._##t = unique(val._##t); \
      return *val._##t; }

XIMPL(tuple)
XIMPL(dict)
XIMPL(set)


int variant::size() const
{
    switch (type)
    {
    case STR:   return _str_read().size();
    case TUPLE: if (val._tuple == NULL) return 0; return _tuple_read().size();
    case DICT:  if (val._dict == NULL) return 0; return _dict_read().size();
    case SET:   if (val._set == NULL) return 0; return _set_read().size();
    default: _type_err(); return 0;
    }
}


bool variant::empty() const
{
    switch (type)
    {
    case STR:   return _str_read().empty();
    case TUPLE: if (val._tuple == NULL) return true; return _tuple_read().empty();
    case DICT:  if (val._dict == NULL) return true; return _dict_read().empty();
    case SET:   if (val._set == NULL) return true; return _set_read().empty();
    default: _type_err(); return false;
    }
}


char variant::getch(int i) const            { _req(STR); return _str_read()[i]; }
void variant::append(const str& s)          { _req(STR); _str_write().append(s); }
void variant::append(const char* s)         { _req(STR); _str_write().append(s); }
void variant::append(char c)                { _req(STR); _str_write().push_back(c); }
void variant::append(const variant& v)      { _req(STR); _str_write().append(v.as_str()); }
void variant::push_back(const variant& v)   { _req(TUPLE); _tuple_write().push_back(v); }
void variant::insert(const variant& v)      { _req(SET); _set_write().insert(v); }


str variant::substr(int index, int count) const
{
    _req(STR);
    if (count == -1)
        count = str::npos;
    return _str_read().substr(index, count);
}


void variant::insert(int index, const variant& v)
{
    _req(TUPLE);
    tuple& t = _tuple_write();
    if (index < 0 || index > int(t.size()))
        _index_err();
    t.insert(index, v);
}


void variant::put(const variant& key, const variant& value)
{
    _req(DICT);
    dict& d = _dict_write();
    if (value.is_null())
        d.erase(key);
    else
        d[key] = value;
}


void variant::erase(int index)
{
    switch (type)
    {
    case STR: _str_write().erase(index, 1); break;
    case TUPLE: _tuple_write().erase(index); break;
    case DICT: _dict_write().erase(index); break;
    case SET: _set_write().erase(index); break;
    default: _type_err(); break;
    }
}


void variant::erase(int index, int count)
{
    switch (type)
    {
    case STR: _str_write().erase(index, count); break;
    case TUPLE: _tuple_write().erase(index, count); break;
    default: _type_err(); break;
    }
}


void variant::erase(const variant& key)
{
    switch (type)
    {
    case DICT: _dict_write().erase(key); break;
    case SET: _set_write().erase(key); break;
    default: _type_err(); break;
    }
}


const variant& variant::operator[] (int index) const
{
    if (type == DICT)
        return this->operator[] (variant(index));
    _req(TUPLE);
    if (val._tuple == NULL)
        _index_err();
    if (index < 0 || index >= int(_tuple_read().size()))
        _index_err();
    return _tuple_read()[index];
}


const variant& variant::operator[] (const variant& index) const
{
    _req(DICT);
    if (val._dict == NULL)
        return null;
    dict::iterator it = _dict_read().find(index);
    if (it == _dict_read().end())
        return null;
    return it->second;
}


bool variant::has(const variant& index) const
{
    switch (type)
    {
    case DICT:
        {
            if (val._dict == NULL)
                return false;
            return _dict_read().find(index) != _dict_read().end();
        }
        break;
    case SET:
        {
            if (val._set == NULL)
                return false;
            return _set_read().find(index) != _set_read().end();
        }
    default: _type_err(); return false;
    }
}



