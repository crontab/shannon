
#define __STDC_LIMIT_MACROS

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>

#include <iostream>
#include <sstream>

#include "common.h"
#include "variant.h"
#include "symbols.h"
#include "source.h"


using namespace std;


#define fail(e) \
    (printf("%s:%u: test failed `%s'\n", __FILE__, __LINE__, e), exit(200))

#define check(e) \
    { if (!(e)) fail(#e); }

#define check_throw(e,...) \
    { bool chk_throw = false; try { __VA_ARGS__; } catch(e&) { chk_throw = true; } check(chk_throw); }

#define check_nothrow(...) \
    { try { __VA_ARGS__; } catch(...) { fail("exception thrown"); } }

#define XSTR(s) _STR(s)
#define _STR(s) #s

#ifdef SH64
#  define INTEGER_MAX_STR "9223372036854775807"
#  define INTEGER_MAX_STR_PLUS "9223372036854775808"
#  define INTEGER_MIN_STR "-9223372036854775808"
#else
#  define INTEGER_MAX_STR "2147483647"
#  define INTEGER_MAX_STR_PLUS "2147483648"
#  define INTEGER_MIN_STR "-2147483648"
#endif

void test_common()
{
    // make sure the assembly code for atomic ops is sane
    int i = 1;
    check(pincrement(&i) == 2);
    check(pdecrement(&i) == 1);

    // string conversion
    check(to_string(0) == "0");
    check(to_string(-1) == "-1");
    check(to_string(INTEGER_MAX) == INTEGER_MAX_STR);
    check(to_string(INTEGER_MIN) == INTEGER_MIN_STR);
    bool e = true, o = true;
    check(from_string("0", &e, &o) == 0);
    check(!o); check(!e);
    check(from_string(INTEGER_MAX_STR, &e, &o) == INTEGER_MAX);
    check(from_string(INTEGER_MAX_STR_PLUS, &e, &o) == uinteger(INTEGER_MAX) + 1);
    check(from_string(INTEGER_MAX_STR "0", &e, &o) == 0 ); check(o);
    check(from_string("-1", &e, &o) == 0 ); check(e);
    check(from_string("89abcdef", &e, &o, 16) == 0x89abcdef);
    check(from_string("afg", &e, &o, 16) == 0); check(e);
    
    // syserror
    check(str(esyserr(2, "arg").what()) == "No such file or directory (arg)");
    check(str(esyserr(2).what()) == "No such file or directory");
}


class test_obj: public object
{
protected:
    virtual object* clone() const { fail("Can't clone test object"); }
public:
    void dump(std::ostream& s) const
        { s << "test_obj"; }
    test_obj()  { }
};


void test_variant()
{
    int save_alloc = object::alloc;
    {
        variant v1 = null;              check(v1.is_null());
        variant v2 = 0;                 check(v2.is_int());     check(v2.as_int() == 0);
        variant v3 = 1;                 check(v3.is_int());     check(v3.as_int() == 1);
        variant v4 = INTEGER_MAX;       check(v4.is_int());     check(v4.as_int() == INTEGER_MAX);
        variant v5 = INTEGER_MIN;       check(v5.is_int());     check(v5.as_int() == INTEGER_MIN);
        variant v6 = 1.1;               check(v6.is_real());    check(v6.as_real() == real(1.1));
        variant v7 = false;             check(v7.is_bool());    check(!v7.as_bool());
        variant v8 = true;              check(v8.is_bool());    check(v8.as_bool());
        variant v12 = 'x';              check(v12.is_char());   check(v12.as_char() == 'x');
        variant v9 = "";                check(v9.is_str());     check(v9.as_str().empty());
        variant v10 = "abc";            check(v10.is_str());    check(v10.as_str() == "abc");
        string s1 = "def";
        variant vst = s1;               check(vst.is_str());    check(vst.as_str() == s1);
        variant vr = new_range();       check(vr.is_range());   check(vr.is_null_ptr());
        variant vr1 = new_range(1, 5);  check(vr1.is_range());  check(!vr1.is_null_ptr());
        variant vr2(6, 7);              check(vr2.is_range());  check(!vr2.is_null_ptr());
        variant vr3(6, 5);              check(vr3.is_range());  check(vr3.is_null_ptr());
        variant vt = new_tuple();       check(vt.is_tuple());   check(vt.is_null_ptr());
        variant vd = new_dict();        check(vd.is_dict());    check(vd.is_null_ptr());
        variant vs = new_set();         check(vs.is_set());     check(vs.is_null_ptr());
        variant vts = new_tinyset();    check(vts.is_tinyset());  check(vts.as_tinyset() == 0);
        object* o = new test_obj();
        variant vo = o;                 check(vo.is_object());  check(vo.as_object() == o);

        check_throw(evarianttype, v1.as_int());
        check_throw(evarianttype, v1.as_real());
        check_throw(evarianttype, v1.as_bool());
        check_throw(evarianttype, v1.as_char());
        check_throw(evarianttype, v1.as_str());
        check_throw(evarianttype, v1.as_range());
        check_throw(evarianttype, v1.as_tuple());
        check_throw(evarianttype, v1.as_dict());
        check_throw(evarianttype, v1.as_set());
        check_throw(evarianttype, v1.as_tinyset());
        check_throw(evarianttype, v1.as_object());
        
        check(v1.to_string() == "null");
        check(v2.to_string() == "0");
        check(v3.to_string() == "1");
        check(v4.to_string() == INTEGER_MAX_STR);
        check(v5.to_string() == INTEGER_MIN_STR);
        check(v6.to_string() == "1.1");
        check(v7.to_string() == "false");
        check(v8.to_string() == "true");
        check(v9.to_string() == "\"\"");
        check(v10.to_string() == "\"abc\"");
        check(vst.to_string() == "\"def\"");
        check(v12.to_string() == "'x'");
        check(vr.to_string() == "[]");
        check(vr1.to_string() == "[1..5]");
        check(vr2.to_string() == "[6..7]");
        check(vr3.to_string() == "[]");
        check(vt.to_string() == "[]");
        check(vd.to_string() == "[]");
        check(vs.to_string() == "[]");
        check(vts.to_string() == "[]");
        check(vo.to_string() == "[test_obj]");

        check(v1 < v2); check(!(v2 < v1));
        check(v2 < v3);
        check(v3 != v6); check(v3 < v6); check(!(v6 < v3));
        
        variant v;
        check(v == null);
        v = 0;                 check(v.as_int() == 0);              check(v == 0);
        check(v != null);      check(v != true);                    check(v != "abc");
        v = 1;                 check(v.as_int() == 1);              check(v == 1);
        v = INTEGER_MAX;       check(v.as_int() == INTEGER_MAX);    check(v == INTEGER_MAX);
        v = INTEGER_MIN;       check(v.as_int() == INTEGER_MIN);    check(v == INTEGER_MIN);
        v = 1.1;               check(v.as_real() == real(1.1));     check(v == 1.1);
        v = false;             check(!v.as_bool());                 check(v == false);
        v = true;              check(v.as_bool());                  check(v == true);
        v = 'x';               check(v.as_char() == 'x');           check(v == 'x');    check(v != 'z');
        v = "";                check(v.as_str().empty());           check(v == "");
        v = "abc";             check(v.as_str() == "abc");          check(v == "abc");
        v = s1;                check(v.as_str() == s1);             check(v == s1);
        v = new_range();       check(v.is_null_ptr());              check(v.left() == 0 && v.right() == -1);
        v = new_tuple();       check(v.is_null_ptr());
        v = new_dict();        check(v.is_null_ptr());
        v = new_set();         check(v.is_null_ptr());
        v = o;                 check(v.as_object() == o);
        check(v != null);
        check(!v.is_null());
        check(!v.is_int());
        check(!v.is_real());
        check(!v.is_bool());
        check(!v.is_int());
        check(!v.is_char());
        check(!v.is_str());
        check(!v.is_range());
        check(!v.is_tuple());
        check(!v.is_dict());
        check(!v.is_set());
        check(!v.is_tinyset());
        v = null;
        check(!v.is_object());
        check(v == null);

        // tinyset
        check(vts.empty()); check_throw(evarianttype, vts.size());
        vts.tie(5);
        vts.tie(26);
        vts.tie(31);
#ifdef SH64
        vts.tie(63);
        vts.has(63);
        vts.untie(63);
#else
        check_throw(evariantrange, vts.tie(63));
#endif
        check_throw(evariantrange, vts.tie(100));
        check_throw(evariantrange, vts.tie(-1));
        check_throw(evariantrange, vts.untie(100));
        check_throw(evariantrange, vts.untie(-1));
        check_throw(evarianttype, vts.tie("abc"));
        check(vts.to_string() == "[5, 26, 31]");
        check(!vts.empty());
        vts.untie(26);
        vts.untie(30);
        check(vts.to_string() == "[5, 31]");
        check(vts.has(5));
        check(vts.has(31));
        check(!vts.has(26));

        // str
        vst = "";
        check(vst.empty()); check(vst.size() == 0);
        vst.append("abc");
        vst.append("");
        vst.append(s1);
        vst.append("ghi");
        vst.append(variant("jkl"));
        check(vst.as_str() == "abcdefghijkl");
        check(vst.substr(5, 3) == "fgh");
        check(vst.substr(5) == "fghijkl");
        vst.erase(1);
        check(vst.as_str() == "acdefghijkl");
        vst.erase(3, 4);
        check(vst.as_str() == "acdijkl");
        check(vst.getch(2) == 'd');
        vst.resize(10, '-');
        check(vst.as_str() == "acdijkl---");
        check(vst.size() == 10);
        vst.resize(5, '-');
        check(vst.as_str() == "acdij");

        // range
        vr = new_range();
        check(vr.empty());
        vr = new_range(-1, 1);
        check(vr.left() == -1 && vr.right() == 1);
        check(vr.has(0));
        check(!vr.has(-2));
        vr = new_range(5, 2);
        check(vr.left() == 0 && vr.right() == -1);
        check(!vr.has(-2));
        variant vr4 = 0;
        vr4.assign(0, 5);
        check(vr4.left() == 0 && vr4.right() == 5);
        check_throw(evarianttype, vr4.has("abc"));

        // tuple
        check(vt.empty()); check(vt.size() == 0);
        check_throw(evariantindex, vt.insert(1, 0));
        vt.push_back(0);
        vt.push_back("abc");
        vt.insert(vt.size(), 'd');
        vt.push_back(vs);
        vt.insert(1, true);
        check(vt.to_string() == "[0, true, \"abc\", 'd', []]");
        check(!vt.empty()); check(vt.size() == 5);
        check_throw(evariantindex, vt.insert(10, 0));
        check_throw(evariantindex, vt.insert(mem(-1), 0));
        vt.erase(2);
        check(vt.to_string() == "[0, true, 'd', []]");
        check(vt[2] == 'd');
        vt.put(2, "asd");
        check(vt.to_string() == "[0, true, \"asd\", []]");
        check_throw(evariantindex, vt.put(4, 0));
        vt.resize(2);
        check(vt.to_string() == "[0, true]");
        vt.resize(4);
        check(vt.to_string() == "[0, true, null, null]");
        check(vt.size() == 4);

        // dict
        check(vd.empty()); check(vd.size() == 0);
        vd.tie("k1", "abc");
        vd.tie("k2", true);
        vd.tie("k3", new_set());
        check(vd.to_string() == "[\"k1\": \"abc\", \"k2\": true, \"k3\": []]");
        check(!vd.empty()); check(vd.size() == 3);
        vd.tie("k2", null);
        check(vd.to_string() == "[\"k1\": \"abc\", \"k3\": []]");
        vd.tie("kz", null);
        check(vd.to_string() == "[\"k1\": \"abc\", \"k3\": []]");
        stringstream vds;
        vforeach(dict, i, vd)
            vds << ' ' << i->first << ": " << i->second;
        check(vds.str() == " \"k1\": \"abc\" \"k3\": []");
        vd.untie("k2");
        check(vd.to_string() == "[\"k1\": \"abc\", \"k3\": []]");
        vd.untie("k3");
        check(vd.to_string() == "[\"k1\": \"abc\"]");
        check(vd["k1"] == "abc");
        check(vd["k2"] == null);
        check(vd["kz"] == null);
        check(vd.has("k1"));
        check(!vd.has("k2"));

        vd = new_dict();
        vd.tie(new_dict(), 6);
        vd.tie("abc", 5);
        vd.tie(1.1, 4);
        vd.tie(10, 3);
        vd.tie('a', 2);
        vd.tie(false, 1);
        vd.tie(null, 0);
        check(vd.to_string() == "[null: 0, false: 1, 'a': 2, 10: 3, 1.1: 4, \"abc\": 5, []: 6]");
        
        // dict[range]
        vd = new_dict();
        vd.tie(new_range(0, 4), new_range(0, 1));
        vd.tie(new_range(5, 6), "abc");
        check(vd.has(new_range(5, 6)));
        check(!vd.has(new_range()));
        check(!vd.has(new_range(0, 6)));
        
        // set
        check(vs.empty()); check_throw(evarianttype, vs.size());
        vs.tie(5);
        vs.tie(26);
        vs.tie(127);
        check(vs.to_string() == "[5, 26, 127]");
        stringstream vdss;
        vforeach(set, i, vs)
            vdss << ' ' << *i;
        check(vdss.str() == " 5 26 127");
        check(!vs.empty());
        vs.untie(26);
        vs.untie(226);
        check(vs.to_string() == "[5, 127]");
        check(vs.has(5));
        check(vs.has(127));
        check(!vs.has(26));
        
        vs.tie("abc");
        vs.tie("abc");
        vs.tie("def");
        check(vs.to_string() == "[5, 127, \"abc\", \"def\"]")
        check(vs.has("abc"));
        vs.tie(1.1);
        vs.tie(2.2);
        vs.tie(true);
        vs.tie(null);
        check(vs.to_string() == "[null, true, 5, 127, 1.1, 2.2, \"abc\", \"def\"]")
        check(vs.has(1.1));

        stringstream ss;
        ss << vs;
        check(ss.str() == "[null, true, 5, 127, 1.1, 2.2, \"abc\", \"def\"]");
        
        // ref counting
        vs = new_set();
        vs.tie(0);
        check(vs.as_set().is_unique());
        variant vss = vs;
        check(vss.has(0));
        check(!vs.as_set().is_unique());
        check(!vss.as_set().is_unique());
        check(vs.to_string() == "[0]");
        check(vss.to_string() == "[0]");
        vs.tie(1);
        check(vs.as_set().is_unique());
        check(vss.as_set().is_unique());
        check(vs.to_string() == "[0, 1]");
        check(vss.to_string() == "[0]");
        
        // object
        // vo = vo; // this doesn't work at the moment
        // cout << vo.as_object() << endl;
    
        int save_alloc2 = object::alloc;
        objptr<test_obj> p1 = new test_obj();
        check(object::alloc == save_alloc2 + 1);
        check(p1->is_unique());
        objptr<test_obj> p2 = p1;
        check(!p1->is_unique());
        check(!p2->is_unique());
        check(p1 == p2);
        objptr<test_obj> p3 = new test_obj();
        check(p1 != p3);
        p3 = p3; // see if it doesn't core dump
    }
    check(object::alloc == save_alloc);
}


void test_symbols()
{
    int save_alloc = object::alloc;
    {
        objptr<Symbol> s = new Symbol("sym");
        check(s->name == "sym");
        SymbolTable<Symbol> t;
        check(t.empty());
        objptr<Symbol> s1 = new Symbol("abc");
        check(s1->is_unique());
        t.addUnique(s1);
        check(s1->is_unique());
        objptr<Symbol> s2 = new Symbol("def");
        t.addUnique(s2);
        objptr<Symbol> s3 = new Symbol("abc");
        check_throw(EDuplicate, t.addUnique(s3));
        check(t.find("abc") == s1);
        check(s2 == t.find("def"));
        check(t.find("xyz") == NULL);
        check(!t.empty());

        List<Symbol> l;
        check(l.size() == 0);
        check(l.empty());
        l.push_back(s1);
        check(!s1->is_unique());
        l.push_back(new Symbol("ghi"));
        check(l.size() == 2);
        check(!l.empty());
    }
    check(object::alloc == save_alloc);
}


class InMem: public InText
{
protected:
    string sbuffer;
    virtual void validateBuffer()   { eof = true; }
public:
    InMem(const str& buf): sbuffer(buf)
        { buffer = (char*)buf.data(); bufsize = buf.size(); linenum = 1; }
    virtual str getFileName()       { return "<memory>"; }
};


void test_source()
{
    {
        InFile file("nonexistent");
        check_throw(esyserr, file.get());
    }
    {
        InMem m("one\t two\n567");
        check(m.preview() == 'o');
        check(m.get() == 'o');
        check(m.token(identRest) == "ne");
        m.skip(wsChars);
        check(m.token(identRest) == "two");
        check(m.getEol());
        check(!m.getEof());
        check(m.getColumn() == 12);
        m.skipEol();
        check(m.getLineNum() == 2);
        check(m.token(digits) == "567");
        check(m.getEol());
        check(m.getEof());
    }
    {
#ifdef XCODE
        const char* fn = "../../src/tests/parser.txt";
#else
        const char* fn = "tests/parser.txt";
#endif
        Parser p(new InFile(fn));
        static Token expect[] = {
            tokIdent, tokComma, tokSep,
            tokIndent, tokModule, tokSep,
            tokIndent, tokIdent, tokIdent, tokIdent, tokSep,
            tokIdent, tokIdent, tokConst, tokPeriod, tokSep,
            tokBlockEnd,
            tokIntValue, tokSep,
            tokIndent, tokStrValue, tokSep,
            tokIdent, tokComma, tokSep,
            tokIdent, tokSep,
            tokBlockEnd, tokBlockEnd,
            tokIdent, tokBlockBegin, tokIdent, tokBlockEnd,
            tokIdent, tokBlockBegin, tokIdent, tokSep, 
            tokIdent, tokSep, tokBlockEnd,
            tokBlockEnd,
            tokEof
        };
        int i = 0;
        while (p.next() != tokEof && expect[i] != tokEof)
        {
            check(expect[i] == p.token);
            switch (i)
            {
            case 0: check(p.strValue == "Thunder") break;
            case 1: check(p.strValue == ","); break;
            case 17: check(p.intValue == 42); break;
            case 20: check(p.strValue == "Thunder, 'thunder',\tthunder, Thundercats"); break;
            }
            i++;
        }
        check(expect[i] == tokEof && p.token == tokEof);
    }
    {
        Parser p(new InMem(INTEGER_MAX_STR"\n  "INTEGER_MAX_STR_PLUS"\n  null\n aaa"
            " 'asd\n'[\\t\\r\\n\\x41\\\\]' '\\xz'"));
        check(p.next() == tokIntValue);
        check(p.intValue == INTEGER_MAX);
        check(p.next() == tokSep);
        check(p.next() == tokIndent);
        check_throw(EParser, p.next()); // integer overflow
        check(p.next() == tokSep);
        check(p.next() == tokNull);
        check(p.next() == tokSep);
        check_throw(EParser, p.next()); // unmatched unindent
        check(p.next() == tokIndent);
        check(p.next() == tokIdent);
        check_throw(EParser, p.next()); // unexpected end of line
        check(p.next() == tokStrValue);
        check(p.strValue == "[\t\r\nA\\]");
        check_throw(EParser, p.next()); // bad hex sequence
    }
}


int main()
{
    cout << "short: " << sizeof(short) << "  long: " << sizeof(long) << "  long long: "
        << sizeof(long long) << "  int: " << sizeof(int) << "  void*: " << sizeof(void*)
        << "  float: " << sizeof(float) << "  double: " << sizeof(double) << endl;
    cout << "integer: " << sizeof(integer) << "  mem: " << sizeof(mem)
        << "  real: " << sizeof(real) << "  variant: " << sizeof(variant) << endl;

    check(sizeof(int) == 4);
    check(sizeof(mem) >= 4);

#ifdef SH64
    check(sizeof(integer) == 8);
#  ifdef PTR64
    check(sizeof(variant) == 16);
#  else
    check(sizeof(variant) == 12);
#  endif
#else
    check(sizeof(integer) == 4);
    check(sizeof(variant) == 8);
#endif

    try
    {
        test_common();
        test_variant();
        test_symbols();
        test_source();
    }
    catch (exception& e)
    {
        fail(e.what());
    }

    check(object::alloc == 0);

    return 0;
}

