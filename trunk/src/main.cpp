

#include "common.h"
#include "runtime.h"
#include "parser.h"
#include "typesys.h"
#include "vm.h"


// --- HIS MAJESTY THE COMPILER -------------------------------------------- //

/*
struct CompilerOptions
{
    bool enableDump;
    bool enableAssert;
    bool linenumInfo;
    bool vmListing;

    CompilerOptions()
      : enableDump(true), enableAssert(true), linenumInfo(true),
        vmListing(true)  { }
};


class Compiler: protected Parser
{
    CompilerOptions options;

    objptr<Module> module;
    objptr<CodeSeg> codeseg;

    CodeGen* codegen;
    Scope* scope;           // for looking up symbols
    BlockScope* blockScope; // for local vars in nested blocks, can be NULL
    State* state;           // for this-vars and type objects

    void statementList();

public:
    Compiler(const str&, fifo*);
    ~Compiler();

    void compile();
    Module* getModule() const;
    CodeSeg* getCodeSeg() const;
};


Compiler::Compiler(const str& modName, fifo* f)
    : Parser(f), module(new Module(modName)), codeseg(new MainCodeSeg(module))
       { }

Compiler::~Compiler()
    { }


void Compiler::statementList()
{
}


void Compiler::compile()
{
    CodeGen mainCodeGen(codeseg);
    codegen = &mainCodeGen;
    scope = module;
    blockScope = NULL;
    state = module;
    try
    {
        next();
        statementList();
        expect(tokEof, "End of file");
    }
    catch (EDuplicate& e)
        { error("'" + e.ident + "' is already defined within this scope"); }
    catch (EUnknownIdent& e)
        { error("'" + e.ident + "' is unknown in this context"); }
    catch (exception& e)
        { error(e.what()); }

    mainCodeGen.end();

//    if (options.vmListing)
//    {
//        outtext f(NULL, remove_filename_ext(getFileName()) + ".lst");
//        mainModule.listing(f);
//    }
}


// --- Execute program ----------------------------------------------------- //


static str moduleNameFromFileName(const str& n)
    { return remove_filename_path(remove_filename_ext(n)); }


int executeFile(const str& fn)
{
    objptr<Module> module;
    objptr<CodeSeg> codeseg;

    {
        str mn = moduleNameFromFileName(fn);
        if (!isValidIdent(mn))
            throw emessage("Invalid module name: '" + mn + "'");
        Compiler compiler(mn, new intext(queenBee->defCharFifo, fn));
        compiler.compile();
        module = compiler.getModule();
        codeseg = compiler.getCodeSeg();
    }

    return 0; 
}

*/
// --- tests --------------------------------------------------------------- //


#include "typesys.h"


void ut_fail(unsigned line, const char* e)
{
    fprintf(stderr, "%s:%u: test failed `%s'\n", __FILE__, line, e);
    exit(200);
}

#define fail(e)     ut_fail(__LINE__, e)
#define check(e)    { if (!(e)) fail(#e); }

#define check_throw(a) \
    { bool chk_throw = false; try { a; } catch(exception&) { chk_throw = true; } check(chk_throw); }


#ifdef XCODE
    const char* fileName = "../../src/tests/test.shn";
#else
    const char* fileName = "tests/test.shn";
#endif

int main()
{
    int exitcode = 0;
    initTypeSys();
    try
    {
//        exitcode = executeFile(fileName);
    }
    catch (exception& e)
    {
        serr << "Error: " << e.what() << endl;
        exitcode = 101;
    }
    doneTypeSys();

    if (object::allocated != 0)
    {
        fprintf(stderr, "object::allocated: %d\n", object::allocated);
        _fatal(0xff01);
    }

    return exitcode;
}

