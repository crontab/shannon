#ifndef __COMPILER_H
#define __COMPILER_H

#include "parser.h"
#include "typesys.h"


class Compiler: protected Parser
{
    friend class Context;
protected:
    Context& context;
    Module& module;
    CodeGen* codegen;
    Scope* scope;           // for looking up symbols
    BlockScope* blockScope; // for local vars in nested blocks, can be NULL
    State* state;           // for this-vars, type objects and definitions

    void subrange();
    void enumeration();
    void identifier(const str&);
    void vectorCtor();
    void dictCtor();
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
    void expression();
    void expression(Type*);
    Type* getTypeDerivators(Type*);
    Type* getConstValue(Type* resultType, variant& result);
    Type* getTypeValue();
    Type* getTypeAndIdent(str& ident);
    void definition();
    void assertion();
    void dumpVar();
    void otherStatement();
    void statementList();

    void compileModule();

    Compiler(Context&, Module&, buffifo*);
    ~Compiler();
};


#endif // __COMPILER_H
