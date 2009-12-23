#ifndef __VM_H
#define __VM_H

#include "common.h"
#include "runtime.h"
#include "typesys.h"


enum OpCode
{
    // NOTE: the order of many of these instructions in their groups is significant

    opEnd = 0,          // end execution and return
    opNop,
    opExit,             // throws eexit()

    // Const loaders
    opLoadTypeRef,      // [Type*] +obj
    opLoadNull,         // +null
    opLoad0,            // +ord
    opLoad1,            // +ord
    opLoadOrd8,         // [int8] +ord
    opLoadOrd,          // [int] +ord
    opLoadStr,          // [object*] +str
    opLoadEmptyVar,     // [variant::Type:8] + var
    opLoadConst,        // [Definition*] +var

    // Loaders
    opLoadSelfVar,      // [self-idx:8] +var
    opLoadStkVar,       // [stk-idx:8] +var

    // Storers
    opStoreSelfVar,     // [self-idx:8] -var
    opStoreStkVar,      // [stk-idx:8] -var

    opDeref,            // -ptr +var
    opPop,              // -var

    opChrToStr,         // -ord +str
    opVarToVec,         // -var +vec

    opInv,
    opMaxCode = opInv,
};


inline bool isUndoableLoadOp(OpCode op)
    { return (op >= opLoadTypeRef && op <= opLoadConst)
        || (op >= opLoadSelfVar && op <= opLoadStkVar); }

inline bool isDesignatorLoadOp(OpCode op)
    { return op >= opLoadSelfVar && op <= opLoadStkVar; }

inline OpCode designatorLoadToStore(OpCode op)
    { return OpCode(op + opStoreSelfVar - opLoadSelfVar); }


#define DEFAULT_STACK_SIZE 8192


class CodeSeg: public rtobject
{
    friend class CodeGen;
    typedef rtobject parent;

    str code;

protected:
    memint stackSize;

    // Code gen helpers
    template <class T>
        void append(const T& t)     { code.push_back<T>(t); }
    void append(OpCode op)          { code.push_back(char(op)); }
    void append(const str& s)       { code.append(s); }
    void resize(memint s)           { code.resize(s); }
    str  cutTail(memint start)
        { str t = code.substr(start); resize(start); return t; }
    template<class T>
        const T& at(memint i) const { return *code.data<T>(i); }
    template<class T>
        T& atw(memint i)            { return *code.atw<T>(i); }
    OpCode operator[] (memint i) const { return OpCode(code[i]); }

public:
    CodeSeg(State*) throw();
    ~CodeSeg() throw();

    State* getType() const          { return cast<State*>(parent::getType()); }
    memint size() const             { return code.size(); }
    memint getStackSize() const     { return stackSize; }
    bool empty() const;
    void close(memint s);

    // Return a NULL-terminated string ready to be run: NULL char is an opcode
    // to exit the function
    const char* getCode() const     { assert(stackSize >= 0); return code.data(); }
};


// --- Code Generator ------------------------------------------------------ //


class CodeGen: noncopyable
{
protected:
    State* codeOwner;
    CodeSeg& codeseg;

    struct SimStackItem
    {
        Type* type;
        memint offs;
        SimStackItem(Type* t, memint o)
            : type(t), offs(o)  { }
    };

    podvec<SimStackItem> simStack;  // exec simulation stack
    memint locals;                  // number of local vars allocated
    memint maxStack;                // total stack slots needed without function calls

    template <class T>
        void add(const T& t)                        { codeseg.append<T>(t); }
    void addOp(OpCode op)                           { codeseg.append(op); }
    void addOp(Type*, OpCode op);
    template <class T>
        void addOp(OpCode op, const T& a)           { addOp(op); add<T>(a); }
    template <class T>
        void addOp(Type* t, OpCode op, const T& a)  { addOp(t, op); add<T>(a); }
    Type* stkPop();
    void stkReplaceTop(Type* t);
    Type* stkTop()
        { return simStack.back().type; }
    memint stkTopOffs()
        { return simStack.back().offs; }
    Type* stkTop(memint i)
        { return simStack.back(i).type; }
    static void error(const char*);
    void undoLastLoad();
    void canAssign(Type* from, Type* to, const char* errmsg = NULL);
    bool tryImplicitCast(Type*);
    void implicitCast(Type*, const char* errmsg = NULL);

public:
    CodeGen(CodeSeg&);
    ~CodeGen();
    
    memint getLocals()      { return locals; }
    State* getState()       { return codeOwner; }
    void deinitLocalVar(Variable*);
    void discard();

     // NOTE: compound consts should be held by a smart pointer somewhere else
    void loadConst(Type* type, const variant&);
    void loadConst(Definition*);

    void loadEmptyCont(Container* type);
    void loadVariable(Variable*);
    void storeVariable(Variable*);
    void storeRet(Type*);
//    Type* undoDesignatorLoad(str& loader);
//    void storeDesignator(str loaderCode, Type* type);
    void end();
    void runConstExpr(Type* expectType, variant& result); // defined in vm.cpp
};


// --- Execution context --------------------------------------------------- //


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
    friend class Compiler;
protected:
    CompilerOptions options;
    objvec<ModuleDef> modules;
    QueenBeeDef* queenBeeDef;

    ModuleDef* addModuleDef(ModuleDef*);
    ModuleDef* loadModule(const str& filePath);
    str lookupSource(const str& modName);
    ModuleDef* getModule(const str&); // for use by the compiler, "uses" clause
public:
    Context();
    ~Context();
    ModuleDef* findModuleDef(Module*);
    variant execute(const str& filePath);
};


// The Virtual Machine. This routine is used for both evaluating const
// expressions at compile time and, obviously, running runtime code. It is
// reenterant and can be launched concurrently in one process so long as
// the arguments passed belong to one thread (except the code seggment which
// is read-only anyway).

void runRabbitRun(Context* context, stateobj* self, rtstack& stack, const char* code);


struct eexit: public ecmessage
    { eexit() throw(); ~eexit() throw(); };


#endif // __VM_H
