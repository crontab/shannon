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
    Scope* scope;           // for looking up symbols, can be local or state scope
    State* state;           // for this-vars, type objects and definitions

    // in compexpr.cpp
    Type* getTypeDerivators(Type*);
    void enumeration(const str& firstIdent);
    void identifier(const str&);
    void vectorCtor(Type* type);
    void dictCtor(Type* type);
    void typeOf();
    void ifFunc();
    void atom(Type*);
    void designator(Type*);
    void factor(Type*);
    void concatExpr(Container*);
    void term();
    void arithmExpr();
    void relation();
    void notLevel();
    void andLevel();
    void orLevel();
    void runtimeExpr(Type*);
    void constExpr(Type*);

    // in compiler.cpp
    Type* getConstValue(Type* resultType, variant& result, bool atomType);
    Type* getTypeValue(bool atomType);
    Type* getTypeAndIdent(str& ident);
    void definition();
    void variable();
    void assertion();
    void dumpVar();
    void programExit();
    void otherStatement();
    void block();
    void singleStatement();
    void statementList();

    void compileModule();

    Compiler(Context&, Module&, buffifo*);
    ~Compiler();
};


#endif // __COMPILER_H
