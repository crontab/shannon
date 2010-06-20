#ifndef __VM_H
#define __VM_H

#include "common.h"
#include "runtime.h"
#include "typesys.h"


enum OpCode
{
    // NOTE: the relative order of many of these instructions in their groups is significant

    // --- 1. MISC CONTROL
    opEnd,              // end execution and return
    opNop,
    opExit,             // throws eexit()

    // --- 2. CONST LOADERS
    // --- begin undoable loaders, see isUndoableLoadOp()
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
    // loaders and their 'load effective address' variants
    // The LEA variants push two values: the base (object*) and an address
    // of a variant field inside that object. The first is needed to keep the 
    // object in memory. Storers check if the refcount reached 1 before 
    // assignment, which means the object is gone and a runtime exception  
    // should be thrown.
    // NOTE: LEA opcode variants should be at +1 from their ordinary load ops
    opLoadSelfVar,      // [self-idx:u8] +var
    opLEASelfVar,       // [self-idx:u8] +NULL +ptr
    opLoadStkVar,       // [stk-idx:s8] +var
    opLEAStkVar,        // [stk-idx:s8] +NULL +ptr
    // --- end undoable loaders
    opLoadMember,       // [self-idx:u8] -stateobj +var
    opLEAMember,        // [self-idx:u8] -stateobj +stateobj +ptr
    opDeref,            // -ref +var
    opLEARef,           // -ref +ref +ptr            ; opStoreRef

    // --- 4. STORERS
    opInitSelfVar,      // [var-idx:u8] -var
    opStoreSelfVar,     // [var-idx:u8] -var
    opInitStkVar,       // [stk-idx:s8] -var
    opStoreStkVar,      // [stk-idx:s8] -var
    opStoreMember,      // [self-idx:u8] -var -stateobj
    opStoreRef,         // -var -ref
    // opStoreEA,            // -var -ptr

    // --- 5. DESIGNATOR OPS, MISC
    opMkSubrange,       // [Ordinal*] -int -int +type  -- compile-time only
    opMkRef,            // -var +ref
    opNonEmpty,         // -var +bool
    opPop,              // -var

    // --- 6. STRINGS, VECTORS
    opChrToStr,         // -int +str
    opChrCat,           // -int -str +str
    opStrCat,           // -str -str +str
    opVarToVec,         // -var +vec
    opVarCat,           // -var -vec +vec
    opVecCat,           // -vec -vec +vec
    opStrLen,           // -str +int
    opVecLen,           // -str +int
    opStrElem,          // -idx -str +int
    opVecElem,          // -idx -vec +var
    opStoreStrElem,     // -int -int -ptr -obj
    opStoreVecElem,     // -var -int -ptr -obj

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

inline bool isAddressableOp(OpCode op)
    { return op == opLoadSelfVar || op == opLoadStkVar || op == opLoadMember
        || op == opDeref; }


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
    memint prevLoaderOffs;          // see beginAssignment()
    str storerCode;                 // see beginAssignment()

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
    void stkReplaceTop(Type* t);  // only if the opcode is not changed
    Type* stkTop()
        { return simStack.back().type; }
    Type* stkTop(memint i)
        { return simStack.back(i).type; }
    const SimStackItem& stkTopItem()
        { return simStack.back(); }
    const SimStackItem& stkTopItem(memint i)
        { return simStack.back(i); }
    static void error(const char*);
    static void error(const str&);

public:
    CodeGen(CodeSeg&, State* treg, bool compileTime);
    ~CodeGen();

    memint getStackLevel()      { return simStack.size(); }
    bool isCompileTime()        { return codeOwner == NULL; }
    memint getLocals()          { return locals; }
    State* getState()           { return codeOwner; }
    Type* getTopType()          { return stkTop(); }
    memint getCurrentOffs()     { return codeseg.size(); }
    memint beginDiscardable()   { return getCurrentOffs(); }
    void endDiscardable(memint offs);
    Type* tryUndoTypeRef();
    void deinitLocalVar(Variable*);
    void popValue();
    bool tryImplicitCast(Type*);
    void implicitCast(Type*, const char* errmsg = NULL);
    void explicitCast(Type*);
    void createSubrangeType();
    void undoLastLoad();

    bool deref();
    void mkref();
    void nonEmpty();
    void loadTypeRef(Type*);
    void loadConst(Type* type, const variant&);
    void loadDefinition(Definition*);
    void loadEmptyCont(Container* type);
    void loadSymbol(Symbol*);
    // These 3 functions return code offset of the last addressable load op
    // that can be converted to a LEA equivalent
    void loadVariable(Variable*);
    void loadMember(const str& ident);
    void loadMember(Variable*);

    void storeRet(Type*);
    void initLocalVar(LocalVar*);
    void initSelfVar(SelfVar*);
    void beginAssignment();
    void endAssignment();

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


class Context;

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

void runRabbitRun(stateobj* self, rtstack& stack, const char* code);


struct eexit: public emessage
{
    eexit() throw();
    ~eexit() throw();
};


void initVm();
void doneVm();


#endif // __VM_H
