
#include "common.h"
// #include "runtime.h"
// #include "parser.h"
// #include "typesys.h"
// #include "vm.h"
// #include "compiler.h"


// --- vector -------------------------------------------------------------- //


template <class T>
class vector: public podvec<T>
{
protected:
    enum { Tsize = sizeof(T) };
    typedef podvec<T> parent;
    typedef T* Tptr;
    typedef Tptr& Tref;

    class cont: public container
    {
    protected:
        // Virtual overrides
        void finalize(void* p, memint len)
        {
            (char*&)p += len - Tsize;
            for ( ; len; len -= Tsize, Tref(p)--)
                Tptr(p)->~T();
        }

        void copy(void* dest, const void* src, memint len)
        {
            for ( ; len; len -= Tsize, Tref(dest)++, Tref(src)++)
                new(dest) T(*Tptr(src));
        }

        cont(memint siz): container(siz, siz)  { }

    public:
        static cont* allocate(memint len)
            { return new(len) cont(len); }

        ~cont()
            { if (_size) { finalize(data(), _size); _size = 0; } }
    };
    
    char* _init(memint len)
    {
        assert(len > 0);
        cont* c = cont::allocate(len);
        c->grab();
        return parent::_data = c->data();
    }

public:
    vector(): parent()      { }

    bool empty() const      { return parent::empty(); }

    // Override stuff that requires allocation of 'vector::cont'
    void push_back(const T& t)
        { new(empty() ? _init(Tsize) : bytevec::_appendnz(Tsize)) T(t); }

    void insert(memint pos, const T& t)
        { new(empty() && !pos ? _init(Tsize) : bytevec::_insertnz(pos * Tsize, Tsize)) T(t); }

    // Give a chance to alternative constructors, e.g. str can be constructed
    // from (const char*). Without these templates below temp objects are
    // created and then copied into the vector. Though these are somewhat
    // dangerous too.
    template <class U>
        void push_back(const U& u)
            { new(empty() ? _init(Tsize) : bytevec::_appendnz(Tsize)) T(u); }

    template <class U>
        void insert(memint pos, const U& u)
            { new(empty() && !pos ? _init(Tsize) : bytevec::_insertnz(pos * Tsize, Tsize)) T(u); }

    template <class U>
        void replace(memint pos, const U& u)
            { parent::atw(pos) = u; }

    bool find_insert(const T& item)
    {
        if (empty())
        {
            push_back(item);
            return true;
        }
        else
            return parent::find_insert(item);
    }
};


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


void test_vector()
{
    vector<str> v1;
    v1.push_back("ABC");
    check(v1[0] == "ABC");
    vector<str> v2 = v1;
    check(v2[0] == "ABC");
    v1.push_back("DEF");
    v1.push_back("GHI");
    v1.push_back("JKL");
    vector<str> v3 = v1;
    check(v1.size() == 4);
    check(v2.size() == 1);
    check(v3.size() == 4);
    check(v1[0] == "ABC");
    check(v1[1] == "DEF");
    check(v1[2] == "GHI");
    check(v1[3] == "JKL");
    v1.erase(2);
    check(v1[0] == "ABC");
    check(v1[1] == "DEF");
    check(v1[2] == "JKL");
    check(v1.back() == "JKL");
    v3 = v1;
    v1.replace(2, "MNO");
    check(v1[2] == "MNO");
    check(v3[2] == "JKL");
}


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

    test_vector();

    int exitcode = 0;
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

#ifdef DEBUG
    if (object::allocated != 0)
    {
        fprintf(stderr, "object::allocated: %d\n", object::allocated);
        _fatal(0xff01);
    }
#endif
    return exitcode;
}

