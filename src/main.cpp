
#include "common.h"
#include "runtime.h"
#include "parser.h"
#include "typesys.h"
#include "vm.h"
#include "compiler.h"


// ------------------------------------------------------------------------- //


#ifdef XCODE
    const char* filePath = "../../src/tests/test.shn";
#else
    const char* filePath = "tests/test.shn";
#endif


int main()
{
    sio << "Shannon " << SHANNON_VERSION_MAJOR << '.' << SHANNON_VERSION_MINOR << '.' << SHANNON_VERSION_FIX
        << " (int" << sizeof(integer) * 8 << ')'
        << ' ' << SHANNON_COPYRIGHT << endl << endl;

    int exitcode = 0;

    initRuntime();
    initTypeSys();
    initVm();

    {
        Context context;
        
        try
        {
            // context.options.setDebugOpts(false);
            // context.options.compileOnly = true;
            context.loadModule(filePath);
        }
        catch (exception& e)
        {
            serr << "Error: " << e.what() << endl;
            exitcode = 201;
        }

        if (exitcode == 0)
        {
            try
            {
                variant result = context.execute();
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
                serr << "Runtime error: " << e.what() << endl;
                exitcode = 104;
            }
        }
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

