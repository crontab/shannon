
// We rely on assert(), so make sure it is turned on for this program
#ifdef NDEBUG
#  undef NDEBUG
#endif

#define __STDC_LIMIT_MACROS

#include <stdint.h>
#include <stdio.h>
#include <limits.h>

#include <iostream>

#include "variant.h"

using namespace std;

#define fail(e) \
    (printf("%s:%u: test failed `%s'\n", __FILE__, __LINE__, e), exit(200))

#define check(e) \
    { if (!(e)) fail(#e); }

#define check_throw(e,...) \
    { bool chk_throw = false; try { __VA_ARGS__; } catch(e&) { chk_throw = true; } check(chk_throw); }

#define check_nothrow(...) \
    { try { __VA_ARGS__; } catch(...) { fail("exception thrown"); } }


void test_variant()
{
    variant v1 = none;              check(v1.is_none());
    variant v2 = 0;                 check(v2.is_int());     check(v2.as_int() == 0);
    variant v3 = 1;                 check(v3.is_int());     check(v3.as_int() == 1);
    variant v4 = INT64_MAX;         check(v4.is_int());     check(v4.as_int() == INT64_MAX);
    variant v5 = INT64_MIN;         check(v5.is_int());     check(v5.as_int() == INT64_MIN);
    variant v6 = 1.1;               check(v6.is_real());    check(v6.as_real() == 1.1);
    variant v7 = false;             check(v7.is_bool());    check(!v7.as_bool());
    variant v8 = true;              check(v8.is_bool());    check(v8.as_bool());
    variant v9 = "";                check(v9.is_str());     check(v9.as_str().empty());
    variant v10 = "abc";            check(v10.is_str());    check(v10.as_str() == "abc");
    string s1 = "def";
    variant v11 = s1;               check(v11.is_str());    check(v11.as_str() == s1);
    variant v12 = 'x';              check(v12.is_char());   check(v12.as_char() == 'x');
    variant v13 = new_tuple();      check(v13.is_tuple());  check(&v13.as_tuple() == &null_tuple);
    variant v14 = new_dict();       check(v14.is_dict());   check(&v14.as_dict() == &null_dict);
    variant v15 = new_set();        check(v15.is_set());    check(&v15.as_set() == &null_set);
    object* o = new object();
    variant v16 = o;                check(v16.is_object()); check(v16.as_object() == o);

    check_throw(evarianttype, v1.as_int());
    check_throw(evarianttype, v1.as_real());
    check_throw(evarianttype, v1.as_bool());
    check_throw(evarianttype, v1.as_char());
    check_throw(evarianttype, v1.as_str());
    check_throw(evarianttype, v1.as_tuple());
    check_throw(evarianttype, v1.as_dict());
    check_throw(evarianttype, v1.as_set());
    check_throw(evarianttype, v1.as_object());
}


int main()
{
    try
    {
        test_variant();
    }
    catch (exception& e)
    {
        fail(e.what());
    }

    check(object::alloc == 0);

    return 0;
}
