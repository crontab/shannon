

#include "common.h"
#include "runtime.h"


// --- range --------------------------------------------------------------- //


class range: public noncopyable
{
protected:

    struct cont: public object
    {
        integer left;
        integer right;
        cont(): left(0), right(-1)  { }
        cont(integer l, integer r): left(l), right(r)  { }
    };

    cont* obj;

public:
    range(): obj(&null)  { }
    range(const range& r): obj(r.obj->ref<cont>())  { }
    range(integer l, integer r);
    ~range();
    
    bool empty() const  { return obj->left > obj->right; }
    void operator= (const range& r);
/*
    void assign(integer l, integer r)   { left = l; right = r; }
    uinteger diff() const               { return right - left; }
    bool has(integer i) const           { return i >= left && i <= right; }
    bool equals(integer l, integer r) const
        { return left == l && right == r; }
    bool equals(const range& other) const
        { return left == other.left && right == other.right; }
*/
    int compare(const range&) const;

    static cont null;
};


// --- range --------------------------------------------------------------- //


range::cont range::null;


range::range(integer l, integer r)
    : obj((new cont(l, r))->ref<cont>())  { }

range::~range()
    { if (!empty()) obj->release(); }


int range::compare(const range& r) const
{
    int result = int(obj->left - r.obj->left);
    if (result == 0)
        result = int(obj->right - r.obj->right);
    return result;
}


// --- ordset -------------------------------------------------------------- //


class ordset: public object, public charset
{
    ordset(const ordset&);
public:
    ordset(): object() { }
};


// --- variant ------------------------------------------------------------- //


class variant: public noncopyable
{
    friend void test_variant();

public:

    enum Type
        { NONE, ORD, REAL, STR, VEC, SET, ORDSET, DICT, RANGE, OBJ,
            REFCNT = STR };

protected:

    Type type;
    union
    {
        integer  _ord;      // int, char and bool
        real     _real;
        object*  _obj;
    } val;

    typedef vector<variant> vec_t;
    typedef set<variant> set_t;
    typedef dict<variant, variant> dict_t;

    static void _type_err();
    static void _range_err();
#ifdef DEBUG
    void _dbg(Type t) const             { if (type != t) _type_err(); }
#else
    void _dbg(Type t) const             { }
#endif

    void _fin()                         { if (is_refcnt()) val._obj->release(); }

public:
    variant(): type(NONE)               { }
    variant(const variant& v)
        : type(v.type), val(v.val)      { if (is_refcnt()) v.val._obj->ref(); }
    variant(bool v): type(ORD)          { val._ord = v; }
    variant(char v): type(ORD)          { val._ord = uchar(v); }
    variant(uchar v): type(ORD)         { val._ord = v; }
    variant(int v): type(ORD)           { val._ord = v; }
#ifdef SH64
    variant(long long v): type(ORD)     { val._ord = v; }
#endif
    variant(real v): type(REAL)         { val._real = v; }
    variant(object* v): type(OBJ)       { val._obj = v; }
    variant(const str& v): type(STR)    { val._obj = v.obj->ref(); }
    variant(const vector<variant>& s): type(VEC)    { val._obj = s.obj->ref(); }
    ~variant()                          { _fin(); }

    void operator= (const variant&);
    int compare(const variant&) const;

    Type getType() const                { return type; }
    bool is_refcnt() const              { return type >= REFCNT; }

    // Fast "unsafe" access methods; checked for correctness in DEBUG mode
    bool       _bool()          const { _dbg(ORD); return val._ord; }
    uchar      _uchar()         const { _dbg(ORD); return val._ord; }
    integer    _int()           const { _dbg(ORD); return val._ord; }
    integer&   _intw()                { _dbg(ORD); return val._ord; }
    integer    _ord()           const { _dbg(ORD); return val._ord; }
    const str& _str()           const { _dbg(STR); return *(str*)&val._obj; }
    const vec_t& _vec()         const { _dbg(VEC); return *(vec_t*)&val._obj; }
    const set_t& _set()         const { _dbg(SET); return *(set_t*)&val._obj; }
    object*    _obj()           const { _dbg(OBJ); return val._obj; }
};

template <>
    struct comparator<variant>
        { int operator() (const variant& a, const variant& b) { return a.compare(b); } };


extern template class vector<variant>;
extern template class set<variant>;
extern template class dict<variant, variant>;
extern template class podvec<variant>;

typedef vector<variant> varlist;


template class vector<variant>;
template class podvec<variant>;
template class set<variant>;
template class dict<variant, variant>;


void variant::_type_err() { throw ecmessage("Variant type mismatch"); }
void variant::_range_err() { throw ecmessage("Variant range error"); }


void variant::operator= (const variant& v)
{
    if (v.is_refcnt())
    {
        if (val._obj != v.val._obj)
            val._obj = v.val._obj->ref();
    }
    else
        val = v.val;
    type = v.type;
}


int variant::compare(const variant& v) const
{
    notimpl();
    return 0;
/*
    switch(type)
    {
    case NONE: return 0;
    case ORD: return int(val._ord - v.val._ord);
    case REAL: return val._real < v.val._real ? -1 : (val._real > v.val._real ? 1 : 0);
    case STR: return _str().compare(v._str());
    // , VEC, SET, ORDSET, DICT, RANGE, OBJ
    }
*/
}



// --- tests --------------------------------------------------------------- //


void ut_fail(unsigned line, const char* e)
{
    fprintf(stderr, "%s:%u: test failed `%s'\n", __FILE__, line, e);
    exit(200);
}

#define fail(e)     ut_fail(__LINE__, e)
#define check(e)    { if (!(e)) fail(#e); }


void test_range()
{
    range r1;
    range r2 = r1;
    range r3(1, 3);
    range r4 = r3;
    r1 = r3;
    check(r2.empty());
}


void test_variant()
{
    variant v1;
    variant v2 = v1;
}


void ttt(const variant& v)
{
    variant v0 = v;
    vector<variant> v1;
    podvec<variant> v2;
    v2.push_back(1);
}


int main()
{
    printf("%lu %lu\n", sizeof(object), sizeof(container));

    test_range();
    test_variant();

    if (object::allocated != 0)
    {
        fprintf(stderr, "object::allocated: %d\n", object::allocated);
        _fatal(0xff01);
    }

    return 0;
}
