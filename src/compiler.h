#ifndef __COMPILER_H
#define __COMPILER_H

#include "parser.h"
#include "typesys.h"


class Compiler: protected Parser
{
    friend class Context;
    friend class AutoScope;

    struct AutoScope: public BlockScope
    {
        Compiler& compiler;

        AutoScope(Compiler& c);
        ~AutoScope();
    };
    
    struct LoopInfo
    {
        Compiler& compiler;
        LoopInfo* prevLoopInfo;
        memint stackLevel;
        memint continueTarget;
        podvec<memint> breakJumps;

        LoopInfo(Compiler& c);
        ~LoopInfo();
        void resolveBreakJumps();
    };

protected:
    Context& context;
    Module& module;
    CodeGen* codegen;
    Scope* scope;           // for looking up symbols, can be local or state scope
    State* state;           // for this-vars, type objects and definitions
    LoopInfo* loopInfo;

    // in compexpr.cpp
    Type* getTypeDerivators(Type*);
    void enumeration(const str& firstIdent);
    void identifier(const str&);
    void vectorCtor(Type* type);
    void dictCtor(Type* type);
    void typeOf();
    void ifFunc();
    void actualArgs(Prototype*);
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
    void expression(Type*);
    void constExpr(Type*);
    Type* getConstValue(Type* resultType, variant& result, bool atomType);
    Type* getTypeValue(bool atomType);

    // in compiler.cpp
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
    void ifBlock();
    void caseValue(Type*);
    void caseLabel(Type*);
    void caseBlock();
    void whileBlock();
    void doContinue();
    void doBreak();
    void doDel();
    void stateBody(State*);

    void compileModule();

    Compiler(Context&, Module&, buffifo*);
    ~Compiler();
};


#endif // __COMPILER_H
