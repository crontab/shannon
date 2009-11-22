
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
    // make sure the assembly code for atomic ops is sane
    int i = 1;
    check(pincrement(&i) == 2);
    check(pdecrement(&i) == 1);

/*
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
*/
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
    }
    catch (exception& e)
    {
        fprintf(stderr, "Exception: %s\n", e.what());
        exitcode = 201;
    }

/*
    if (object::alloc != 0)
    {
        fprintf(stderr, "Error: object::alloc = %d\n", object::alloc);
        exitcode = 202;
    }
*/
    return exitcode;
}

