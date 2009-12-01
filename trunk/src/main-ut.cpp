
#define __STDC_LIMIT_MACROS

#include "common.h"
#include "runtime.h"

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
    int i = 1;
    check(pincrement(&i) == 2);
    check(pdecrement(&i) == 1);
}


void test_rcblock()
{
    rcblock* b = rcref(rcblock::alloc(10));
    check(b->refcount == 1);
    rcblock* c = rcref(b);
    check(b->refcount == 2);
    rcrelease(c);
    check(b->refcount == 1);
    rcassign(b, rcblock::alloc(20));
    rcrelease(b);
}


void test_container()
{
    // TODO: check the number of reallocations
    container c1;
    check(c1.empty());
    check(c1.size() == 0);
    check(c1.memsize() == 0);

    container c2("ABC", 3);
    check(!c2.empty());
    check(c2.size() == 3);
    check(c2.memsize() == 3);

    check(c1.unique());
    check(c2.unique());
    c1.assign(c2);
    check(!c2.unique());
    check(!c1.unique());
    container c3("DEFG", 4);
    c1.insert(3, c3);
    check(c1.unique());
    check(c1.size() == 7);
    check(c1.memsize() > 7);
    check(memcmp(c1.data(), "ABCDEFG", 7) == 0);

    container c4 = c1;
    check(!c1.unique());
    c1.insert(3, "ab", 2);
    check(c1.unique());
    check(c1.size() == 9);
    check(c1.memsize() > 9);
    check(memcmp(c1.data(), "ABCabDEFG", 9) == 0);
    c1.insert(0, "@", 1);
    check(c1.unique());
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
    check(!c2.unique());
    c2.append(c3);
    check(c2.unique());
    check(c2.size() == 14);
    check(memcmp(c2.data(), "ABCABCabcdDEFG", 14) == 0);

    c1.erase(4, 2);
    check(memcmp(c1.data(), "@ABCDEFG0123456789", 18) == 0);
    c4 = c1;
    c1.erase(8, 5);
    check(memcmp(c1.data(), "@ABCDEFG56789", 13) == 0);
    c1.erase(8, 5);
    check(memcmp(c1.data(), "@ABCDEFG", 8) == 0);

    c1.push_back('!');
    check(c1.size() == 9);
    check(memcmp(c1.data(), "@ABCDEFG!", 9) == 0);
    c4 = c1;
    c1.push_back('?');
    check(c1.size() == 10);
    check(memcmp(c1.data(), "@ABCDEFG!?", 10) == 0);
    c1.pop_back<char>();
    check(memcmp(c1.data(), "@ABCDEFG!", 9) == 0);
    c1.pop_back(9);
    check(c1.empty());

    check(container::_null.valid());
}


void test_string()
{
    str s1;
    check(s1.empty());
    check(s1.size() == 0);
    check(s1.c_str()[0] == 0);
    str s2 = "Kuku";
    check(!s2.empty());
    check(s2.size() == 4);
    check(s2.memsize() == 4);
    check(s2 == "Kuku");
    str s3 = s1;
    check(s3.empty());
    str s4 = s2;
    check(s4 == s2);
    check(s4 == "Kuku");
    check(!s4.unique());
    check(!s2.unique());
    str s5 = "!";
    check(s5.size() == 1);
    check(s5.c_str()[0] == '!');
    check(s5.c_str()[1] == 0);
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
}


void test_strutils()
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
}



int main()
{
/*
    sio << "short: " << sizeof(short) << "  long: " << sizeof(long)
         << "  long long: " << sizeof(long long) << "  int: " << sizeof(int)
         << "  void*: " << sizeof(void*) << "  float: " << sizeof(float)
         << "  double: " << sizeof(double) << '\n';
    sio << "integer: " << sizeof(integer) << "  mem: " << sizeof(mem)
         << "  real: " << sizeof(real) << "  variant: " << sizeof(variant)
         << "  object: " << sizeof(object) << "  joffs: " << sizeof(joffs_t) << '\n';
*/
    check(sizeof(int) == 4);
    check(sizeof(memint) >= 4);

#ifdef SH64
    check(sizeof(integer) == 8);
#  ifdef PTR64
//    check(sizeof(variant) == 16);
#  else
//    check(sizeof(variant) == 12);
#  endif
#else
    check(sizeof(integer) == 4);
//    check(sizeof(variant) == 8);
#endif

    int exitcode = 0;
    try
    {
        test_common();
        test_rcblock();
        test_container();
        test_string();
        test_strutils();
    }
    catch (exception& e)
    {
        fprintf(stderr, "Exception: %s\n", e.what());
        exitcode = 201;
    }

    if (rcblock::allocated != 0)
    {
        fprintf(stderr, "Error: object::alloc = %d\n", rcblock::allocated);
        exitcode = 202;
    }

    return exitcode;
}

