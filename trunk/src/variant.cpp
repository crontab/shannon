
#define __STDC_LIMIT_MACROS


#include "runtime.h"


const variant null;
const str null_str;


void variant::_init(const str& s)   { type = STR; ::new(&_str_write()) str(s); }
void variant::_init(const char* s)  { type = STR; ::new(&_str_write()) str(s); }
void variant::_init(object* o)      { type = OBJECT; val._obj = grab(o); }


void variant::_init(const variant& other)
{
    type = other.type;
    switch (type)
    {
    case NONE:
        break;
    case BOOL:
    case CHAR:
    case INT:  val._int = other.val._int; break;
    case REAL: val._real = other.val._real; break;
    case STR:
        ::new(&_str_write()) str(other._str_read());
        break;
    case OBJECT:
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
    case BOOL:
    case CHAR:
    case INT:
    case REAL:
        break;
    case STR:
        _str_write().~str();
        break;
    default:    // containers and objects
        release(val._obj);
        break;
    }
}


void variant::dump(fifo_intf& s) const
{
    switch (type)
    {
    case NONE: s << "null"; break;
    case BOOL: s << (val._int ? "true" : "false"); break;
    case CHAR: s << '\'' << uchar(val._int) << '\''; break;
    case INT:  s << val._int; break;
    case REAL: s << integer(val._real); break; // TODO: !!!
    case STR:  s << '"' << _str_read() << '"'; break;
    default:    // containers and objects
        s << '[';
        if (val._obj != NULL)
            val._obj->dump(s);
        s << ']';
        break;
    }
}


str variant::to_string() const
{
    str_fifo s(NULL);
    dump(s);
    return s.all();
}


bool variant::operator== (const variant& other) const
{
    if (type != other.type)
        return false;
    switch (type)
    {
    case NONE:      return true;
    case BOOL:
    case CHAR:
    case INT:       return val._int == other.val._int;
    case REAL:      return val._real == other.val._real;
    case STR:       return _str_read() == other._str_read();
    case OBJECT:    return val._obj == other.val._obj; // TODO: a virtual call?
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
    case BOOL:
    case CHAR:
    case INT:  return val._int < other.val._int;
    case REAL: return val._real < other.val._real;
    case STR:  return _str_read() < other._str_read();
    case OBJECT:
        if (val._obj == NULL)
            return other.val._obj != NULL;
        if (other.val._obj == NULL)
            return false;
        return val._obj->less_than(other.val._obj);
    }
    return true;
}


bool variant::is_unique() const
{
    return !is_refcnt() || val._obj == NULL || val._obj->is_unique();
}


void variant::_type_err() { throw emessage("Variant type mismatch"); }
void variant::_range_err() { throw emessage("Variant range error"); }
void variant::_index_err() { throw emessage("Variant index error"); }


unsigned variant::as_char_int() const
{
    integer i = as_ordinal();
    if (i < 0 || i >= charset::BITS)
        _range_err();
    return i;
}


bool variant::empty() const
{
    switch (type)
    {
    case NONE:      return true;
    case BOOL:
    case CHAR:
    case INT:       return val._int == 0;
    case REAL:      return val._real == 0.0;
    case STR:       return _str_read().empty();
    case OBJECT:    return val._obj == NULL || val._obj->empty();
    }
    return true;
}


fifo_intf& operator<< (fifo_intf& s, const variant& v)
{
    v.dump(s);
    return s;
}


// --- OBJECT -------------------------------------------------------------- //

#ifdef DEBUG
int object::alloc = 0;
#endif


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


object* object::clone()                 const { throw emessage("Object can't be cloned"); }
bool object::empty()                          { return false; }
void object::dump(fifo_intf& s)         const { s << "object"; }
bool object::less_than(object* other)   const { return this < other; }


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


void _unique(object*& o)
{
    object* p = grab(o->clone());
    release(o);
    o = p;
}


// --- CONTAINERS ---------------------------------------------------------- //

#define XCLONE(t) \
    object* t::clone() const { return new t(*this); }

XCLONE(range)
XCLONE(tuple)
XCLONE(dict)
XCLONE(ordset)
XCLONE(set)


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

bool range::less_than(object* o) const
{
    const range& other = *(range*)o;
    if (left < other.left)
        return true;
    if (left > other.left)
        return false;
    return right < other.right;
}

void range::dump(fifo_intf& s) const
{
    s << left << ".." << right;
}


varlist::varlist()                          { }
varlist::varlist(const varlist& other)      : impl(other.impl)  { }
void varlist::pop_back()                    { impl.pop_back(); }
void varlist::push_back(const variant& v)   { impl.push_back(v); }
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


tuple::tuple(Type* rt)
    : object(rt)  { }
tuple::tuple(const tuple& other)
    : object(other.runtime_type), varlist(other)  { }
tuple::~tuple()  { }

tuple::tuple(Type* rt, mem count, const variant& v)
    : object(rt)
{
    while (count--)
        push_back(v);
}

void tuple::dump(fifo_intf& s) const
{
    for(mem i = 0; i < size(); i++)
    {
        if (i != 0)
            s << ", ";
        s << (*this)[i];
    }
}


dict::dict(Type* rt)
    : object(rt)  { }
dict::dict(const dict& other)
    : object(other.runtime_type), impl(other.impl)  { }
dict::~dict()   { }

void dict::dump(fifo_intf& s) const
{
    foreach(dict_impl::const_iterator, i, impl)
    {
        if (i != impl.begin())
            s << ", ";
        s << i->first << ": " << i->second;
    }
}


ordset::ordset(Type* rt)
    : object(rt)  { }
ordset::ordset(const ordset& other)
    : object(other.runtime_type), impl(other.impl)  { }
ordset::~ordset()     { }

void ordset::dump(fifo_intf& s) const
{
    int cnt = 0;
    for (int i = 0; i < charset::BITS; i++)
        if (impl[i])
        {
            if (++cnt > 1)
                s << ", ";
            s << integer(i);
        }
}


set::set(Type* rt)
    : object(rt)  { }
set::set(const set& other)
    : object(other.runtime_type), impl(other.impl)  { }
set::~set()     { }

void set::dump(fifo_intf& s) const
{
    foreach(set_impl::const_iterator, i, impl)
    {
        if (i != impl.begin())
            s << ", ";
        s << *i;
    }
}

