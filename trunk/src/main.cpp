
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
    sio << "Shannon " << SHANNON_VERSION_MAJOR << '.' << SHANNON_VERSION_MINOR << '.' << SHANNON_VERSION_FIX
        << " (" << sizeof(integer) * 8 << ')'
        << ' ' << SHANNON_COPYRIGHT << endl << endl;

    int exitcode = 0;

    initRuntime();
    initTypeSys();
    initVm();

    try
    {
        Context context;
        // context.options.setDebugOpts(false);
        variant result = context.execute(filePath);

        if (result.is_null())
            exitcode = 0;
        else if (result.is(variant::ORD))
            exitcode = int(result._int());
        else if (result.is(variant::STR))
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

    doneVm();
    doneTypeSys();
    doneRuntime();

#ifdef DEBUG
    // TODO: make this a compiler option
    if (object::allocated != 0)
    {
        fprintf(stderr, "object::allocated: %d\n", object::allocated);
        _fatal(0xff01);
    }
#endif

    return exitcode;
}

