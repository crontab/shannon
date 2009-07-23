
#define __STDC_LIMIT_MACROS

#include "common.h"
#include "runtime.h"
#include "source.h"
#include "typesys.h"
#include "vm.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>


#define fail(e) \
    (fprintf(stderr, "%s:%u: test failed `%s'\n", __FILE__, __LINE__, e), exit(200))

#define check(e) \
    { if (!(e)) fail(#e); }

#define check_throw(a) \
    { bool chk_throw = false; try { a; } catch(exception&) { chk_throw = true; } check(chk_throw); }


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
    check(to_string(integer(0)) == "0");
    check(to_string(integer(-1)) == "-1");
    check(to_string(INTEGER_MAX) == INTEGER_MAX_STR);
    check(to_string(INTEGER_MIN) == INTEGER_MIN_STR);
    check(to_string(1, 10, 4, '0') == "0001");
    check(to_string(integer(123456789)) == "123456789");
    check(to_string(-123, 10, 7, '0') == "-000123");
    check(to_string(0xabcde, 16, 6) == "0ABCDE");
    
    bool e = true, o = true;
    check(from_string("0", &e, &o) == 0);
    check(!o); check(!e);
    check(from_string(INTEGER_MAX_STR, &e, &o) == INTEGER_MAX);
    check(from_string(INTEGER_MAX_STR_PLUS, &e, &o) == uinteger(INTEGER_MAX) + 1);
    check(from_string("92233720368547758070", &e, &o) == 0); check(o);
    check(from_string("-1", &e, &o) == 0 ); check(e);
    check(from_string("89abcdef", &e, &o, 16) == 0x89abcdef);
    check(from_string("afg", &e, &o, 16) == 0); check(e);
    
    // syserror
    check(str(esyserr(2, "arg").what()) == "No such file or directory (arg)");
    check(str(esyserr(2).what()) == "No such file or directory");
    
    check(remove_filename_path("/usr/bin/true") == "true");
    check(remove_filename_path("usr/bin/true") == "true");
    check(remove_filename_path("/true") == "true");
    check(remove_filename_path("true") == "true");
    check(remove_filename_path("c:\\Windows\\false") == "false");
    check(remove_filename_path("\\Windows\\false") == "false");
    check(remove_filename_path("Windows\\false") == "false");
    check(remove_filename_path("\\false") == "false");
    check(remove_filename_path("false") == "false");

    check(remove_filename_ext("/usr/bin/true.exe") == "/usr/bin/true");
    check(remove_filename_ext("true.exe") == "true");
    check(remove_filename_ext("true") == "true");
}


class test_obj: public object
{
public:
    void dump(fifo_intf& s) const
        { s << "test_obj"; }
    test_obj(): object(NULL)  { }
    bool empty()  { return false; }
};


void test_variant()
{
    int save_alloc = object::alloc;
    {
        variant v1 = null;              check(v1.is_null());
        variant v2 = 0;                 check(v2.is(variant::INT));     check(v2.as_int() == 0);
        variant v3 = 1;                 check(v3.as_int() == 1);
        variant v4 = INTEGER_MAX;       check(v4.as_int() == INTEGER_MAX);
        variant v5 = INTEGER_MIN;       check(v5.as_int() == INTEGER_MIN);
        variant v6 = 1.1;               check(v6.as_real() == real(1.1));
        variant v7 = false;             check(!v7.as_bool());
        variant v8 = true;              check(v8.as_bool());
        variant v12 = 'x';              check(v12.as_char() == 'x');
        variant v9 = "";                check(v9.as_str().empty());
        variant v10 = "abc";            check(v10.as_str() == "abc");
        str s1 = "def";
        variant vst = s1;               check(vst.as_str() == s1);
        object* o = new test_obj();
        variant vo = o;                 check(vo.is_object());  check(vo.as_object() == o);

        check_throw(v1.as_int());
        check_throw(v1.as_real());
        check_throw(v1.as_bool());
        check_throw(v1.as_char());
        check_throw(v1.as_str());
        check_throw(v1.as_object());
        
        check(v1.to_string() == "null");
        check(v2.to_string() == "0");
        check(v3.to_string() == "1");
        check(v4.to_string() == INTEGER_MAX_STR);
        check(v5.to_string() == INTEGER_MIN_STR);
        // check(v6.to_string() == "1.1");
        check(v7.to_string() == "false");
        check(v8.to_string() == "true");
        check(v9.to_string() == "''");
        check(v10.to_string() == "'abc'");
        check(vst.to_string() == "'def'");
        check(v12.to_string() == "'x'");
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
        v = o;                 check(v.as_object() == o);
        check(v != null);
        v = null;
        check(!v.is(variant::OBJECT));
        check(v == null);

/*
        // str
        vst = "";
        check(vst.empty()); check(vst.size() == 0);
        vst.append("abc");
        vst.append("");
        vst.append(s1);
        vst.append("ghijkl");
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
        check(vr.left() == 5 && vr.right() == 2);
        check(!vr.has(-2));
        variant vr4 = 0;
        vr4.assign(0, 5);
        check(vr4.left() == 0 && vr4.right() == 5);
        check_throw(vr4.has("abc"));
        variant vra = vr;
        check(vra == vr);

        // vector
        check(vt.empty()); check(vt.size() == 0);
        check_throw(vt.insert(1, 0));
        vt.push_back(0);
        vt.push_back("abc");
        vt.insert(vt.size(), 'd');
        vt.push_back(vs);
        vt.insert(1, true);
        check(vt.to_string() == "[0, true, \"abc\", 'd', []]");
        check(!vt.empty()); check(vt.size() == 5);
        check_throw(vt.insert(10, 0));
        check_throw(vt.insert(mem(-1), 0));
        vt.erase(2);
        check(vt.to_string() == "[0, true, 'd', []]");
        check(vt[2] == 'd');
        vt.put(2, "asd");
        check(vt.to_string() == "[0, true, \"asd\", []]");
        check_throw(vt.put(4, 0));
        vt.resize(2);
        check(vt.to_string() == "[0, true]");
        vt.resize(4);
        check(vt.to_string() == "[0, true, null, null]");
        vt.put(2, "abc");
        vt.put(3, new_range(1, 2));
        check(vt.to_string() == "[0, true, \"abc\", [1..2]]");
        check(vt.size() == 4);
        variant vta = vt;
        check(vta == vt);
        vt.put(0, 100);
        check(!vt.is_unique()); check(!vta.is_unique());
        check(vt.to_string() == "[100, true, \"abc\", [1..2]]");
        check(vta.to_string() == "[100, true, \"abc\", [1..2]]");
        vta.unique();
        check(vt.is_unique()); check(vta.is_unique());
        check(vta.to_string() == "[100, true, \"abc\", [1..2]]");
        vta.put(2, "def");
        check(vt.to_string() == "[100, true, \"abc\", [1..2]]");
        check(vta.to_string() == "[100, true, \"def\", [1..2]]");

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
        str_fifo vds;
        vforeach(dict, i, vd)
            vds << ' ' << i->first << ": " << i->second;
        check(vds.all() == " \"k1\": \"abc\" \"k3\": []");
        vd.untie("k2");
        check(vd.to_string() == "[\"k1\": \"abc\", \"k3\": []]");
        vd.untie("k3");
        check(vd.to_string() == "[\"k1\": \"abc\"]");
        check(vd["k1"] == "abc");
        check(vd["k2"] == null);
        check(vd["kz"] == null);
        check(vd.has("k1"));
        check(!vd.has("k2"));
        
        variant vda = vd;
        check(vda == vd);
        vda.tie(1, true);
        check(vd.to_string() == "[1: true, \"k1\": \"abc\"]");
        check(vda.to_string() == "[1: true, \"k1\": \"abc\"]");
        vda.unique();
        vda.untie("k1");
        check(vd.to_string() == "[1: true, \"k1\": \"abc\"]");
        check(vda.to_string() == "[1: true]");
        check(vd.is_unique());  check(vd.is_unique());

        variant vdb = new_dict();
        variant vdc = new_dict();
        vdb.tie(1, 2);
        vd.tie(vdb, 100);
        vd.tie(vdc, 101);
        // non-deterministic, depends on addresses: check(vd.to_string() == "[[1: 2]: false, []: true]");
        check(vd[vdb] == 100);
        check(vd[vdc] == 101);

        vd = new_dict();
        vd.tie(new_dict(), 6);
        vd.tie("abc", 5);
        // vd.tie(1.1, 4);
        vd.tie(10, 3);
        vd.tie('a', 2);
        vd.tie(false, 1);
        vd.tie(null, 0);
        check(vd.to_string() == "[null: 0, false: 1, 'a': 2, 10: 3, \"abc\": 5, []: 6]");
        
        // dict[range]
        vd = new_dict();
        vd.tie(new_range(0, 4), new_range(0, 1));
        vd.tie(new_range(5, 6), "abc");
        check(vd.has(new_range(5, 6)));
        check(!vd.has(new_range()));
        check(!vd.has(new_range(0, 6)));
        
        // ordset
        check(vos.empty()); check_throw(vos.size());
        vos.tie(5);
        vos.tie(131);
        vos.tie(255);
        check_throw(vos.tie(256));
        check_throw(vos.tie(1000));
        check_throw(vos.tie(-1));
        check_throw(vos.untie(1000));
        check_throw(vos.untie(-1));
        check_throw(vos.tie("abc"));
        check(vos.to_string() == "[5, 131, 255]");
        check(!vos.empty());
        vos.untie(131);
        vos.untie(30);
        check(vos.to_string() == "[5, 255]");
        check(vos.has(5));
        check(vos.has(char(255)));
        check(!vos.has(26));
        variant vosa = vos;
        check(vosa == vos);
        check(vosa.to_string() == "[5, 255]");
        check(!vosa.is_unique()); check(!vos.is_unique()); 
        vosa.tie(100);
        check(vos.to_string() == "[5, 100, 255]");
        check(vosa.to_string() == "[5, 100, 255]");
        vosa.unique();
        vosa.untie(255);
        check(vos.is_unique()); check(vos.to_string() == "[5, 100, 255]");
        check(vos.is_unique()); check(vosa.to_string() == "[5, 100]");

        // set
        check(vs.empty()); check_throw(vs.size());
        vs.tie(5);
        vs.tie(26);
        vs.tie(127);
        check(vs.to_string() == "[5, 26, 127]");
        str_fifo vdss;
        vforeach(set, i, vs)
            vdss << ' ' << *i;
        check(vdss.all() == " 5 26 127");
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
        // vs.tie(1.1);
        // vs.tie(2.2);
        vs.tie(true);
        vs.tie(null);
        check(vs.to_string() == "[null, true, 5, 127, \"abc\", \"def\"]")
        // check(vs.has(1.1));

        str_fifo ss;
        ss << vs;
        check(ss.all() == "[null, true, 5, 127, \"abc\", \"def\"]");
        
        // ref counting & cloning
        vs = new_set();
        vs.tie(0);
        check(vs.as_set().is_unique());
        variant vss = vs;
        check(vss == vs);
        check(vss.has(0));
        check(!vs.as_set().is_unique());
        check(!vss.as_set().is_unique());
        check(vs.to_string() == "[0]");
        check(vss.to_string() == "[0]");
        vs.tie(1);
        check(vs.to_string() == "[0, 1]");
        check(vss.to_string() == "[0, 1]");
        variant vsa = vs;
        vsa.unique();
        vsa.tie(2);
        check(vsa.to_string() == "[0, 1, 2]");
        check(vs.to_string() == "[0, 1]");
*/        
        variant voa = vo;
        check_throw(voa.unique());
        
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
        objptr<Symbol> s = new Symbol(Symbol::THISVAR, NULL, "sym");
        check(s->name == "sym");
        SymbolTable<Symbol> t;
        check(t.empty());
        objptr<Symbol> s1 = new Symbol(Symbol::THISVAR, NULL, "abc");
        check(s1->is_unique());
        t.addUnique(s1);
        check(s1->is_unique());
        objptr<Symbol> s2 = new Symbol(Symbol::THISVAR, NULL, "def");
        t.addUnique(s2);
        objptr<Symbol> s3 = new Symbol(Symbol::THISVAR, NULL, "abc");
        check_throw(t.addUnique(s3));
        check(t.find("abc") == s1);
        check(s2 == t.find("def"));
        check(t.find("xyz") == NULL);
        check(!t.empty());

        List<Symbol> l;
        check(l.size() == 0);
        check(l.empty());
        l.add(s1);
        check(!s1->is_unique());
        l.add(new Symbol(Symbol::THISVAR, NULL, "ghi"));
        check(l.size() == 2);
        check(!l.empty());
    }
    check(object::alloc == save_alloc);
}


void test_source()
{
    {
        in_text file(NULL, "nonexistent");
        check_throw(file.open());
        check_throw(file.get());
    }
    {
        str_fifo m(NULL, "one\t two\n567");
        check(m.preview() == 'o');
        check(m.get() == 'o');
        check(m.token(identRest) == "ne");
        m.skip(wsChars);
        check(m.token(identRest) == "two");
        check(m.eol());
        check(!m.eof());
        m.skip_eol();
        check(m.token(digits) == "567");
        check(m.eol());
        check(m.eof());
    }
    {
#ifdef XCODE
        const char* fn = "../../src/tests/parser.txt";
#else
        const char* fn = "tests/parser.txt";
#endif
        Parser p(fn, new in_text(NULL, fn));
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
        Parser p("mem", new str_fifo(NULL,
            INTEGER_MAX_STR"\n  "INTEGER_MAX_STR_PLUS"\n  if\n aaa"
            " 'asd\n'[\\t\\r\\n\\x41\\\\]' '\\xz'"));
        check(p.next() == tokIntValue);
        check(p.intValue == INTEGER_MAX);
        check(p.next() == tokSep);
        check(p.getLineNum() == 2);
        check(p.getIndent() == 2);
        check(p.next() == tokIndent);
        check_throw(p.next()); // integer overflow
        check(p.next() == tokSep);
        check(p.getLineNum() == 3);
        check(p.next() == tokIf);
        check(p.next() == tokSep);
        check(p.getLineNum() == 4);
        check_throw(p.next()); // unmatched unindent
        check(p.next() == tokIndent);
        check(p.next() == tokIdent);
        check_throw(p.next()); // unexpected end of line
        check(p.next() == tokStrValue);
        check(p.strValue == "[\t\r\nA\\]");
        check_throw(p.next()); // bad hex sequence
    }
}


void test_bidir_char_fifo(fifo_intf& fc)
{
    check(fc.is_char_fifo());
    fc.enq("0123456789abcdefghijklmnopqrstuvwxy");
    fc.var_enq(variant('z'));
    fc.var_enq(variant("./"));
    check(fc.preview() == '0');
    check(fc.get() == '0');
    variant v;
    fc.var_deq(v);
    check(v.as_char() == '1');
    v.clear();
    fc.var_preview(v);
    check(v.as_char() == '2');
    check(fc.deq(16) == "23456789abcdefgh");
    check(fc.deq(fifo::CHAR_ALL) == "ijklmnopqrstuvwxyz./");
    check(fc.empty());

    fc.enq("0123456789");
    fc.enq("abcdefghijklmnopqrstuvwxyz");
    check(fc.get() == '0');
    while (!fc.empty())
        fc.deq(fifo_intf::CHAR_SOME);

    fc.enq("0123456789abcdefghijklmnopqrstuvwxyz");
    check(fc.deq("0-9") == "0123456789");
    check(fc.deq("a-z") == "abcdefghijklmnopqrstuvwxyz");
    check(fc.empty());
}


void test_fifos()
{
#ifdef DEBUG
    fifo::CHUNK_SIZE = 2 * sizeof(variant);
#endif

    fifo f(NULL, false);
    objptr<vector> t = new vector(NULL);
    t->push_back(0);
    f.var_enq((vector*)t);
    f.var_enq("abc");
    f.var_enq("def");
    variant w = new range(NULL, 1, 2);
    f.var_enq(w);
    // f.dump(std::cout); std::cout << std::endl;
    variant x;
    f.var_deq(x);
    check(x.is_object());
    f.var_deq(w);
    check(w.is(variant::STR));
    f.var_eat();
    variant vr;
    f.var_preview(vr);
    check(vr.is_object());

    fifo fc(NULL, true);
    test_bidir_char_fifo(fc);
    
    str_fifo fs(NULL);
    test_bidir_char_fifo(fs);
}


void test_typesys()
{
    initTypeSys();
    try
    {
    check(defTypeRef->isTypeRef());
    check(defTypeRef->get_rt() == defTypeRef);
    check(queenBee->defNone->isNone());
    check(queenBee->defNone->get_rt() == defTypeRef);
    check(queenBee->defInt->isInt());
    check(queenBee->defInt->get_rt() == defTypeRef);
    check(queenBee->defInt->isOrdinal());
    check(queenBee->defBool->isBool());
    check(queenBee->defBool->get_rt() == defTypeRef);
    check(queenBee->defBool->isEnum());
    check(queenBee->defBool->isOrdinal());
    check(queenBee->defBool->rangeEq(0, 1));
    check(queenBee->defChar->isChar());
    check(queenBee->defChar->get_rt() == defTypeRef);
    check(queenBee->defChar->isOrdinal());
    check(queenBee->defStr->isStr());
    check(queenBee->defStr->get_rt() == defTypeRef);
    check(queenBee->defStr->isContainer());
    check(queenBee->defChar->deriveVector() == queenBee->defStr);

    Symbol* b = queenBee->deepFind("true");
    check(b != NULL && b->isDefinition() && b->isConstant());
    check(PDef(b)->value.as_int() == 1);
    check(PDef(b)->type->isBool());

    {
        State state(NULL, queenBee, queenBee->defBool);
        b = state.deepFind("true");
        check(b != NULL && b->isDefinition());
        check(state.deepFind("untrue") == NULL);
        state.addThisVar(queenBee->defInt, "a");
        check_throw(state.addThisVar(queenBee->defInt, "a"));
        state.addThisVar(queenBee->defInt, "true");
        state.addTypeAlias("ool", queenBee->defBool);
        b = state.deepFind("ool");
        check(b != NULL && b->isDefinition());
        check(b->isTypeAlias());
        check(PTypeAlias(b)->aliasedType->isBool());
        state.addThisVar(queenBee->defInt, "v");
        Symbol* v = state.deepFind("v");
        check(v->isThisVar());
        check(PVar(v)->id == 2);
        state.addThisVar(queenBee->defChar, "c1");
        state.addThisVar(queenBee->defChar, "c2");
        v = state.deepFind("c2");
        check(v->isThisVar());
        check(PVar(v)->id == 4);
        b = state.deepFind("result");
        check(b != NULL);
        check(b->isResultVar());
        check(PVar(b)->type->isBool());
    }
    }
    catch(exception&)
    {
        doneTypeSys();
    }
    doneTypeSys();
}


void test_vm()
{
    initTypeSys();

    try
    {

    {
        Module mod("test");

        Constant* c = mod.addConstant(queenBee->defChar, "c", char(1));

        // Arithmetic, typecasts
        variant r;
        ConstCode seg;
        {
            CodeGen gen(&seg);
            gen.loadConst(c->type, c->value);
            gen.explicitCastTo(queenBee->defVariant);
            gen.explicitCastTo(queenBee->defBool);
            gen.explicitCastTo(queenBee->defInt);
            gen.loadInt(9);
            gen.arithmBinary(opAdd);
            gen.loadInt(2);
            gen.arithmUnary(opNeg);
            gen.arithmBinary(opSub);
            gen.explicitCastTo(queenBee->defStr);
            gen.endConstExpr(queenBee->defStr);
        }
        seg.run(r);
        check(r.as_str() == "12");

        // String operations
        c = mod.addConstant(queenBee->defStr, "s", "ef");
        seg.clear();
        {
            CodeGen gen(&seg);
            gen.loadChar('a');
            gen.elemToVec();
            gen.loadChar('b');
            gen.elemCat();
            gen.loadStr("cd");
            gen.cat();
            gen.loadStr("");
            gen.cat();
            gen.loadConst(c->type, c->value);
            gen.cat();
            gen.endConstExpr(queenBee->defStr);
        }
        seg.run(r);
        check(r.as_str() == "abcdef");

        // Range operations
        seg.clear();
        {
            CodeGen gen(&seg);
            gen.loadInt(6);
            gen.loadInt(5);
            gen.loadInt(10);
            gen.mkRange();
            gen.inRange();
            gen.loadInt(1);
            gen.loadInt(5);
            gen.loadInt(10);
            gen.mkRange();
            gen.inRange();
            gen.mkRange();
            gen.endConstExpr(queenBee->defBool->deriveRange());
        }
        seg.run(r);
        check(prange(r.as_object())->get_rt()->isRange()
            && prange(r.as_object())->equals(1, 0));

        // Vector concatenation
        vector* t = new vector(queenBee->defInt->deriveVector(), 1, 3);
        t->push_back(4);
        c = mod.addConstant(queenBee->defInt->deriveVector(), "v", t);
        seg.clear();
        {
            CodeGen gen(&seg);
            gen.loadInt(1);
            gen.elemToVec();
            gen.loadInt(2);
            gen.elemCat();
            gen.loadConst(c->type, c->value);
            gen.cat();
            gen.endConstExpr(queenBee->defInt->deriveVector());
        }
        seg.run(r);
        check(r.to_string() == "[1, 2, 3, 4]");

        seg.clear();
        {
            CodeGen gen(&seg);
            gen.loadBool(true);
            gen.elemToVec();

            gen.loadStr("abc");
            gen.loadStr("abc");
            gen.cmp(opEqual);
            gen.elemCat();

            gen.loadInt(1);
            gen.elemToVec();
            gen.loadTypeRef(queenBee->defInt->deriveVector());
            gen.dynamicCast();
            gen.loadTypeRef(queenBee->defInt->deriveVector());
            gen.testType();
            gen.elemCat();

            mem s = seg.size();
            gen.loadInt(1);
            gen.testType(queenBee->defInt); // compile-time
            gen.testType(queenBee->defBool);
            check(s == seg.size() - 1);
            gen.elemCat();
            gen.loadConst(queenBee->defVariant, 2); // doesn't yield variant actually
            gen.implicitCastTo(queenBee->defVariant, "Type mismatch");
            gen.testType(queenBee->defVariant);
            gen.elemCat();
            gen.loadStr("");
            gen.explicitCastTo(queenBee->defBool);
            gen.elemCat();
            gen.loadStr("abc");
            gen.explicitCastTo(queenBee->defBool);
            gen.elemCat();
            gen.endConstExpr(queenBee->defBool->deriveVector());
        }
        seg.run(r);
        check(r.to_string() == "[true, true, true, true, true, false, true]");
    }

    {
        Module mod("test2");
        Dict* dictType = mod.registerType(new Dict(queenBee->defStr, queenBee->defInt));
        dict* d = new dict(dictType);
        d->tie("key1", 2);
        d->tie("key2", 3);
        Constant* c = mod.addConstant(dictType, "dict", d);
        Array* arrayType = mod.registerType(new Array(queenBee->defBool, queenBee->defStr));
        Ordset* ordsetType = queenBee->defChar->deriveSet();
        Set* setType = queenBee->defInt->deriveSet();
        check(!setType->isOrdset());
        {
            CodeGen gen(&mod);
            BlockScope block(&mod, &gen);

            Variable* v1 = block.addLocalVar(dictType, "v1");
            gen.loadNullContainer(dictType);
            gen.initLocalVar(v1);
            gen.loadVar(v1);
            gen.loadStr("k1");
            gen.loadInt(15);
            gen.storeContainerElem();
            gen.loadVar(v1);
            gen.loadStr("k2");
            gen.loadInt(25);
            gen.storeContainerElem();

            Variable* v2 = block.addLocalVar(arrayType, "v2");
            gen.loadNullContainer(arrayType);
            gen.initLocalVar(v2);
            gen.loadVar(v2);
            gen.loadBool(false);
            gen.loadStr("abc");
            gen.storeContainerElem();
            gen.loadVar(v2);
            gen.loadBool(true);
            gen.loadStr("def");
            gen.storeContainerElem();

            Variable* v3 = block.addLocalVar(ordsetType, "v3");
            gen.loadNullContainer(ordsetType);
            gen.initLocalVar(v3);
            gen.loadVar(v3);
            gen.loadChar('a');
            gen.addToSet();
            gen.loadVar(v3);
            gen.loadChar('b');
            gen.addToSet();

            Variable* v4 = block.addLocalVar(setType, "v4");
            gen.loadNullContainer(setType);
            gen.initLocalVar(v4);
            gen.loadVar(v4);
            gen.loadInt(100);
            gen.addToSet();
            gen.loadVar(v4);
            gen.loadInt(1000);
            gen.addToSet();

            gen.loadConst(queenBee->defVariant, 10);
            gen.elemToVec();
            gen.loadStr("xyz");
            gen.loadInt(1);
            gen.loadContainerElem();
            gen.elemCat();
            gen.dup();
            gen.loadInt(0);
            gen.loadContainerElem();
            gen.elemCat();
            gen.loadConst(c->type, c->value);
            gen.loadStr("key2");
            gen.loadContainerElem();
            gen.elemCat();
            gen.loadVar(v1);
            gen.elemCat();
            gen.loadVar(v2);
            gen.elemCat();
            gen.dup();
            gen.loadInt(6);
            gen.loadInt(21);
            gen.storeContainerElem();
            gen.dup();
            gen.loadInt(6);
            gen.loadInt(22);
            gen.storeContainerElem();
            gen.loadVar(v3);
            gen.elemCat();
            gen.loadVar(v4);
            gen.elemCat();

            gen.loadChar('a');
            gen.loadVar(v3);
            gen.inSet();
            gen.elemCat();

            gen.loadChar('c');
            gen.loadVar(v3);
            gen.inSet();
            gen.elemCat();

            gen.loadInt(1000);
            gen.loadVar(v4);
            gen.inSet();
            gen.elemCat();

            gen.loadInt(1001);
            gen.loadVar(v4);
            gen.inSet();
            gen.elemCat();

            gen.loadStr("k3");
            gen.loadVar(v1);
            gen.inDictKeys();
            gen.elemCat();

            gen.loadStr("k1");
            gen.loadVar(v1);
            gen.inDictKeys();
            gen.elemCat();

            ThisVar* var = mod.addThisVar(queenBee->defInt, "var");
            gen.loadInt(200);
            gen.initThisVar(var);
            gen.loadVar(var);
            gen.elemCat();

            gen.loadVar(v1);
            gen.loadStr("k2");
            gen.delDictElem();

            Symbol* io = mod.deepFind("sio");
            check(io != NULL);
            check(io->isVariable());
            gen.loadVar(PVar(io));
            gen.elemCat();

            Symbol* r = mod.deepFind("sresult");
            check(r != NULL);
            check(r->isVariable());
            gen.loadVar(PVar(r));
            gen.elemCat();

            gen.storeVar(queenBee->sresultvar);
            block.deinitLocals();
            gen.end();
        }
        variant result = mod.run();
        str s = result.to_string();
        check(s ==
            "[10, 'y', 10, 3, ['k1': 15], ['abc', 'def'], 22, [97, 98], [100, 1000], "
            "true, false, true, false, false, true, 200, [<char-fifo>], null]");
    }

    {
        Module mod("test2");
        {
            CodeGen gen(&mod);
            BlockScope block(&mod, &gen);

            Variable* s0 = block.addLocalVar(queenBee->defVariant->deriveVector(), "s0");
            gen.loadNullContainer(queenBee->defVariant->deriveVector());
            gen.initLocalVar(s0);

            Variable* s1 = block.addLocalVar(queenBee->defStr, "s1");
            gen.loadStr("abc");
            gen.loadStr("def");
            gen.cat();
            gen.initLocalVar(s1);

            Variable* s2 = block.addLocalVar(queenBee->defInt, "s2");
            gen.loadInt(123);
            gen.initLocalVar(s2);

            gen.loadVar(s0);
            gen.loadVar(s1);
            gen.elemCat();
            gen.loadVar(s2);
            gen.elemCat();
            gen.dup();
            gen.count();
            gen.elemCat();

            mem o1 = gen.jumpForward();
            gen.nop();
            gen.resolveJump(o1);

            // if(true, 10, 20)
            gen.loadBool(true);
            mem of = gen.boolJumpForward(false);
            gen.loadInt(10);
            gen.genPop();
            mem oj = gen.jumpForward();
            gen.resolveJump(of);
            gen.loadInt(20);
            gen.resolveJump(oj);
            gen.elemCat();

            gen.loadChar('a');
            gen.caseLabel(queenBee->defChar, 'a');
            gen.swap();
            gen.discard();
            gen.elemCat();

            gen.loadInt(10);
            gen.caseLabel(queenBee->defInt->deriveRange(), new range(NULL, 9, 11));
            gen.swap();
            gen.discard();
            gen.elemCat();

            gen.loadStr("22:49");
            gen.caseLabel(queenBee->defStr, "22:49");
            gen.swap();
            gen.discard();
            gen.elemCat();

            gen.loadTypeRef(queenBee->defInt);
            gen.caseLabel(defTypeRef, queenBee->defInt);
            gen.swap();
            gen.discard();
            gen.elemCat();

//            gen.loadStr("The value of true is: ");
//            gen.echo();
//            gen.loadBool(true);
//            gen.echo();
//            gen.echoLn();
//            mem f = mod.registerAssertFileName(__FILE__);
//            gen.loadBool(false);
//            gen.assertion(f, __LINE__);

            gen.exit();

            block.deinitLocals();   // not reached
            gen.end();
        }
        variant result = mod.run();
        str s = result.to_string();
        check(s == "['abcdef', 123, 2, 10, true, true, true, true]");
    }

    }
    catch (exception&)
    {
        doneTypeSys();
        throw;
    }

    doneTypeSys();
}


int main()
{
    sio << "short: " << sizeof(short) << "  long: " << sizeof(long)
         << "  long long: " << sizeof(long long) << "  int: " << sizeof(int)
         << "  void*: " << sizeof(void*) << "  float: " << sizeof(float)
         << "  double: " << sizeof(double) << '\n';
    sio << "integer: " << sizeof(integer) << "  mem: " << sizeof(mem)
         << "  real: " << sizeof(real) << "  variant: " << sizeof(variant)
         << "  object: " << sizeof(object) << "  joffs: " << sizeof(joffs_t) << '\n';

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

    int exitcode = 0;
    try
    {
        test_common();
        test_variant();
        test_symbols();
        test_source();
        test_fifos();
        test_typesys();
        test_vm();
    }
    catch (exception& e)
    {
        fprintf(stderr, "Exception: %s\n", e.what());
        exitcode = 201;
    }

    if (object::alloc != 0)
    {
        fprintf(stderr, "Error: object::alloc = %d\n", object::alloc);
        exitcode = 202;
    }

    return exitcode;
}

