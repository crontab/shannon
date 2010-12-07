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

        AutoScope(Compiler* c) throw();
        ~AutoScope() throw();
        StkVar* addInitStkVar(const str&, Type*);
    };

    struct ReturnInfo
    {
        Compiler& compiler;
        ReturnInfo* prev;
        bool topLevelReturned;
        podvec<memint> jumps;
        ReturnInfo(Compiler&) throw();
        ~ReturnInfo() throw();
        void resolveJumps();
    };

    struct LoopInfo
    {
        Compiler& compiler;
        LoopInfo* prev;
        memint stackLevel;
        memint continueTarget;
        podvec<memint> jumps;
        LoopInfo(Compiler& c) throw();
        ~LoopInfo() throw();
        void resolveJumps();
    };

public:
    Context& context;
    rtstack constStack;
    Module* const module;
    CodeGen* codegen;
    Scope* scope;           // for looking up symbols, can be local or state scope
    State* state;           // for this-vars, type objects and definitions
    LoopInfo* loopInfo;
    ReturnInfo* returnInfo;

    bool isLocalScope() const
        { return scope != state; }
    bool isStateScope() const
        { return scope == state; }
    bool isModuleScope() const
        { return scope == module; }

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
    void statementList();
    void ifBlock();
    void caseValue(Type*);
    void caseLabel(Type*);
    void switchBlock();
    void whileBlock();
    void forBlockTail(StkVar*, memint outJumpOffs, memint incJumpOffs = -1);
    void forBlock();
    void doContinue();
    void doBreak();
    void doReturn();
    void stateBody(State*);

    void compileModule();

    Compiler(Context&, Module*, buffifo*);
    ~Compiler();
};


#define LOCAL_ITERATOR_NAME "__iter"
#define LOCAL_INDEX_NAME "__idx"


#endif // __COMPILER_H
