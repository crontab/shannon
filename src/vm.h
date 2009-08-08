#ifndef __VM_H
#define __VM_H


#include "common.h"
#include "runtime.h"
#include "typesys.h"

#include <stack>
#include <set>

// Implementation is in codegen.cpp and vm.cpp

enum OpCode
{
    // NOTE: the order of many of these instructions in their groups is significant!

    opInv,  // to detect corrupt code segments
    opEnd,  // also return from function
    opNop,
    opExit, // throws eexit()
    
    // Const loaders
    opLoadNull,         // +null
    opLoadFalse,        // +false
    opLoadTrue,         // +true
    opLoadChar,         // [8] +char
    opLoad0,            // +0
    opLoad1,            // +1
    opLoadInt,          // [int] +int
    opLoadNullRange,    // [Range*] +range
    opLoadNullDict,     // [Dict*] +dict
    opLoadNullStr,      // +str
    opLoadNullVec,      // [Vector*] +vec
    opLoadNullArray,    // [Array*] +array
    opLoadNullOrdset,   // [Ordset*] +ordset
    opLoadNullSet,      // [Set*] +set
    opLoadNullVarFifo,  // [Fifo*] +varfifo
    opLoadNullCharFifo, // [Fifo*] +charfifo
    opLoadNullComp,     // +null-obj
    opLoadConst,        // [const-index: 8] +var // compound values only
    opLoadConst2,       // [const-index: 16] +var // compound values only
    opLoadTypeRef,      // [Type*] +typeref

    opPop,              // -var
    opSwap,
    opDup,              // +var

    // Safe typecasts
    opToBool,           // -var, +bool
    opIntToStr,         // -int, +str
    opToString,         // [Type*] -var, +str
    opToType,           // [Type*] -var, +var
    opToTypeRef,        // -type, -var, +var
    opIsType,           // [Type*] -var, +bool
    opIsTypeRef,        // -type, -var, +bool

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

    // Range operations (work for all ordinals)
    opMkRange,          // [Ordinal*] -right-int, -left-int, +range
    opInRange,          // -range, -ord, +bool
    opInBounds,         // -ord, -ord, -ord, +bool

    // Comparators
    opCmpOrd,           // -ord, -ord, +{-1,0,1}
    opCmpStr,           // -str, -str, +{-1,0,1}
    opCmpVar,           // -var, -var, +{0,1}

    // Compare the stack top with 0 and replace it with a bool value.
    // The order of these opcodes is in sync with tokens tokEqual..tokGreaterEq
    opEqual,            // -int, +bool
    opNotEq,            // -int, +bool
    opLessThan,         // -int, +bool
    opLessEq,           // -int, +bool
    opGreaterThan,      // -int, +bool
    opGreaterEq,        // -int, +bool

    // Initializers
    opInitRet,          // [ret-index] -var
    // opInitLocal,        // [stack-index: 8]
    opInitThis,         // [this-index: 8]

    // Loaders
    // NOTE: opLoadRet through opLoadArg are in sync with Symbol::symbolId.
    // Also, opLoadRet..opLoadOuter are in sync with storers below: any of
    // these ops can be replaced with a storer counterpart if assignment is
    // encountered - see CodeGen::revertDesignatorOp() and storeDesignator().
    // The trick doesn't work for containers though.
    opLoadRet,          // [ret-index] +var
    opLoadLocal,        // [stack-index: 8] +var
    opLoadThis,         // [this-index: 8] +var
    opLoadArg,          // [stack-neg-index: 8] +var
    opLoadStatic,       // [Module*, var-index: 8] +var
    opLoadMember,       // [var-index: 8] -obj, +val
    opLoadOuter,        // [level: 8, var-index: 8] +var

    opLoadDictElem,     // -key, -dict, +val
    opLoadStrElem,      // -index, -str, +char
    opLoadVecElem,      // -index, -vector, +val
    opLoadArrayElem,    // -index, -array, +val

    // Storers
    opStoreRet,         // [ret-index] -var
    opStoreLocal,       // [stack-index: 8] -var
    opStoreThis,        // [this-index: 8] -var
    opStoreArg,         // [stack-neg-index: 8] -var
    opStoreStatic,      // [Module*, var-index: 8] -var
    opStoreMember,      // [var-index: 8] -val, -obj
    opStoreOuter,       // [level: 8, var-index: 8] -var

    opStoreDictElem,    // [bool pop] -val, -key, (-dict)
    opStoreStrElem,     // [bool pop] -char, -index, -str
    opStoreVecElem,     // [bool pop] -val, -index, (-vector)
    opStoreArrayElem,   // [bool pop] -val, -index, (-array)

    // Container operations
    opKeyInDict,        // -dict, -key, +bool
    opPairToDict,       // [Dict*] -val, -key, +dict
    opDelDictElem,      // -key, -dict
    opPairToArray,      // [Array*] -val, -idx, +array
    opInOrdset,         // -ordset, -ord, +bool
    opAddToOrdset,      // [bool pop] -ord, -ordset
    opElemToOrdset,     // [Ordset*] -ord, +ordset
    opRangeToOrdset,    // -range, +ordset
    opAddRangeToOrdset, // [bool pop] -range, +ordset
    opDelOrdsetElem,    // -key, -ordset
    opInSet,            // -set, -key, +bool
    opAddToSet,         // [bool pop] -key, -set
    opElemToSet,        // [Set*] -var, +set
    opDelSetElem,       // -key, -set

    // Concatenation
    opChrToStr,         // -char, +str
    opChrToStr2,        // swap, -char, +str, swap
    opCharCat,          // -char, -str, +str
    opStrCat,           // -str, -str, +str
    opVarToVec,         // [Vector*] -var, +vec
    opVarCat,           // -var, -vec, +vec
    opVecCat,           // -var, -vec, +vec

    // Misc. built-ins
    opEmpty,            // -var, +bool
    opStrLen,           // -str, +int
    opVecLen,           // -vec, +int
    opRangeDiff,        // -range, +int

    // Jumps; [dst] is a relative 16-bit offset.
    opJump,             // [dst 16]
    opJumpTrue,         // [dst 16] -bool
    opJumpFalse,        // [dst 16] -bool
    // Short bool evaluation: pop if jump, leave it otherwise
    opJumpOr,           // [dst 16] (-)bool
    opJumpAnd,          // [dst 16] (-)bool

    // Case labels
    // TODO: these ops can sometimes be used with simple conditions (if a == 1...)
    opCaseInt,          // [int], +bool
    opCaseRange,        // [int, int], +bool
    opCaseStr,          // -str, +bool
    opCaseTypeRef,      // -typeref, +bool

    // Function call
    opCall,             // [Type*]

    // Helpers
    opDump,             // [Type*] -var
    opEchoLn,
    opLineNum,          // [file-id: 16, line-num: 16]
    opAssert,           // -bool

    opMaxCode,

    // Special values
    opLoadBase = opLoadRet, opStoreBase = opStoreRet,
    opCmpFirst = opEqual, opCmpLast = opGreaterEq,
};


inline bool isUndoableLoadOp(OpCode op)
    { return (op >= opLoadNull && op <= opLoadTypeRef)
        || (op >= opLoadRet && op <= opLoadOuter); }

inline bool isDesignatorOp(OpCode op)
    { return op >= opLoadRet && op <= opLoadOuter; }

inline OpCode getStorer(OpCode op)
    { assert(isDesignatorOp(op)); return OpCode(op + (opStoreRet - opLoadRet)); }

inline bool isCmpOp(OpCode op)
    { return op >= opCmpFirst && op <= opCmpLast; }

inline bool isJump(OpCode op)
    { return op >= opJump && op <= opJumpAnd; }

inline bool isBoolJump(OpCode op)
    { return op >= opJumpTrue && op <= opJumpAnd; }


template<class T>
    inline T ADV(const uchar*& ip)
        { T t = *(T*)ip; ip += sizeof(T); return t; }


DEF_EXCEPTION(eexit, "exit called");


class ConstCode: public CodeSeg
{
public:
    ConstCode(): CodeSeg(NULL, NULL) { }
    void run(variant&);
};


// --- CODE GENERATOR ------------------------------------------------------ //

class BlockScope;

class CodeGen: noncopyable
{
    typedef std::map<str, mem> StringMap;
protected:

    CodeSeg* codeseg;
    State* state;
    mem lastOpOffs;
    StringMap stringMap;
    // TODO: replace this vector with a static array
    PtrList<Type> genStack;

    mem stkMax;
    mem locals;
#ifdef DEBUG
    mem stkSize;
#endif

    mem addOp(OpCode);
    mem addOp(const str&);
    void addOpPtr(OpCode, void*);
    void add8(uint8_t i);
    void add16(uint16_t i);
    void addJumpOffs(joffs_t i);
    void addInt(integer i);
    void addPtr(void* p);
    void revertLastLoad();
    str detachLastOp()
            { str t = codeseg->detach(lastOpOffs); lastOpOffs = mem(-1); return t; }
    OpCode lastOp();
    TypeReference* lastLoadedTypeRef();
    void close();
    void error(const char*);

    void stkPush(Type* t);
    Type* stkTop()
            { return genStack.top(); }
    Type* stkTop(mem i)
            { return genStack.top(i); }
    Type* stkPop();
    void stkReplace(Type* type)
            { genStack.top() = type; }
    void stkReplace(Type* type, mem i)
            { genStack.top(i) = type; }

    void loadConstById(mem id);
    mem  loadCompoundConst(Type*, const variant&, OpCode);
    void doStaticVar(ThisVar* var, OpCode);
    void loadStoreVar(Variable* var, bool load);
    void canAssign(Type* from, Type* to, const char* errmsg);
    bool tryImplicitCastTo(Type* to, bool under);

public:
    CodeGen(CodeSeg*);
    ~CodeGen();

    mem    getLocals()      { return locals; }
    State* getState()       { return state; }
    Type*  getTopType()     { return stkTop(); }
    mem    getStackSize()   { return genStack.size(); }
    void   justForget()     { stkPop(); }
    Type*  getLastTypeRef();

    void end();
    void endConstExpr(Type*);

    void exit();
    void loadNone();
    void loadBool(bool b);
    void loadChar(uchar c);
    void loadInt(integer i);
    void loadStr(const str& s);
    void loadTypeRef(Type*);
    void loadConst(Type*, const variant&);
    void loadDefinition(Definition* def)
            { loadConst(def->type, def->value); }
    void loadNullComp(Type* type);
    void loadSymbol(Symbol*);
    void discard();
    void swap();    // not used currently
    void dup();

    void initRetVal(Type*); // for const expressions
    void initVar(Variable*);
    void deinitLocalVar(Variable*);

    void loadVar(Variable*);
    void storeVar(Variable*);
    void loadMember(const str& ident);
    void loadContainerElem();
    void storeContainerElem(bool pop = true);
    Type* detachDesignatorOp(str&);
    void storeDesignator(str loaderCode, Type*);
    void delDictElem();
    void keyInDict();
    void pairToDict(Dict*);
    void pairToArray(Array*);
    void addToSet(bool pop);
    void delSetElem();
    void inSet();
    void elemToSet(Container* setType = NULL);
    void rangeToOrdset(Ordset*);
    void addRangeToOrdset(bool pop);

    void implicitCastTo(Type*, const char* errmsg = NULL);
    void implicitCastTo2(Type*, const char* errmsg = NULL);
    void explicitCastTo(Type*, const char* errmsg = NULL);
    void toBool();
    void dynamicCast();
    void testType(Type*);
    void testType();
    void arithmBinary(OpCode);
    void arithmUnary(OpCode);
    void _not();
    void boolXor();
    void elemToVec(Vec* vecType = NULL);
    void elemCat();
    void cat();
    void mkRange(Range*);
    void inRange();
    void inBounds();
    void cmp(OpCode op);

    void empty();
    void count();
    void lowHigh(bool high);

    mem  getCurPos()
            { return codeseg->size(); }
    void discardCode(mem from);
    void genPop()  // pop a value off the generator's stack
            { stkPop(); }
    void jump(mem target);
    mem  jumpForward(OpCode op = opJump);
    mem  boolJumpForward(OpCode);
    void resolveJump(mem jumpOffs);
    void nop()  // my favorite
            { addOp(opNop); }
    void caseLabel(Type*, const variant&);
    
    void dumpVar()
            { Type* type = stkPop(); addOpPtr(opDump, type); }
    void echoLn()
            { addOp(opEchoLn); }
    void assertion();
    void linenum(integer file, integer line);
};


class BlockScope: public Scope
{
protected:
    List<Variable> localvars;
    mem startId;
    CodeGen* gen;
public:
    BlockScope(Scope* outer, CodeGen*);
    ~BlockScope();
    Variable* addLocalVar(Type*, const str&);
    void deinitLocals();
};


#endif // __VM_H

