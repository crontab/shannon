
#include "vm.h"
#include "compiler.h"


Compiler::Compiler(Context& c, ModuleDef& mod, fifo* f) throw()
    : Parser(f), context(c), moduleDef(mod)  { }


Compiler::~Compiler() throw()
    { }


void Compiler::statementList()
{
    expect(tokBlockEnd, "'}'");
}


void Compiler::module()
{
    // The system module is always added implicitly
    moduleDef.module->addUses(queenBee);
    // Start parsing and code generation
    CodeGen mainCodeGen(*moduleDef.getCodeSeg());
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
    catch (EParser& e)
        { throw; }    // comes with file name and line no. already
    catch (exception& e)
        { error(e.what()); }

    mainCodeGen.end();
    moduleDef.setComplete();

//    if (options.vmListing)
//    {
//        outtext f(NULL, remove_filename_ext(getFileName()) + ".lst");
//        mainModule.listing(f);
//    }
}

