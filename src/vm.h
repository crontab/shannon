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
    opLoadMember,       // [self-idx:8] -stateobj +var

    // Storers
    opInitStkVar,       // [stk-idx:8] -var

    opPop,              // -var

    opChrToStr,         // -ord +str
    opVarToVec,         // -var +vec

    // Arithmetic binary: -ord, -ord, +ord
    opAdd,              // -int, +int, +int
    opSub,              // -int, +int, +int
    opMul,              // -int, +int, +int
    opDiv,              // -int, +int, +int
    opMod,              // -int, +int, +int
    opBitAnd,           // -int, +int, +int
    opBitOr,            // -int, +int, +int
    opBitXor,           // -int, +int, +int
    opBitShl,           // -int, +int, +int
    opBitShr,           // -int, +int, +int

    // Boolean operations are performed with short evaluation using jumps,
    // except NOT and XOR
    opBoolXor,          // -bool, -bool, +bool

    // Arithmetic unary: -ord, +ord
    opNeg,              // -int, +int
    opBitNot,           // -int, +int
    opNot,              // -bool, +bool

    opInv,
    opMaxCode = opInv,
};


inline bool isUndoableLoadOp(OpCode op)
    { return (op >= opLoadTypeRef && op <= opLoadConst); }

/*
inline bool isDesignatorLoadOp(OpCode op)
    { return op >= opLoadSelfVar && op <= opLoadStkVar; }

inline OpCode designatorLoadToStore(OpCode op)
    { return OpCode(op + opStoreSelfVar - opLoadSelfVar); }
*/


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

    template <class T>
        void add(const T& t)                        { codeseg.append<T>(t); }
    void addOp(OpCode op)                           { codeseg.append<uchar>(op); }
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
    void loadStoreVar(Variable*, bool);

public:
    CodeGen(CodeSeg&);
    ~CodeGen();
    
    memint getLocals()      { return locals; }
    State* getState()       { return codeOwner; }
    Type* getTopType()      { return stkTop(); }
    Type* tryUndoTypeRef();
    void deinitLocalVar(Variable*);
    void discard();
    void implicitCast(Type*, const char* errmsg = NULL);

    void loadTypeRef(Type*);
    void loadConst(Type* type, const variant&);
    void loadDefinition(Definition*);
    void loadEmptyCont(Container* type);
    void loadSymbol(ModuleVar*, Symbol*);
    void loadVariable(Variable*);
    void loadMember(Symbol*);
    void loadMember(Variable*);
    void storeRet(Type*);
//    Type* undoDesignatorLoad(str& loader);
//    void storeDesignator(str loaderCode, Type* type);
    void arithmBinary(OpCode op);
    void arithmUnary(OpCode op);
    void _not();
    void boolXor();
    void end();
    Type* runConstExpr(Type* expectType, variant& result); // defined in vm.cpp
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
    objvec<ModuleInst> modules;
    ModuleInst* queenBeeInst;

    ModuleInst* addModuleInst(ModuleInst*);
    ModuleInst* loadModule(const str& filePath);
    str lookupSource(const str& modName);
    ModuleInst* getModule(const str&); // for use by the compiler, "uses" clause
public:
    Context();
    ~Context();
    ModuleInst* findModuleDef(Module*);
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
