#ifndef __VM_H
#define __VM_H


#include "port.h"
#include "except.h"
#include "contain.h"
#include "baseobj.h"
#include "langobj.h"


#ifdef SINGLE_THREADED
#  define VM_STATIC static
#else
#  define VM_STATIC
#endif


enum OpCode
{
    opEnd,          // []
    opNop,          // []
    opStkFrame,     // [size]
    
    opLoadZero,     // []                   +1
    opLoadLargeZero,// []                   +1
    opLoadOne,      // []                   +1
    opLoadLargeOne, // []                   +1
    opLoadInt,      // [int]                +1
    opLoadLarge,    // [large]            +2/1
    opLoadFalse,    // []                   +1
    opLoadTrue,     // []                   +1
    opLoadNull,     // []                   +1
    opLoadNullVec,  // []                   +1
    opLoadVec,      // [str-data-ptr]       +1
    opLoadTypeRef,  // [ShType*]            +1
    
    opCmpInt,       // []               -2  +1
    opCmpLarge,     // []               -2  +1
    opCmpStr,       // []               -2  +1
    opCmpStrChr,    // []               -2  +1
    opCmpChrStr,    // []               -2  +1
    opCmpPodVec,    // []               -2  +1 - only EQ or NE
    // opCmpIntZero,   // []               -1  +1
    // opCmpLargeZero, // []               -1  +1
    // opCmpVecNull,   // []               -1  +1

    // TODO: opCmpInt0, opCmpLarge0, opStrIsNull

    // compare the stack top with 0 and replace it with a bool value
    opEQ,           // []               -1  +1
    opLT,           // []               -1  +1
    opLE,           // []               -1  +1
    opGE,           // []               -1  +1
    opGT,           // []               -1  +1
    opNE,           // []               -1  +1
    
    // typecasts
    opLargeToInt,   // []               -1  +1
    opIntToLarge,   // []               -1  +1

    // binary
    opMkSubrange,   // []               -2  +1

    opAdd,          // []               -2  +1
    opAddLarge,     // []               -2  +1
    opSub,          // []               -2  +1
    opSubLarge,     // []               -2  +1
    opMul,          // []               -2  +1
    opMulLarge,     // []               -2  +1
    opDiv,          // []               -2  +1
    opDivLarge,     // []               -2  +1
    opMod,          // []               -2  +1
    opModLarge,     // []               -2  +1
    opBitAnd,       // []               -2  +1
    opBitAndLarge,  // []               -2  +1
    opBitOr,        // []               -2  +1
    opBitOrLarge,   // []               -2  +1
    opBitXor,       // []               -2  +1
    opBitXorLarge,  // []               -2  +1
    opBitShl,       // []               -2  +1
    opBitShlLarge,  // []               -2  +1
    opBitShr,       // []               -2  +1
    opBitShrLarge,  // []               -2  +1

    // string/vector operators
    // [elem-size]: if < 4 int_ is taken from the stack, otherwise - large_
    opPodVecCat,    // []               -2  +1   vec + vec
    opPodVecElemCat,// [elem-size]      -2  +1   vec + elem
    opPodElemVecCat,// [elem-size]      -2  +1   elem + vec
    opPodElemElemCat,// [elem-size]     -2  +1   elem + elem
    opPodElemToVec, // [elem-size]      -1  +1

    // unary
    opNeg,          // []               -1  +1
    opNegLarge,     // []               -1  +1
    opBitNot,       // []               -1  +1
    opBitNotLarge,  // []               -1  +1
    opBoolNot,      // []               -1  +1
    
    // jumps
    //   short bool evaluation: pop if jump, leave it otherwise
    opJumpOr,       // [dst]            -1/0
    opJumpAnd,      // [dst]            -1/0

    // TODO: linenum, rangecheck opcodes

    opMaxCode,
    
    opCmpFirst = opEQ,

    opInv = -1,
};


inline bool isJump(OpCode op) { return op >= opJumpOr && op <= opJumpAnd; }


class VmStack: public noncopyable
{
    PodStack<VmQuant> stack;

public:
    VmStack(): stack()        { }
    int size() const          { return stack.size(); }
    bool empty() const        { return stack.empty(); }
    void clear()              { return stack.clear(); }
    void reserve(int size)    { stack.reserve(size); }
    VmQuant& push()           { return stack.pushr(); }
    VmQuant  pop()            { return stack.pop(); }
    void  pushInt(int v)      { push().int_ = v; }
    void  pushPtr(ptr v)      { push().ptr_ = v; }
    int   popInt()            { return pop().int_; }
    ptr   popPtr()            { return pop().ptr_; }
    int   topInt() const      { return stack.top().int_; }
    ptr   topPtr() const      { return stack.top().ptr_; }
    int*  topIntRef()         { return &stack.top().int_; }
    ptr*  topPtrRef()         { return &stack.top().ptr_; }

#ifdef PTR64
    void   pushLarge(large v) { push().large_ = v; }
    large  popLarge()         { return pop().large_; }
    large  topLarge() const   { return stack.top().large_; }
    large* topLargeRef()      { return &stack.top().large_; }
#else
    void  pushLarge(large v)
        { push().int_ = int(v); push().int_ = int(v >> 32); }
    large popLarge()
        { int hi = popInt(); return (large(hi) << 32) | unsigned(popInt()); }
    large topLarge() const
        { return (large(stack.at(-1).int_) << 32) | unsigned(stack.at(-2).int_); }
#endif
};


class VmCodeGen: public noncopyable
{
protected:
    struct GenStackInfo
    {
        ShType* type;
        GenStackInfo(ShType* iType): type(iType)  { }
    };

    VmCodeSegment seg;
    PodStack<GenStackInfo> genStack;
    int stackMax;
    
    void genPush(ShType* v);
    ShType* genPopType()                { return genStack.pop().type; }
    GenStackInfo& genTop()              { return genStack.top(); }
    void genPop()                       { genStack.pop(); }

    void genOp(OpCode op)               { seg.code.add().op_ = op; }
    void genInt(int v)                  { seg.code.add().int_ = v; }
    void genPtr(ptr v)                  { seg.code.add().ptr_ = v; }
    VmQuant* genCodeAt(int offs)        { return (VmQuant*)&seg.code[offs]; }

#ifdef PTR64
    void genLarge(large v)  { code.add().large_ = v; }
#else
    void genLarge(large v)  { genInt(int(v)); genInt(int(v >> 32)); }
#endif

    void genCmpOp(OpCode op, OpCode cmp);
    void genEnd();
    void verifyClean();

    VM_STATIC void runtimeError(int code, const char*);
    VM_STATIC void run(VmQuant* p);

public:
    VmCodeGen(ShType* iResultTypeHint = NULL);
    
    ShType* resultTypeHint;
    
    void genLoadIntConst(ShOrdinal*, int);
    void genLoadLargeConst(ShOrdinal*, large);
    void genLoadNull();
    void genLoadVecConst(ShType*, const char*);
    void genLoadTypeRef(ShType*);
    void genLoadConst(ShType*, podvalue);
    void genMkSubrange();
    void genComparison(OpCode);
    void genStaticCast(ShType*);
    void genBinArithm(OpCode op, ShInteger*);
    void genUnArithm(OpCode op, ShInteger*);
    void genVecCat();
    void genElemToVec(ShVector*);
    void genBoolXor()
            { genPop(); genOp(opBitXor); }
    void genBoolNot()
            { genOp(opBoolNot); }
    void genBitNot(ShInteger* type)
            { genOp(OpCode(opBitNot + type->isLargeInt())); }
    int  genForwardBoolJump(OpCode op);
    void genResolveJump(int jumpOffset);
    int  genOffset() const
            { return seg.code.size(); }
    ShType* genTopType()
            { return genTop().type; }
    
    void runConstExpr(ShValue& result);
    ShType* runTypeExpr();
    VmCodeSegment getCode();
};


#ifdef SINGLE_THREADED

extern VmStack stk;

#endif


#endif
