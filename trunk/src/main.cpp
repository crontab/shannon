
#include "common.h"
#include "runtime.h"
#include "parser.h"
#include "typesys.h"
#include "vm.h"
#include "compiler.h"


// ------------------------------------------------------------------------- //

// --- tests --------------------------------------------------------------- //


// #include "typesys.h"

/*
void ut_fail(unsigned line, const char* e)
{
    fprintf(stderr, "%s:%u: test failed `%s'\n", __FILE__, line, e);
    exit(200);
}

#define fail(e)     ut_fail(__LINE__, e)
#define check(e)    { if (!(e)) fail(#e); }

#define check_throw(a) \
    { bool chk_throw = false; try { a; } catch(exception&) { chk_throw = true; } check(chk_throw); }
*/


#ifdef XCODE
    const char* filePath = "../../src/tests/test.shn";
#else
    const char* filePath = "tests/test.shn";
#endif


int main()
{
    sio << "Shannon v" << SHANNON_VERSION_MAJOR << '.' << SHANNON_VERSION_MINOR << '.' << SHANNON_VERSION_FIX
        << ' ' << SHANNON_COPYRIGHT << endl << endl;

    int exitcode = 0;

    initRuntime();
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

