

#include "common.h"
#include "runtime.h"
#include "source.h"
#include "typesys.h"
#include "vm.h"


// --- HIS MAJESTY, THE COMPILER ------------------------------------------- //


class Compiler
{
    Parser& parser;
    Context& context;
    CodeGen* codegen;
    bool started;
    bool successful;

public:
    Compiler(Parser& _parser, Context& _context);
    ~Compiler();
    
    void compile();
};


Compiler::Compiler(Parser& _parser, Context& _context)
  : parser(_parser), context(_context), codegen(NULL),
    started(false), successful(false)  { }

Compiler::~Compiler()  { }


void Compiler::compile()
{
    if (started)
        fatal(0x7001, "Compiler object can't be used more than once");
    started = true;

    Module* module = context.addModule("main");    // TODO: read from file
    CodeGen mainCodeGen(*module);
    codegen = &mainCodeGen;

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
        Context context;
        Compiler compiler(parser, context);
        compiler.compile();
        context.setReady();

        variant result = context.run();
        if (result.is_ordinal())
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
        exit(101);
    }
    doneTypeSys();

#ifdef DEBUG
    assert(object::alloc == 0);
#endif

    return 0;
}

