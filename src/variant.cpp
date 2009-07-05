
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

const variant none;
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
    s << "<object>";
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


tuple::tuple()  { }
tuple::~tuple() { }

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

void set::dump(std::ostream& s) const
{
    foreach(set_impl::const_iterator, i, impl)
    {
        if (i != impl.begin())
            s << ", ";
        s << *i;
    }
}


void variant::_init(const str& s)   { type = STR; ::new(&_str_impl()) str(s); }
void variant::_init(const char* s)  { type = STR; ::new(&_str_impl()) str(s); }
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
        ::new(&_str_impl()) str(other._str_impl());
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
        _str_impl().~str();
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
        s << "none";
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
        s << '"' << _str_impl() << '"';
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


bool variant::operator< (const variant& other) const
{
    _req(other.type);
    switch (type)
    {
    case NONE: return false;
    case INT: return val._int < other.val._int;
    case REAL: return val._real < other.val._real;
    case BOOL: return val._bool < other.val._bool;
    case CHAR: return val._char < other.val._char;
    case STR: return _str_impl() < other._str_impl();
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
    static const char* names[] = {"none", "integer", "real", "boolean",
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


// _xxx_impl(): return a container implementation, create if necessary

#define XIMPL(t) t##_impl& variant::_##t##_impl() \
    { if (val._##t == NULL) val._##t = grab(new t()); return val._##t->impl; }

XIMPL(tuple)
XIMPL(dict)
XIMPL(set)


int variant::size() const
{
    if (type == STR)
        return _str_impl().size();
    switch (type)
    {
    case TUPLE: if (val._tuple == NULL) return 0; return val._tuple->impl.size();
    case DICT:  if (val._dict == NULL) return 0; return val._dict->impl.size();
    case SET:   if (val._set == NULL) return 0; return val._set->impl.size();
    default: _type_err(); return 0;
    }
}


bool variant::empty() const
{
    if (type == STR)
        return _str_impl().empty();
    switch (type)
    {
    case TUPLE: if (val._tuple == NULL) return true; return val._tuple->impl.empty();
    case DICT:  if (val._dict == NULL) return true; return val._dict->impl.empty();
    case SET:   if (val._set == NULL) return true; return val._set->impl.empty();
    default: _type_err(); return false;
    }
}


void variant::cat(const str& s)     { _req(STR); _str_impl().append(s); }
void variant::cat(const char* s)    { _req(STR); _str_impl().append(s); }
void variant::cat(char c)           { _req(STR); _str_impl().push_back(c); }
void variant::cat(const variant& v) { _req(STR); _str_impl().append(v.as_str()); }


str variant::sub(int index, int count) const
{
    _req(STR);
    if (count == -1)
        count = str::npos;
    return _str_impl().substr(index, count);
}


void variant::add(const variant& v)
{
    switch (type)
    {
    case TUPLE: _tuple_impl().push_back(v);  break;
    case SET: _set_impl().insert(v); break;
    default: _type_err(); break;
    }
}


void variant::ins(const variant& v)
{
    switch (type)
    {
    case TUPLE:
        {
            tuple_impl& t = _tuple_impl();
            t.insert(t.begin(), v);
        }
        break;
    case SET: _set_impl().insert(v); break;
    default: _type_err(); break;
    }
}


void variant::ins(int index, const variant& v)
{
    tuple_impl& t = _tuple_impl();
    if (index < 0 || index > int(t.size()))
        _index_err();
    t.insert(t.begin() + index, v);
}


void variant::put(const variant& key, const variant& value)
{
    _req(DICT);
    dict_impl& d = _dict_impl();
    if (value.is_none())
        d.erase(key);
    else
        d[key] = value;
}


void variant::del(int index)
{
    switch (type)
    {
    case TUPLE:
        {
            tuple_impl& t = _tuple_impl();
            t.erase(t.begin() + index);
        }
        break;
    case STR:
        {
            str& s = _str_impl();
            s.erase(s.begin() + index);
        }
        break;
    default: _type_err(); break;
    }
}


void variant::del(int index, int count)
{
    switch (type)
    {
    case TUPLE:
        {
            tuple_impl& t = _tuple_impl();
            t.erase(t.begin() + index, t.begin() + count - 1);
        }
        break;
    case STR:
        {
            str& s = _str_impl();
            s.erase(s.begin() + index, s.begin() + count - 1);
        }
        break;
    default: _type_err(); break;
    }
}


void variant::del(const variant& key)
{
    switch (type)
    {
    case DICT: _dict_impl().erase(key); break;
    case SET: _set_impl().erase(key); break;
    default: _type_err(); break;
    }
}


const variant& variant::get(int index) const
{
    _req(TUPLE);
    if (val._tuple == NULL)
        _index_err();
    const tuple_impl& t = val._tuple->impl;
    if (index < 0 || index >= int(t.size()))
        _index_err();
    return t[index];
}


const variant& variant::get(const variant& index) const
{
    _req(DICT);
    if (val._dict == NULL)
        return none;
    const dict_impl& d = val._dict->impl;
    dict_impl::const_iterator it = d.find(index);
    if (it == d.end())
        return none;
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
            const dict_impl& d = val._dict->impl;
            return d.find(index) != d.end();
        }
        break;
    case SET:
        {
            if (val._set == NULL)
                return false;
            const set_impl& s = val._set->impl;
            return s.find(index) != s.end();
        }
    default: _type_err(); return false;
    }
}



