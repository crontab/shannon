

#include "common.h"
#include "runtime.h"
#include "parser.h"
#include "typesys.h"
#include "vm.h"


// --- HIS MAJESTY THE COMPILER -------------------------------------------- //


struct CompilerOptions
{
    bool enableDump;
    bool enableAssert;
    bool linenumInfo;
    bool vmListing;
    memint stackSize;
    vector<str> modulePath;

    CompilerOptions();
};


class Context: public Scope
{
protected:
    CompilerOptions options;
    objvec<ModuleDef> modules;
    ModuleDef* addModuleDef(ModuleDef*);
    ModuleDef* loadModule(const str& fileName);
    str lookupSource(const str& modName);
public:
    Context();
    ~Context();
    ModuleDef* getModule(const str&); // for use by the compiler, "uses" clause
    void execute(const str& fileName);
};


class Compiler: protected Parser
{
    friend class Context;
protected:
    Context& context;
    ModuleDef& moduleDef;

    CodeGen* codegen;
    Scope* scope;           // for looking up symbols
    BlockScope* blockScope; // for local vars in nested blocks, can be NULL
    State* state;           // for this-vars and type objects

    void statementList();

    void module();

    Compiler(Context&, ModuleDef&, fifo*) throw();
    ~Compiler() throw();
};


// --- Execution Context --------------------------------------------------- //


CompilerOptions::CompilerOptions()
  : enableDump(true), enableAssert(true), linenumInfo(true),
    vmListing(true), stackSize(8192)
        { modulePath.push_back("./"); }


static str moduleNameFromFileName(const str& n)
    { return remove_filename_path(remove_filename_ext(n)); }


Context::Context()
    : Scope(NULL), options(), modules()
        { addModuleDef(queenBeeDef); }


Context::~Context()
    { modules.release_all(); }


ModuleDef* Context::addModuleDef(ModuleDef* m)
{
    assert(m->getId() == modules.size());
    addUnique(m);
    modules.push_back(m->ref<ModuleDef>());
    return m;
}


ModuleDef* Context::loadModule(const str& fileName)
{
    str modName = moduleNameFromFileName(fileName);
    if (!isValidIdent(modName))
        throw emessage("Invalid module name: '" + modName + "'");
    ModuleDef* mod = addModuleDef(new ModuleDef(modName, modules.size()));
    Compiler compiler(*this, *mod, new intext(NULL, fileName));
    compiler.module();
    return mod;
}


str Context::lookupSource(const str& modName)
{
    for (memint i = 0; i < options.modulePath.size(); i++)
    {
        str t = options.modulePath[i] + "/" + modName + SOURCE_EXT;
        if (isFile(t.c_str()))
            return t;
    }
    throw emessage("Module not found: " + modName);
}


ModuleDef* Context::getModule(const str& modName)
{
    ModuleDef* m = cast<ModuleDef*>(Scope::find(modName));
    if (m == NULL)
        m = loadModule(lookupSource(modName));
    return m;
}


// --- Compiler ------------------------------------------------------------ //


Compiler::Compiler(Context& c, ModuleDef& mod, fifo* f) throw()
    : Parser(f), context(c), moduleDef(mod)  { }


Compiler::~Compiler() throw()
    { }


void Compiler::statementList()
{
}


void Compiler::module()
{
    CodeGen mainCodeGen(*moduleDef.codeseg);
    codegen = &mainCodeGen;
    blockScope = NULL;
    scope = state = moduleDef.getStateType();
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
//        exitcode = execute(fileName);
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

