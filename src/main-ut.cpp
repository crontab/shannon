
#define __STDC_LIMIT_MACROS

#include "common.h"
#include "runtime.h"
#include "parser.h"
#include "typesys.h"
#include "vm.h"
#include "compiler.h"


static void ut_fail(unsigned line, const char* e)
{
    fprintf(stderr, "%s:%u: test failed `%s'\n", __FILE__, line, e);
    exit(200);
}

#define fail(e)     ut_fail(__LINE__, e)
#define check(e)    { if (!(e)) fail(#e); }

#define check_throw(a) \
    { bool chk_throw = false; try { a; } catch(exception&) { chk_throw = true; } check(chk_throw); }


#define XSTR(s) _STR(s)
#define _STR(s) #s

#ifdef SHN_64
#  define INTEGER_MAX_STR "9223372036854775807"
#  define INTEGER_MAX_STR_PLUS "9223372036854775808"
#  define INTEGER_MIN_STR "-9223372036854775808"
#else
#  define INTEGER_MAX_STR "2147483647"
#  define INTEGER_MAX_STR_PLUS "2147483648"
#  define INTEGER_MIN_STR "-2147483648"
#endif


static void test_common()
{
    int i = 1;
    check(pincrement(&i) == 2);
    check(pdecrement(&i) == 1);
}


struct testobj: public object
{
    testobj()  { }
};

static void test_object()
{
    {
        object* b = (new testobj())->grab();
        check(b->unique());
        object* c = b->grab();
        check(!b->unique());
        c->release();
        check(b->unique());
        b->release();
        b = (new testobj())->grab();
        b->release();
    }
    {
        objptr<object> p3 = new testobj();
        objptr<object> p4 = p3;
        check(!p3.empty());
        check(!p4.empty());
    }
}


static void test_ordset()
{
    ordset s1;
    check(s1.empty());
    s1.insert(129);
    check(s1.has(129));
    check(!s1.has(1));
    check(!s1.empty());
    ordset s2 = s1;
    check(s2.has(129));
    check(!s2.has(1));
    check(!s2.empty());
    s1.erase(129);
    check(s1.empty());
    check(!s2.empty());
    ordset s3;
    s3 = s1;
}


void test_bytevec()
{
    // TODO: check the number of reallocations
    bytevec c1;
    check(c1.begin() == NULL);
    check(c1.empty());
    check(c1._isunique());
    check(c1.size() == 0);
    check(c1.capacity() == 0);

    bytevec c2("ABC", 3);
    check(!c2.empty());
    check(c2.size() == 3);
    check(c2.capacity() == 3);

    check(c1._isunique());
    check(c2._isunique());
    c1 = c2;
    check(!c2._isunique());
    check(!c1._isunique());
    c2.clear();
    check(c1._isunique());
    check(c2._isunique());
    check(c2.empty());
    check(!c1.empty());
    check(c1.size() == 3);
    c1 = c1;

    c2 = c1;
    check(!c2._isunique());
    check(!c1._isunique());
    *c2.atw(0) = 'a';
    check(c2._isunique());
    check(c1._isunique());
    check(c2.data()[0] == 'a');
    check(c1.data()[0] == 'A');
    *c2.atw(0) = 'A';
    check(c2.data()[0] == 'A');

    bytevec c2a("", 0);
    check(c2a.begin() == NULL);
    check(c2a.empty());
    check(c2a.size() == 0);
    check(c2a.capacity() == 0);

    bytevec c3("DEFG", 4);
    c1.insert(3, c3);
    check(c1._isunique());
    check(c1.size() == 7);
    check(c1.capacity() > 7);
    check(memcmp(c1.data(), "ABCDEFG", 7) == 0);

    bytevec c4 = c1;
    check(!c1._isunique());
    c1.insert(3, "ab", 2);
    check(c1._isunique());
    check(c1.size() == 9);
    check(c1.capacity() == 9);
    check(memcmp(c1.data(), "ABCabDEFG", 9) == 0);
    c1.insert(0, "@", 1);
    check(c1._isunique());
    check(c1.size() == 10);
    check(memcmp(c1.data(), "@ABCabDEFG", 10) == 0);
    c1.insert(10, "0123456789", 10);
    check(c1.size() == 20);
    check(memcmp(c1.data(), "@ABCabDEFG0123456789", 20) == 0);

    c2.append(c2);
    check(memcmp(c2.data(), "ABCABC", 6) == 0);
    check(c2.size() == 6);
    c2.append("abcd", 4);
    check(c2.size() == 10);
    check(memcmp(c2.data(), "ABCABCabcd", 10) == 0);
    c4 = c2;
    check(!c2._isunique());
    c2.append(c3);
    check(c2._isunique());
    check(c2.size() == 14);
    check(memcmp(c2.data(), "ABCABCabcdDEFG", 14) == 0);

    c1.erase(4, 2);
    check(memcmp(c1.data(), "@ABCDEFG0123456789", 18) == 0);
    c4 = c1;
    c1.erase(8, 5);
    check(c1.size() == 13);
    check(memcmp(c1.data(), "@ABCDEFG56789", 13) == 0);
    c1.erase(8, 5);
    check(c1.size() == 8);
    check(memcmp(c1.data(), "@ABCDEFG", 8) == 0);

    c1.pop_back(2);
    check(c1.size() == 6);
    check(memcmp(c1.data(), "@ABCDE", 6) == 0);
    c1.pop_back(2);
    check(c1.size() == 4);
    c1.pop_back(4);
    check(c1.empty());
    
    c1.resize(3);
    check(c1.size() == 3);
    check(memcmp(c1.data(), "@AB", 3) == 0);
    c1.resize(6, '!');
    check(c1.size() == 6);
    check(memcmp(c1.data(), "@AB!!!", 3) == 0);
    c1.resize(0);
    check(c1.empty());
    check(c1.begin() == NULL);
}


void test_string()
{
    str s1;
    check(s1.empty());
    check(s1.size() == 0);
    check(s1.c_str()[0] == 0);
    check(s1.begin() == NULL);
    str s2 = "Kuku";
    check(!s2.empty());
    check(s2.size() == 4);
    check(s2.capacity() == 4);
    check(s2 == "Kuku");
    str s3 = s1;
    check(s3.empty());
    str s4 = s2;
    check(s4 == s2);
    check(s4 == "Kuku");
    check(!s4._isunique());
    check(!s2._isunique());
    str s5 = "!";
    check(s5.size() == 1);
    check(s5.c_str()[0] == '!');
    check(s5.c_str()[1] == 0);
    check(s5.size() == 1);
    str s6 = "";
    check(s6.empty());
    check(s6.c_str()[0] == 0);
    s6 = s5;
    check(s6 == s5);
    s5 = s6;
    check(s6 == "!");
    s4 = "Mumu";
    check(s4 == "Mumu");
    check(*s4.data(2) == 'm');

    str s7 = "ABC";
    s7 += "DEFG";
    check(s7.size() == 7);
    check(s7 == "ABCDEFG");
    s7 += "HIJKL";
    check(s7.size() == 12);
    check(s7 == "ABCDEFGHIJKL");
    check(s7.back() == 'L');
    s7 += s4;
    check(s7.size() == 16);
    check(s7 == "ABCDEFGHIJKLMumu");
    s1 += "Bubu";
    check(s1 == "Bubu");
    check(s1.size() == 4);
    s1.append("Tutu", 4);
    check(s1.size() == 8);
    check(s1 == "BubuTutu");
    s1.erase(2, 4);
    check(s1.size() == 4);
    check(s1 == "Butu");
    check(s1.substr(1, 2) == "ut");
    check(s1.substr(1) == "utu");
    check(s1.substr(0) == "Butu");

    check(s1.find('u') == 1);
    check(s1.find('v') == str::npos);
    check(s1.rfind('u') == 3);
    check(s1.rfind('t') == 2);
    check(s1.rfind('B') == 0);
    check(s1.rfind('v') == str::npos);

    s1.clear();
    check(s1.begin() == NULL);

    str s8;
    s8 += "";
    check(s8 == "");
    check(s8.empty());
    s8 += "ABC";
    check(s8 == "ABC");
    s8.clear();
    s8.clear();
    s8.insert(0, "DEF");
    check(s8 == "DEF");
}


static void test_strutils()
{
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

    check(to_printable('a') == "a");
    check(to_printable('\\') == "\\\\");
    check(to_printable('\'') == "\\'");
    check(to_printable('\x00') == "\\x00");
    check(to_printable('\x7f') == "\\x7F");
    check(to_printable("abc \x01'\\") == "abc \\x01\\'\\\\");
}


void test_podvec()
{
    podvec<int> v1;
    check(v1.empty());
    podvec<int> v2 = v1;
    check(v1.empty() && v2.empty());
    v1.push_back(10);
    v1.push_back(20);
    v1.push_back(30);
    v1.push_back(40);
    check(v1.size() == 4);
    check(v2.empty());
    check(v1[0] == 10);
    check(v1[1] == 20);
    check(v1[2] == 30);
    check(v1[3] == 40);
    v2 = v1;
    check(!v1._isunique() && !v2._isunique());
    check(v2.size() == 4);
    v1.erase(2);
    check(v1.size() == 3);
    check(v2.size() == 4);
    check(v1[0] == 10);
    check(v1[1] == 20);
    check(v1[2] == 40);
    v1.erase(2);
    check(v1.size() == 2);
    v1.insert(0, 50);
    check(v1.size() == 3);
    check(v1[0] == 50);
    check(v1[1] == 10);
    check(v1[2] == 20);
    v2.clear();
    check(v2.empty());
    check(!v1.empty());
    check(v1.back() == 20);
}


static void test_vector()
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
    str s1 = "ABC";
    check(v1[0] == s1);
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


static void test_dict()
{
    dict<str, int> d1;
    d1.find_replace("three", 3);
    d1.find_replace("one", 1);
    d1.find_replace("two", 2);
    check(d1.size() == 3);
    check(d1[0].key == "one");
    check(d1[1].key == "three");
    check(d1[2].key == "two");
    check(d1[0].value == 1);
    check(d1[1].value == 3);
    check(d1[2].value == 2);
    dict<str, int> d2 = d1;
    d1.find_erase("three");
    check(d1.size() == 2);
    check(d1[0].key == "one");
    check(d1[1].key == "two");
    check(*d1.find("one") == 1);
    check(d1.find("three") == NULL);
    dict<str, int> d3;
    d3 = d2;
    check(d2 == d3);
    d3.replace(0, 0);
    check(d2 != d3);
    check(d2.size() == 3);
}


static void test_set()
{
    vector<str> s1;
    check(s1.find_insert("GHI"));
    check(s1.find_insert("ABC"));
    check(s1.find_insert("DEF"));
    check(!s1.find_insert("ABC"));
    check(s1.size() == 3);
    check(s1[0] == "ABC");
    check(s1[1] == "DEF");
    check(s1[2] == "GHI");
    s1.find_erase("DEF");
    check(s1.size() == 2);
    check(s1[0] == "ABC");
    check(s1[1] == "GHI");
}


static void test_symtbl()
{
    symtbl_impl s1;
    objptr<symbol> p1 = new symbol("abc");
    s1.insert(0, p1.get());
    check(s1[0] == p1.get());
    check(s1.at(0) == p1.get());
    check(s1.back() == p1.get());
    memint i;
    check(s1.bsearch("abc", i));
    check(i == 0);
    check(!s1.bsearch("def", i));
}


void test_variant()
{
    {
        variant v1;
        check(v1.is(variant::NONE));
    }
    {
        variant v1 = variant::null;
        check(v1.is_none());
        variant v2 = v1;
        check(v2.is_none());
        variant v3;
        v3 = v2;
        check(v3.is_none());
    }
    {
        variant v1 = 10; check(v1.as_int() == 10);
        variant v2 = v1; check(v2.as_int() == 10);
        variant v3; v3 = v2; check(v3.as_int() == 10);
    }
    {
        variant v1 = "abc"; check(v1.as_str() == "abc");
        variant v2 = v1; check(v2.as_str() == "abc");
        variant v3; v3 = v2; check(v3.as_str() == "abc");
        str s = "def";
        variant v4 = s; check(v4.as_str() == "def");
        v4 = 20; check(v4.as_int() == 20);
    }

    {
//        variant v1 = range(20, 50); check(v1.is(variant::RANGE)); check(v1.as_range().equals(20, 50));
//        variant v2 = v1; check(v2.is(variant::RANGE)); check(v2.as_range().equals(20, 50));
//        variant v3; v3 = v1; check(v3.is(variant::RANGE)); check(v3.as_range().equals(20, 50));
//        check(v2 == v3 && v1 == v3);
    }

    {
        variant v1 = varvec();
        check(v1.is(variant::VEC));
        variant v2 = v1;
        check(v1 == v2);
        v2.as_vec().push_back("ABC");
        check(v2.as_vec()[0].as_str() == "ABC");
        check(v1 != v2);
        v1 = 20;
        v1 = v2;
        v2 = 30;
        v1 = v2;
    }
    {
        variant v1 = ordset(); check(v1.is(variant::ORDSET));
    }
    {
        variant v1 = vardict(); check(v1.is(variant::DICT));
    }
}


static void test_bidir_char_fifo(fifo& fc)
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
    check(fc.deq(memfifo::CHAR_ALL) == "ijklmnopqrstuvwxyz./");
    check(fc.empty());

    fc.enq("0123456789");
    fc.enq("abcdefghijklmnopqrstuvwxyz");
    check(fc.get() == '0');
    while (!fc.empty())
        fc.deq(fifo::CHAR_SOME);

    fc.enq("0123456789abcdefghijklmnopqrstuvwxyz");
    check(fc.deq("0-9") == "0123456789");
    check(fc.deq("a-z") == "abcdefghijklmnopqrstuvwxyz");
    check(fc.empty());
}


static void test_fifos()
{
#ifdef DEBUG
    memfifo::CHUNK_SIZE = 2 * sizeof(variant);
#endif

    memfifo f(NULL, false);
    varvec t;
    t.push_back(0);
    f.var_enq(t);
    f.var_enq("abc");
    f.var_enq("def");
    variant w = varset();
    f.var_enq(w);
    // f.dump(std::cout); std::cout << std::endl;
    variant x;
    f.var_deq(x);
    check(x.is(variant::VEC));
    f.var_deq(w);
    check(w.is(variant::STR));
    f.var_eat();
    variant vr;
    f.var_preview(vr);
    check(vr.is(variant::VEC));

    memfifo fc(NULL, true);
    test_bidir_char_fifo(fc);
    
    strfifo fs(NULL);
    test_bidir_char_fifo(fs);
}


static void test_parser()
{
    {
        Parser p(new strfifo(NULL,
            INTEGER_MAX_STR"\n  "INTEGER_MAX_STR_PLUS"\n  if\n aaa"
            " 'asd\n'[\\t\\r\\n\\x41\\\\]' '\\xz'"));
        check(p.next() == tokIntValue);
        check(p.intValue == INTEGER_MAX);
        check(p.next() == tokSep);
        check(p.getLineNum() == 2);
        check(p.next() == tokIntValue);
        check(p.next() == tokSep);
        check(p.getLineNum() == 3);
        check(p.next() == tokIf);
        check(p.next() == tokSep);
        check(p.getLineNum() == 4);
        check(p.next() == tokIdent);
        check_throw(p.next()); // unexpected end of line
        check(p.next() == tokStrValue);
        check(p.strValue == "[\t\r\nA\\]");
        check_throw(p.next()); // bad hex sequence
    }
}


void test_typesys()
{
/*
    {
        State state(Type::MODULE, NULL, NULL);
        objptr<Definition> d1 = new Definition("abc", NULL, 0);
        check(d1->name == "abc");
        state.addDefinition("def", NULL, 1);
        state.addDefinition("ghi", NULL, 2);
        Symbol* s = state.find("def");
        check(s != NULL);
        check(s->isDefinition());
        check(s->name == "def");
        state.findShallow("def");
        state.findDeep("def");
    }
*/
    check(queenBee->defBool->definition("") == "enum(false, true)");
    check(defTypeRef->isTypeRef());
    check(defTypeRef->getType() == defTypeRef);
    check(defNone->isNone());
    check(defNone->getType() == defTypeRef);
    check(queenBee->defInt->isInt());
    check(queenBee->defInt->getType() == defTypeRef);
    check(queenBee->defInt->isAnyOrd());
    check(queenBee->defBool->isBool());
    check(queenBee->defBool->getType() == defTypeRef);
    check(queenBee->defBool->isEnum());
    check(queenBee->defBool->isAnyOrd());
    check(queenBee->defBool->left == 0 && queenBee->defBool->right == 1);
    check(queenBee->defChar->isChar());
    check(queenBee->defChar->getType() == defTypeRef);
    check(queenBee->defChar->isAnyOrd());
    check(queenBee->defStr->hasSmallElem());
    check(queenBee->defStr->getType() == defTypeRef);
    check(queenBee->defStr->isVec());
//    queenBee->registerType(queenBee->defInt->deriveVec());

    Symbol* b = queenBee->find("true");
    check(b != NULL && b->isDefinition());
    check(PDefinition(b)->value.as_int() == 1);
    check(PDefinition(b)->type->isBool());
}


/*
static void _codegen_load(Type* type, const variant& v)
{
    CodeSeg code(NULL);
    CodeGen gen(code);
    gen.loadConst(type, v);
    variant result;
    gen.runConstExpr(type, result);
    check(result == v);
}


void test_codegen()
{
    _codegen_load(queenBee->defInt, 21);
    _codegen_load(queenBee->defStr, "ABC");
    {
        CodeSeg code(NULL);
        CodeGen gen(code);
        gen.loadConst(queenBee->defNullCont, variant());
        variant result;
        gen.runConstExpr(queenBee->defStr, result);
        check(result.is_str() && result == "");
    }
}
*/

int main()
{
    sio << "short: " << sizeof(short) << "  long: " << sizeof(long)
         << "  long long: " << sizeof(long long) << "  int: " << sizeof(int)
         << "  void*: " << sizeof(void*) << "  float: " << sizeof(float)
         << "  double: " << sizeof(double) << '\n';
    sio << "integer: " << sizeof(integer) << "  memint: " << sizeof(memint)
         << "  real: " << sizeof(real) << "  variant: " << sizeof(variant)
         << "  object: " << sizeof(object) << "  rtobject: " << sizeof(rtobject) << '\n';

    check(sizeof(memint) == sizeof(void*));
    check(sizeof(memint) == sizeof(size_t));

#ifdef SHN_64
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

    initRuntime();
    initTypeSys();

    int exitcode = 0;
    try
    {
        test_common();
        test_object();
        test_ordset();
        test_bytevec();
        test_string();
        test_strutils();
        test_podvec();
        test_vector();
        test_dict();
        test_set();
        test_symtbl();
        test_variant();
        test_fifos();
        test_parser();
//        test_typesys();
//        test_codegen();
    }
    catch (exception& e)
    {
        fprintf(stderr, "Exception: %s\n", e.what());
        exitcode = 201;
    }


    doneTypeSys();
    doneRuntime();

    if (object::allocated != 0)
    {
        fprintf(stderr, "Error: object::allocated = %d\n", object::allocated);
        exitcode = 202;
    }

    return exitcode;
}

