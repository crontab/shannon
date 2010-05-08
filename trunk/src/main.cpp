
#include "common.h"
#include "runtime.h"
// #include "parser.h"
// #include "typesys.h"
// #include "vm.h"
// #include "compiler.h"


// --- variant ------------------------------------------------------------- //

class variant;

typedef vector<variant> varvec;
typedef varvec varset;
typedef dict<variant, variant> vardict;

/*
class variant
{
    friend void test_variant();

public:
    // TODO: tinyset

    enum Type
        { NONE, ORD, REAL, STR, VEC, ORDSET, DICT, RTOBJ,
            NONPOD = STR};

    struct _None { int dummy; }; 
    static _None null;

protected:
    Type type;
    union
    {
        integer     _ord;       // int, char and bool
        real        _real;      // not implemented in the VM yet
        void*       _data;      // str, vector
        object*     _obj;       // ordset, dict
        rtobject*   _rtobj;     // runtime objects with the "type" field
    } val;

    static void _type_err();
    static void _range_err();
    bool is_nonpod() const              { return type >= NONPOD; }
    void _req(Type t) const             { if (type != t) _type_err(); }
#ifdef DEBUG
    void _dbg(Type t) const             { _req(t); }
#else
    void _dbg(Type t) const             { }
#endif

    void _init()                        { type = NONE; }
    void _init(Type);
    void _init(_None)                   { type = NONE; }
    void _init(bool v)                  { type = ORD; val._ord = v; }
    void _init(char v)                  { type = ORD; val._ord = uchar(v); }
    void _init(uchar v)                 { type = ORD; val._ord = v; }
    void _init(int v)                   { type = ORD; val._ord = v; }
#ifdef SHN_64
    void _init(large v)                 { type = ORD; val._ord = v; }
#endif
    void _init(real v)                  { type = REAL; val._real = v; }
    void _init(const str& v)            { _init(STR, v.data()); }
    void _init(const char* s)           { type = STR; ::new(&val._obj) str(s); }
    void _init(const varvec& v)         { _init(VEC, v.obj); }
    void _init(const ordset& v)         { _init(ORDSET, v.obj); }
    void _init(const vardict& v)        { _init(DICT, v.obj); }
    void _init(Type t, object* o)       { type = t; val._obj = o->grab(); }
    void _init(rtobject* o)             { type = RTOBJ; val._rtobj = o->grab<rtobject>(); }
    void _init(const variant& v);
    void _fin();

public:
    variant()                           { _init(); }
    variant(Type t)                     { _init(t); }
    variant(const variant& v)           { _init(v); }
    template <class T>
        variant(const T& v)             { _init(v); }
    variant(Type t, object* o)          { _init(t, o); }
    ~variant()                          { _fin(); }

    template <class T>
        void operator= (const T& v)     { _fin(); _init(v); }
    void operator= (const variant& v);  // { assert(this != &v); _fin(); _init(v); }
    void clear()                        { _fin(); _init(); }
    bool empty() const;

    memint compare(const variant&) const;
    bool operator== (const variant&) const;
    bool operator!= (const variant& v) const { return !(operator==(v)); }

    Type getType() const                { return Type(type); }
    bool is(Type t) const               { return type == t; }
    bool is_none() const                { return type == NONE; }
    bool is_ord() const                 { return type == ORD; }
    bool is_str() const                 { return type == STR; }

    // Fast "unsafe" access methods; checked for correctness in DEBUG mode
    bool        _bool()           const { _dbg(ORD); return val._ord; }
    uchar       _uchar()          const { _dbg(ORD); return val._ord; }
    integer     _int()            const { _dbg(ORD); return val._ord; }
    integer     _ord()            const { _dbg(ORD); return val._ord; }
    const str&  _str()            const { _dbg(STR); return *(str*)&val._obj; }
    const varvec& _vec()          const { _dbg(VEC); return *(varvec*)&val._obj; }
    const varset& _set()          const { return _vec(); }
    const ordset& _ordset()       const { _dbg(ORDSET); return *(ordset*)&val._obj; }
    const vardict& _dict()        const { _dbg(DICT); return *(vardict*)&val._obj; }
    rtobject*   _rtobj()          const { _dbg(RTOBJ); return val._rtobj; }
    object*     _anyobj()         const { _dbg_anyobj(); return val._obj; }
    integer&    _ord()                  { _dbg(ORD); return val._ord; }
    str&        _str()                  { _dbg(STR); return *(str*)&val._obj; }
    varvec&     _vec()                  { _dbg(VEC); return *(varvec*)&val._obj; }
    varset&     _set()                  { return _vec(); }
    ordset&     _ordset()               { _dbg(ORDSET); return *(ordset*)&val._obj; }
    vardict&    _dict()                 { _dbg(DICT); return *(vardict*)&val._obj; }

    // Safer access methods; may throw an exception
    bool        as_bool()         const { _req(ORD); return _bool(); }
    char        as_char()         const { _req(ORD); return _uchar(); }
    uchar       as_uchar()        const { _req(ORD); return _uchar(); }
    integer     as_int()          const { _req(ORD); return _int(); }
    integer     as_ord()          const { _req(ORD); return _ord(); }
    const str&  as_str()          const { _req(STR); return _str(); }
    const varvec& as_vec()        const { _req(VEC); return _vec(); }
    const varset& as_set()        const { return as_vec(); }
    const ordset& as_ordset()     const { _req(ORDSET); return _ordset(); }
    const vardict& as_dict()      const { _req(DICT); return _dict(); }
    rtobject*   as_rtobj()        const { _req(RTOBJ); return _rtobj(); }
    object*     as_anyobj()       const { _req_anyobj(); return val._obj; }
    integer&    as_ord()                { _req(ORD); return _ord(); }
    str&        as_str()                { _req(STR); return _str(); }
    varvec&     as_vec()                { _req(VEC); return _vec(); }
    varset&     as_set()                { return as_vec(); }
    ordset&     as_ordset()             { _req(ORDSET); return _ordset(); }
    vardict&    as_dict()               { _req(DICT); return _dict(); }
};

template <>
    struct comparator<variant>
        { memint operator() (const variant& a, const variant& b) { return a.compare(b); } };
*/

// ------------------------------------------------------------------------- //

// --- tests --------------------------------------------------------------- //


// #include "typesys.h"


void ut_fail(unsigned line, const char* e)
{
    fprintf(stderr, "%s:%u: test failed `%s'\n", __FILE__, line, e);
    exit(200);
}

#define fail(e)     ut_fail(__LINE__, e)
#define check(e)    { if (!(e)) fail(#e); }

#define check_throw(a) \
    { bool chk_throw = false; try { a; } catch(exception&) { chk_throw = true; } check(chk_throw); }


#ifdef XCODE
    const char* filePath = "../../src/tests/test.shn";
#else
    const char* filePath = "tests/test.shn";
#endif


int main()
{
//    sio << "Shannon v" << SHANNON_VERSION_MAJOR
//        << '.' << SHANNON_VERSION_MINOR
//        << '.' << SHANNON_VERSION_FIX
//        << " Copyright (c) 2008-2010 Hovik Melikyan" << endl << endl;

    int exitcode = 0;

    initRuntime();
/*
    initTypeSys();
    try
    {
        Context context;
        variant result = context.execute(filePath);

        if (result.is_none())
            exitcode = 0;
        else if (result.is_ord())
            exitcode = int(result._ord());
        else if (result.is_str())
        {
            serr << result._str() << endl;
            exitcode = 102;
        }
        else
            exitcode = 103;
    }
    catch (exception& e)
    {
        serr << "Error: " << e.what() << endl;
        exitcode = 101;
    }
    doneTypeSys();
*/
    doneRuntime();

#ifdef DEBUG
    if (object::allocated != 0)
    {
        fprintf(stderr, "object::allocated: %d\n", object::allocated);
        _fatal(0xff01);
    }
#endif
    return exitcode;
}

