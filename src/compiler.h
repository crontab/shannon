#ifndef __COMPILER_H
#define __COMPILER_H

#include "parser.h"
#include "typesys.h"


class Compiler: public Parser
{
    friend class Context;
    friend class AutoScope;

    struct AutoScope: public BlockScope
    {
        Compiler* compiler;

        AutoScope(Compiler* c);
        ~AutoScope();
        StkVar* addInitStkVar(const str&, Type*);
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

public:
    Context& context;
    rtstack constStack;
    Module* const module;
    CodeGen* codegen;
    Scope* scope;           // for looking up symbols, can be local or state scope
    State* state;           // for this-vars, type objects and definitions
    LoopInfo* loopInfo;

    // in compexpr.cpp
    Type* getStateDerivator(Type*, bool allowProto);
    Type* getTypeDerivators(Type*);
    Type* getEnumeration(const str& firstIdent);
    void builtin(Builtin*, bool skipFirst = false);
    void identifier(str);
    void dotIdentifier(str);
    void vectorCtor(Type* type);
    void fifoCtor(Type* type);
    void dictCtor(Type* type);
    void typeOf();
    void ifFunc();
    void actualArgs(FuncPtr*, bool skipFirst = false);
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
    Type* getConstValue(Type* resultType, variant& result);
    Type* getTypeValue();

    // in compiler.cpp
    Type* getTypeAndIdent(str* ident);
    void definition();
    void classDef();
    void variable();
    void assertion();
    void dumpVar();
    void programExit();
    void otherStatement();
    void doDel();
    void doIns();
    void singleOrMultiBlock();
    void nestedBlock();
    void singleStatement();
    void statementList(bool topLevel);
    void ifBlock();
    void caseValue(Type*);
    void caseLabel(Type*);
    void switchBlock();
    void whileBlock();
    void forBlockTail(StkVar*, memint outJumpOffs, memint incJumpOffs = -1);
    void forBlock();
    void doContinue();
    void doBreak();
    void stateBody(State*);

    void compileModule();

    Compiler(Context&, Module*, buffifo*);
    ~Compiler();
};


#define LOCAL_ITERATOR_NAME "__iter"
#define LOCAL_INDEX_NAME "__idx"


#endif // __COMPILER_H
