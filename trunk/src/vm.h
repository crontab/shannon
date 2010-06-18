#ifndef __VM_H
#define __VM_H

#include "common.h"
#include "runtime.h"
#include "typesys.h"


enum OpCode
{
    // NOTE: the relative order of many of these instructions in their groups is significant

    // --- 1. MISC CONTROL
    opEnd = 0,          // end execution and return
    opNop,
    opExit,             // throws eexit()

    // --- 2. CONST LOADERS
    // --- begin undoable loaders
    opLoadTypeRef,      // [Type*] +obj
    opLoadNull,         // +null
    opLoad0,            // +int
    opLoad1,            // +int
    opLoadByte,         // [int8] +int
    opLoadOrd,          // [int] +int
    opLoadStr,          // [object*] +str
    opLoadEmptyVar,     // [variant::Type:8] + var
    opLoadConst,        // [Definition*] +var

    // --- 3. LOADERS
    opLoadSelfVar,      // [self-idx:8] +var
    opLoadStkVar,       // [stk-idx:8] +var
    // --- end undoable loaders
    opLoadMember,       // [self-idx:8] -stateobj +var

    // --- 4. STORERS
    opInitStkVar,       // [stk-idx:8] -var

    // --- 5. DESIGNATOR OPS, MISC
    opMkRef,            // -var +ref
    opAutoDeref,        // -ref +var
    opDeref,            // -ref +var
    opNonEmpty,         // -var +bool
    opPop,              // -var

    // --- 6. STRINGS, VECTORS
    opChrToStr,         // -int +str
    opChrCat,           // -int -str +str
    opStrCat,           // -str -str +str
    opVarToVec,         // -var +vec
    opVarCat,           // -var -vec +vec
    opVecCat,           // -vec -vec +vec
    opStrElem,          // -idx -str +int
    opVecElem,          // -idx -vec +var
    opStrLen,           // -str +int
    opVecLen,           // -str +int

    // --- 7. SETS
    opElemToSet,        // -var +set
    opSetAddElem,       // -var -set + set
    opElemToByteSet,    // -int +set
    opRngToByteSet,     // -int -int +set
    opByteSetAddElem,   // -int -set +set
    opByteSetAddRng,    // -int -int -set +set

    // --- 8. DICTIONARIES
    opPairToDict,       // -var -var +dict
    opDictAddPair,      // -var -var -dict +dict
    opPairToByteDict,   // -var -int +vec
    opByteDictAddPair,  // -var -int -vec +vec
    opDictElem,         // -var -dict +var
    opByteDictElem,     // -int -dict +var

    // --- 9. ARITHMETIC
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
    // opBoolXor,          // -bool, -bool, +bool

    // Arithmetic unary: -int, +int
    opNeg,              // -int, +int
    opBitNot,           // -int, +int
    opNot,              // -bool, +bool

    // --- 10. BOOLEAN
    opCmpOrd,           // -int, -int, +{-1,0,1}
    opCmpStr,           // -str, -str, +{-1,0,1}
    opCmpVar,           // -var, -var, +{0,1}

    opEqual,            // -int, +bool
    opNotEq,            // -int, +bool
    opLessThan,         // -int, +bool
    opLessEq,           // -int, +bool
    opGreaterThan,      // -int, +bool
    opGreaterEq,        // -int, +bool

    // --- 11. JUMPS
    // Jumps; [dst] is a relative 16-bit offset.
    opJump,             // [dst 16]
    opJumpFalse,        // [dst 16] -bool
    opJumpTrue,         // [dst 16] -bool
    // Short bool evaluation: pop if jump, leave it otherwise
    opJumpAnd,          // [dst 16] (-)bool
    opJumpOr,           // [dst 16] (-)bool

    // Misc. builtins
    // TODO: set filename and linenum in a separate op
    opAssert,           // [cond:str, fn:str, linenum:int] -bool
    opDump,             // [expr:str, type:Type*] -var

    opInv,
    opMaxCode = opInv,
};


inline bool isUndoableLoadOp(OpCode op)
    { return (op >= opLoadTypeRef && op <= opLoadStkVar); }

inline bool isCmpOp(OpCode op)
    { return op >= opEqual && op <= opGreaterEq; }

inline bool isJump(OpCode op)
    { return op >= opJump && op <= opJumpOr; }

inline bool isBoolJump(OpCode op)
    { return op >= opJumpFalse && op <= opJumpOr; }


// --- Code Generator ------------------------------------------------------ //


class CodeGen: noncopyable
{
protected:
    State* codeOwner;
    State* typeReg;  // for calling registerType()
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
    bool isConstCode;

    template <class T>
        void add(const T& t)                        { codeseg.append<T>(t); }
    void addOp(OpCode op)                           { codeseg.append<uchar>(op); }
    void addOp(Type*, OpCode op);
    template <class T>
        void addOp(OpCode op, const T& a)           { addOp(op); add<T>(a); }
    template <class T>
        void addOp(Type* t, OpCode op, const T& a)  { addOp(t, op); add<T>(a); }
    void addJumpOffs(jumpoffs o)                    { add<jumpoffs>(o); }
    Type* stkPop();
    void stkReplaceTop(Type* t);
    Type* stkTop()
        { return simStack.back().type; }
    Type* stkTop(memint i)
        { return simStack.back(i).type; }
    const SimStackItem& stkTopItem()
        { return simStack.back(); }
    const SimStackItem& stkTopItem(memint i)
        { return simStack.back(i); }
    memint stkSize()
        { return simStack.size(); }
    static void error(const char*);
    static void error(const str&);
    void loadStoreVar(Variable*, bool);

public:
    CodeGen(CodeSeg&, State* treg);
    ~CodeGen();
    
    memint getLocals()      { return locals; }
    State* getState()       { return codeOwner; }
    Type* getTopType()      { return stkTop(); }
    memint beginDiscardable();
    void endDiscardable(memint offs);
    Type* tryUndoTypeRef();
    void deinitLocalVar(Variable*);
    void popValue();
    bool tryImplicitCast(Type*);
    void implicitCast(Type*, const char* errmsg = NULL);
    void explicitCast(Type*);
    void undoLastLoad();

    bool deref(bool autoDeref);
    void nonEmpty();
    void loadTypeRef(Type*);
    void loadConst(Type* type, const variant&);
    void loadDefinition(Definition*);
    void loadEmptyCont(Container* type);
    void loadSymbol(Variable*, Symbol*);
    void loadVariable(Variable*);
    void loadMember(const str& ident);
    void loadMember(Variable*);

    void storeRet(Type*);

    Container* elemToVec();
    void elemCat();
    void cat();
    void loadContainerElem();
    void length();
    void elemToSet();
    void rangeToSet();
    void setAddElem();
    void checkRangeLeft();
    void setAddRange();
    void pairToDict();
    void checkDictKey();
    void dictAddPair();

    void arithmBinary(OpCode op);
    void arithmUnary(OpCode op);
//    void boolXor();
    void cmp(OpCode);
    void _not(); // 'not' is something reserved, probably only with Apple's GCC

    memint boolJumpForward(OpCode op);
    memint jumpForward(OpCode op);
    void resolveJump(memint jumpOffs);
    void assertion(const str& cond, const str& file, integer line);
    void dumpVar(const str& expr);
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
    strvec modulePath;

    CompilerOptions();
    void setDebugOpts(bool);
};


class ModuleInstance: public Symbol
{
public:
    objptr<Module> module;
    objptr<stateobj> obj;
//    variant* self;
    ModuleInstance(Module* m);
    void run(Context*, rtstack&);
    void finalize();
};


class Context: protected Scope
{
protected:
    objvec<ModuleInstance> instances;
    ModuleInstance* queenBeeInst;
    dict<Module*, stateobj*> modObjMap;

    ModuleInstance* addModule(Module*);
    Module* loadModule(const str& filePath);
    str lookupSource(const str& modName);
    void instantiateModules();
    void clear();
    void dump(const str& listingPath);

public:
    CompilerOptions options;

    Context();
    ~Context();

    Module* getModule(const str& name);     // for use by the compiler, "uses" clause
    stateobj* getModuleObject(Module*);     // for initializing module vars in ModuleInstance::run()
    variant execute(const str& filePath);
};


// The Virtual Machine. This routine is used for both evaluating const
// expressions at compile time and, obviously, running runtime code. It is
// reenterant and can be launched concurrently in one process as long as
// the arguments are thread safe.

void runRabbitRun(variant* selfvars, rtstack& stack, const char* code);


struct eexit: public emessage
{
    eexit() throw();
    ~eexit() throw();
};


#endif // __VM_H
