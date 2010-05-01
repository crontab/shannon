#ifndef __COMPILER_H
#define __COMPILER_H

#include "parser.h"
#include "typesys.h"


class Compiler: protected Parser
{
    friend class Context;
protected:
    Context& context;
    ModuleInst& moduleInst;
    CodeGen* codegen;
    Scope* scope;           // for looking up symbols
    BlockScope* blockScope; // for local vars in nested blocks, can be NULL
    State* state;           // for this-vars, type objects and definitions

    void enumeration(const str& firstIdent);
    void identifier(const str&);
    void atom();
    void designator();
    void factor();
    void term();
    void arithmExpr();
    void simpleExpr();
    void relation();
    void notLevel();
    void andLevel();
    void orLevel();
    void expression()
            { orLevel(); }
    Type* getTypeDerivators(Type*);
    Type* getConstValue(Type* resultType, variant& result);
    Type* getTypeValue();
    Type* getTypeAndIdent(str& ident);
    void definition();
    void assignment();
    void statementList();
    void module();

    Compiler(Context&, ModuleInst&, fifo*) throw();
    ~Compiler() throw();
};


#endif // __COMPILER_H
