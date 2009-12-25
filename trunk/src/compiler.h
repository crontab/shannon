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
    State* state;           // for this-vars, type objects abd definitions

    void identifier(const str&);
    void atom();
    void designator();
    void factor();
    void term();
    void arithmExpr();
    void expression()
            { arithmExpr(); }
    void expression(Type* resultType, const char* errmsg = NULL);
    void subexpression();
    Type* getTypeDerivators(Type*);
    Type* getConstValue(Type* resultType, variant& result);
    Type* getTypeValue();
    Type* getTypeAndIdent(str& ident);
    void definition();
    void statementList();
    void module();

    Compiler(Context&, ModuleDef&, fifo*) throw();
    ~Compiler() throw();
};


#endif // __COMPILER_H
