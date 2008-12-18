
#include <assert.h>
#include <stdio.h>

#include "charset.h"
#include "str.h"
#include "array.h"
#include "baseobj.h"


// ------------------------------------------------------------------------ //
// --- UNIT TESTS --------------------------------------------------------- //
// ------------------------------------------------------------------------ //

// We rely on assert(), so make sure it is turned on
#ifdef NDEBUG
#  undef NDEBUG
#endif


#define assert_throw(e,...) { bool __t = false; try { __VA_ARGS__; } catch(e) { __t = true; } assert(__t); }
#define assert_nothrow(...) { bool __t = false; try { __VA_ARGS__; } catch(...) { __t = true; } assert(!__t); }


void const_string_call(const string& s)
{
    assert('R' == s[2]);
}


void testString()
{
    {
        static char strbuf[10] = "STRING";

        char c = 'A';

        string s1 = "string";
        s1 = s1;
        s1 += s1;
        s1 = s1.copy(6, 6);
        string s2 = s1;
        string s3;
        string s4(strbuf, strlen(strbuf));
        string s5 = 'A';
        string s6 = c;

        assert("string" == s1);
        assert(s1 == s2);
        assert(s3.empty());
        assert("STRING" == s4);
        const_string_call(s4);
        assert('I' == s4[3]);
        assert(6 == s4.size());
        assert(2 == s1.refcount());
        s2.clear();
        assert(1 == s1.refcount());
        s2 = s1;
        s1.unique();
        assert(1 == s1.refcount());
        s1.resize(64);
        string s7 = s1;
        s1.resize(3);
        assert("str" == s1);
        assert("strING" == (s1 += s4.copy(3, 3)));

        s2 = "str" + s1;
        s2 = s1 + "str";
        s2 += "str";
        assert("strINGstrstr" == s2);

        s2 = 's' + s1;
        s2 = s1 + 's';
        s2 += 's';
        assert("strINGss" == s2);
        
        s2 = c + s1;
        s2 = s1 + c;
        s2 += c;
        assert("strINGAA" == s2);

        s2.del(7, 10);
        assert("strINGA" == s2);
        s2.del(3, 2);
        assert("strGA" == s2);
        s2.del(-1, 10);
        assert("strGA" == s2);
        s2.del(0, 2);
        assert("rGA" == s2);

        *(s2.ins(2, 1)) = 'H';
        assert("rGHA" == s2);
        s2.ins(3, "IJ");
        assert("rGHIJA" == s2);
        s2.ins(6, string("str"));
        assert("rGHIJAstr" == s2);
        s2.ins(0, string("str"));
        assert("strrGHIJAstr" == s2);
        s2.ins(-1, string("str"));
        assert("strrGHIJAstr" == s2);
        s2.ins(13, string("str"));
        assert("strrGHIJAstr" == s2);

        s2 = 'a';
        assert("a" == s2);
        s2 = c;
        assert("A" == s2);

        s1 = "abc";
        s1 += s1;
        assert("abcabc" == s1);
        s1 += s1.c_str();
        assert("abcabcabcabc" == s1);

        string s8;
        s8.resize(32 - strrecsize);
        assert(32 - strrecsize == s8.capacity());
        s8.c_str();
        assert(64 - strrecsize == s8.capacity());
    }
    {
        assert("123456789" == itostring(123456789));
        assert("123" == itostring(char(123)));
        assert("-000123" == itostring(-123, 10, 7, '0'));
        assert("0ABCDE" == itostring(0xabcde, 16, 6));
        assert("-9223372036854775808" == itostring(LARGE_MIN));
        assert("18446744073709551615" == itostring(ULARGE_MAX));

        bool e, o;
        assert(1234 == stringtou("1234", &e, &o));
        assert(0x15AF == stringtou("15AF", &e, &o, 16));
        assert(LARGE_MAX == stringtou("5zzzzzzzzzz", &e, &o, 64));
        assert(!e && !o);

        // out of range by 1
        assert(0 == stringtou("18446744073709551616", &e, &o, 10));
        assert(!e && o);
        
        assert(0 == stringtou("", &e, &o));
        assert(e && !o);
        assert(0 == stringtou("a", &e, &o));
        assert(e && !o);
        assert(0 == stringtou("7a", &e, &o));
        assert(e && !o);
    }
}


void testCharset()
{
    charset c = "a-fz~c0-~cf~ff";
    assert(c['a']);
    assert(c['b']);
    assert(c['\xc0']);
    assert(c['\xc5']);
    assert(c['\xcf']);
    assert(c['\xff']);
    assert(!(c['\xfe']));
    assert(!(c['g']));
    assert(!(c[0]));
}


void testArrays()
{
    {
        Array<int> a;
        a.add(1);
        a.add(2);
        a.add(3);
        Array<int> b(a);
        a.del(2);
        a.clear();
    }
    {
        fifoimpl f;
        string s = "abcd";
        f.push(s.c_bytes(), s.size());
        s = "efg";
        f.push(s.c_bytes(), s.size());

        fifoimpl g = f;
        char buf[256];
        int len = g.pull(buf, 6);
        buf[len] = 0;
        assert(strcmp("abcdef", buf) == 0);
    }
    {
        PodFifo<int> f;
        f.push(1);
        f.push(2);
        f.push(3);
        assert(3 == f.size());
        assert(1 == f.pull());
        assert(2 == f.pull());
        PodFifo<int> g = f;
        assert(3 == g.pull());
    }
}


void testContainer()
{
    int saveObjCount = Base::objCount;
    {
        Container<Base> c;
        c.add(new Base());
        c.add(new Base());
        c.add(new Base());
        assert(Base::objCount == saveObjCount + 3);
        c.pop();
        assert(Base::objCount == saveObjCount + 2);
        c.clear();
        assert(Base::objCount == saveObjCount);
        c.add(new Base());
        c.add(new Base());
    }
    // check if the destructor calls clear()
    assert(Base::objCount == saveObjCount);
    {
        saveObjCount = Base::objCount;
        PodArray<Base*> c;
        c.add(new Base());
        c.add(new Base());
        c.add(new Base());
        assert(Base::objCount == saveObjCount + 3);
        c.pop();
        assert(Base::objCount == saveObjCount + 3);
        c.clear();
        assert(Base::objCount == saveObjCount + 3);
        Base::objCount = saveObjCount;
    }
}


void testHashTable()
{
    int saveObjCount = Base::objCount;
    {
        HashTable<Base, true> t;
        t.add("a", new Base());
        assert_throw(EDuplicate, t.add("a", NULL));
        t.add("b", new Base());
        assert(t.find("a") != NULL);
        assert(t.find("z") == NULL);
        assert_nothrow(t.get("a"));
        assert_throw(ENotFound, t.get("z"));
        assert_throw(EInternal, t.remove("z"));
        assert(Base::objCount == saveObjCount + 2);
        assert_nothrow(t.remove("a"));
        assert(Base::objCount == saveObjCount + 1);
        t.clear();
        assert(Base::objCount == saveObjCount);
        t.add("c", new Base());
    }
    // check if the destructor calls clear()
    assert(Base::objCount == saveObjCount);
    {
        HashTable<Base, false> t;
        t.add("a", new Base());
        t.add("b", new Base());
        assert(Base::objCount == saveObjCount + 2);
        assert_nothrow(t.remove("a"));
        assert(Base::objCount == saveObjCount + 2);
        t.clear();
        assert(Base::objCount == saveObjCount + 2);
        Base::objCount = saveObjCount;
    }
}


class _AtExit
{
public:
    ~_AtExit()
    {
        if (Base::objCount != 0)
            fprintf(stderr, "Internal: objCount = %d\n", Base::objCount);
        if (stralloc != 0)
            fprintf(stderr, "Internal: stralloc = %d\n", stralloc);
        if (fifoChunkAlloc != 0)
            fprintf(stderr, "Internal: fifoChunkAlloc = %d\n", fifoChunkAlloc);
    }
} _atexit;


int main ()
{
    testString();
    testCharset();
    testArrays();
    testContainer();
    testHashTable();
    return 0;
}
