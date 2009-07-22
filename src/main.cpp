

#include "common.h"
#include "runtime.h"
#include "source.h"
#include "typesys.h"
#include "vm.h"


#include <stdlib.h>


// --- HIS MAJESTY, THE COMPILER ------------------------------------------- //


class Compiler: noncopyable
{
    Parser& parser;
    Module& mainModule;
    CodeGen* codegen;
    bool started;
    bool successful;

public:
    Compiler(Parser&, Module&);
    ~Compiler();
    void compile();
};


Compiler::Compiler(Parser& _parser, Module& _main)
  : parser(_parser), mainModule(_main), codegen(NULL),
    started(false), successful(false)  { }

Compiler::~Compiler()  { }


void Compiler::compile()
{
    if (started)
        fatal(0x7001, "Compiler object can't be used more than once");
    started = true;

//    Module* module = context.addModule("main");    // TODO: read the module name from the file
//    CodeGen mainCodeGen(module);
//    codegen = &mainCodeGen;

    successful = true;
}


// --- tests --------------------------------------------------------------- //


#define check(x) assert(x)


int main()
{
    int exitcode = 0;
    initTypeSys();
    try
    {
#ifdef XCODE
        const char* fn = "../../src/tests/test.shn";
#else
        const char* fn = "tests/test.shn";
#endif

        Parser parser(fn, new in_text(NULL, fn));
        Module module(remove_filename_path(remove_filename_ext(fn)));

        Compiler compiler(parser, module);
        compiler.compile();

        variant result = module.run();

        if (result.is_null())
            exitcode = 0;
        else if (result.is_ordinal())
            exitcode = result._ord();
        else if (result.is(variant::STR))
        {
            serr << result.as_str() << endl;
            exitcode = 102;
        }
        else
        {
            serr << result << endl;
            exitcode = 102;
        }

    }
    catch (std::exception& e)
    {
        serr << "Error: " << e.what() << endl;
        exitcode = 101;
    }
    doneTypeSys();

#ifdef DEBUG
    assert(object::alloc == 0);
#endif

    return exitcode;
}

