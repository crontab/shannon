#ifndef __COMPILER_H
#define __COMPILER_H

#include "parser.h"
#include "typesys.h"


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


#endif // __COMPILER_H
