
#define __STDC_LIMIT_MACROS


#include "runtime.h"

#include <string.h>


const variant null;
const str null_str;


void variant::_init(const str& s)   { type = STR; ::new(&_strw()) str(s); }
void variant::_init(const char* s)  { type = STR; ::new(&_strw()) str(s); }
void variant::_init(object* o)      { type = OBJ; val._obj = grab(o); }


void variant::_init(const variant& other)
{
    type = other.type;
    switch (type)
    {
    case NONE: break;
    case ORD:  val._ord = other.val._ord; break;
    case REAL: val._real = other.val._real; break;
    case STR:
        ::new(&_strw()) str(other._str());
        break;
    case OBJ:
        val._obj = grab(other.val._obj);
        break;
    }
}

/*
#ifdef RANGE_CHECKING

#define CHK_SIGNED(t) { if (val._int < t##_MIN && val._int > t##_MAX) _range_err(); }
#define CHK_UNSIGNED(t) { if (val._int < 0 && val._int > t##_MAX) _range_err(); }

integer variant::_in_signed(integer s) const
{
    switch (s)
    {
    case 1: CHK_SIGNED(INT8); break;
    case 2: CHK_SIGNED(INT16); break;
#ifndef SH64
    case 4: CHK_SIGNED(INT32); break;
#endif
    }
    return val._int;
}

integer variant::_in_unsigned(integer s) const
{
    switch (s)
    {
    case 1: CHK_UNSIGNED(UINT8); break;
    case 2: CHK_UNSIGNED(UINT16); break;
#ifdef SH64
    case 4: CHK_UNSIGNED(UINT32); break;
    case 8: CHK_UNSIGNED(INT64); break; // we don't support unsigned 64-bit
#else
    case 4: CHK_UNSIGNED(INT32); break;
#endif
    }
    return val._int;
}

#endif // RANGE_CHECKING
*/


void variant::_fin2()
{
    switch (type)
    {
    case NONE:
    case ORD:
    case REAL:
        break;
    case STR:
        _strw().~str();
        break;
    case OBJ:
        release(val._obj);
        break;
    }
}


bool variant::operator== (const variant& other) const
{
    if (type != other.type)
        return false;
    switch (type)
    {
    case NONE:      return true;
    case ORD:       return val._ord == other.val._ord;
    case REAL:      return val._real == other.val._real;
    case STR:       return _str() == other._str();
    case OBJ:    return val._obj == other.val._obj; // TODO: a virtual call?
    }
    return false;
}


bool variant::operator< (const variant& other) const
{
    if (type != other.type)
        return type < other.type;
    switch (type)
    {
    case NONE: return false;
    case ORD:  return val._ord < other.val._ord;
    case REAL: return val._real < other.val._real;
    case STR:  return _str() < other._str();
    case OBJ: return val._obj < other.val._obj;
    }
    return true;
}


void variant::_type_err() { throw emessage("Variant type mismatch"); }
void variant::_range_err() { throw emessage("Variant range error"); }
void variant::_index_err() { throw emessage("Variant index error"); }


unsigned variant::as_char_int() const
{
    integer i = as_ord();
    if (i < 0 || i >= charset::BITS)
        _range_err();
    return i;
}


unsigned variant::as_bool_int() const
{
    integer i = as_ord();
    if (i < 0 || i > 1)
        _range_err();
    return i;
}


bool variant::empty() const
{
    switch (type)
    {
    case NONE:  return true;
    case ORD:   return val._ord == 0;
    case REAL:  return val._real == 0.0;
    case STR:   return _str().empty();
    case OBJ:   return val._obj == NULL || val._obj->empty();
    }
    return true;
}


void varswap(variant* v1, variant* v2)
{
    podvar t = *(podvar*)v1;
    *(podvar*)v1 = *(podvar*)v2;
    *(podvar*)v2 = t;
}


// --- OBJECT -------------------------------------------------------------- //

int object::alloc = 0;


object::object(Type* rt)
    : refcount(0), runtime_type(rt)
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


bool object::empty()                    const { return false; }


void _release(object* o)
{
    if (o == NULL)
        return;
#ifdef DEBUG
    assert(o->refcount >= 1);
#endif
    if (pdecrement(&o->refcount) == 0)
        delete o;
}


void _replace(object*& p, object* o)
{
    if (p != o)
    {
        release(p);
        p = grab(o);
    }
}


// --- CONTAINERS ---------------------------------------------------------- //


range::range(Type* rt)
    : object(rt), left(0), right(-1)  { }
range::range(Type* rt, integer l, integer r)
    : object(rt), left(l), right(r)  { }
range::range(const range& other)
    : object(other.runtime_type), left(other.left), right(other.right)  { }
range::~range()  { }

bool range::equals(integer l, integer r) const
{
    return left == l && right == r;
}

bool range::equals(const range& other) const
{
    return left == other.left && right == other.right;
}

/*
bool range::less_than(object* o) const
{
    const range& other = *(range*)o;
    if (left < other.left)
        return true;
    if (left > other.left)
        return false;
    return right < other.right;
}
*/

varlist::varlist()                          { }
varlist::varlist(const varlist& other)      : impl(other.impl)  { }
void varlist::pop_back()                    { impl.pop_back(); }
void varlist::push_back(const variant& v)   { impl.push_back(v); }
void varlist::put(mem i, const variant& v)  { impl[i] = v; }
void varlist::resize(mem s)                 { impl.resize(s); }
void varlist::clear()                       { impl.clear(); }
void varlist::append(const varlist& other)
    { impl.insert(impl.end(), other.impl.begin(), other.impl.end()); }


void varlist::insert(mem i, const variant& v)
{
    if (i > impl.size())
        throw emessage("Index overflow");
    impl.insert(impl.begin() + i, v);
}


void varlist::erase(mem i)
{
    if (i >= impl.size())
        throw emessage("Index overflow");
    impl.erase(impl.begin() + i);
}


void varlist::erase(mem i, mem count)
{
    mem s = impl.size();
    if (i >= s)
        throw emessage("Index overflow");
    if (i + count >= s)
    {
        count = s - i;
        if (count == 0)
            return;
    }
    impl.erase(impl.begin() + i, impl.begin() + i + count - 1);
}


varstack::varstack()            { }
varstack::~varstack()           { if (!empty()) _err(); }
void varstack::_err()           { fatal(0x1001, "varstack error"); }
void varstack::push(variant v)  { push_back((podvar&)v); }
void varstack::pop()            { ((variant&)back()).~variant(); pop_back(); }


variant* varstack::reserve(mem n)
{
    mem s = size();
    resize(s + n);
    return &(variant&)podvar_impl::operator[](s);
}


void varstack::free(mem n)
{
    mem s = size();
    if (n > s) _err();
    resize(s - n);
}


vector::vector(Type* rt)
    : object(rt)  { }
vector::vector(const vector& other)
    : object(other.runtime_type), varlist(other)  { }
vector::~vector()  { }

vector::vector(Type* rt, mem count, const variant& v)
    : object(rt)
{
    while (count--)
        push_back(v);
}

dict::dict(Type* rt)
    : object(rt)  { }
dict::dict(const dict& other)
    : object(other.runtime_type), impl(other.impl)  { }
dict::~dict()  { }
void dict::tie(const variant& key, const variant& value)
                                                    { impl[key] = value; }
void dict::untie(const variant& v)                  { impl.erase(v); }
dict_iterator dict::find(const variant& v) const    { return impl.find(v); }
bool dict::has(const variant& key) const            { return impl.find(key) != impl.end(); }


ordset::ordset(Type* rt)
    : object(rt)  { }
ordset::ordset(const ordset& other)
    : object(other.runtime_type), impl(other.impl)  { }
ordset::~ordset()  { }


set::set(Type* rt)
    : object(rt)  { }
set::set(const set& other)
    : object(other.runtime_type), impl(other.impl)  { }
set::~set()  { }
void set::tie(const variant& v)             { impl.insert(v); }
void set::untie(const variant& v)           { impl.erase(v); }
bool set::has(const variant& v) const       { return impl.find(v) != impl.end(); }

