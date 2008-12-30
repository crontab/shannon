#ifndef __VM_H
#define __VM_H


#include "port.h"
#include "except.h"
#include "contain.h"
#include "baseobj.h"
#include "langobj.h"


enum OpCode
{
    opEnd,              //   
    opNop,              //   
    
    // copy the function return value
    opRetByte,          //                  -1
    opRetInt,           //                  -1
    opRetLarge,         //                  -1
    opRetPtr,           //                  -1
    opRetVec,           //                  -1
    opRetVoid,          //                  -1

    // const loaders
    opLoadZero,         //                      +1
    opLoadLargeZero,    //                      +1
    opLoadOne,          //                      +1
    opLoadLargeOne,     //                      +1
    opLoadIntConst,     // [int]                +1
    opLoadLargeConst,   // [large]              +2 (+1 for 64-bit env.)
    opLoadFalse,        //                      +1
    opLoadTrue,         //                      +1
    opLoadNull,         //                      +1
    opLoadNullVec,      //                      +1
    opLoadVecConst,     // [str-data-ptr]       +1
    opLoadTypeRef,      // [ShType*]            +1

    // --- VAR LOAD/STORE -------------------------------------------------- //

    // --- through dataseg
    // these are in sync with the StorageModel enum
    opLoadThisByte,     // [offs]               +1
    opLoadThisInt,      // [offs]               +1
    opLoadThisLarge,    // [offs]               +1
    opLoadThisPtr,      // [offs]               +1
    opLoadThisVec,      // [offs]               +1
    opLoadThisVoid,     // [offs]               +1
    
    opStoreThisByte,    // [offs]           -1
    opStoreThisInt,     // [offs]           -1
    opStoreThisLarge,   // [offs]           -1
    opStoreThisPtr,     // [offs]           -1
    opStoreThisVec,     // [offs]           -1
    opStoreThisVoid,    // [offs]           -1

    opInitThisVec,      // [offs]           -1
    opFinThisVec,       // [offs]

    // --- through stkbase
    opLoadLocByte,      // [local-offs]         +1
    opLoadLocInt,       // [local-offs]         +1
    opLoadLocLarge,     // [local-offs]         +1
    opLoadLocPtr,       // [local-offs]         +1
    opLoadLocVec,       // [local-offs]         +1
    opLoadLocVoid,      // [local-offs]         +1
    
    opStoreLocByte,     // [local-offs]     -1
    opStoreLocInt,      // [local-offs]     -1
    opStoreLocLarge,    // [local-offs]     -1
    opStoreLocPtr,      // [local-offs]     -1
    opStoreLocVec,      // [local-offs]     -1
    opStoreLocVoid,     // [local-offs]     -1

    opInitLocVec,       // [local-offs]     -1
    opFinLocVec,        // [local-offs]
    
    opLoadThisRef,      // [offs]               +1
    opLoadLocRef,       // [offs]               +1

    // --------------------------------------------------------------------- //

    // vector concatenation
    opCopyToTmpVec,     // [temp-offs]
    opElemToVec,        // [elem-size] [temp-offs]  -1  +1
    opVecCat,           // [temp-offs]              -2  +1
    opVecElemCat,       // [elem-size] [temp-offs]  -2  +1

    // comparison
    opCmpInt,           //                  -2  +1
    opCmpLarge,         //                  -2  +1
    opCmpStr,           //                  -2  +1
    opCmpStrChr,        //                  -2  +1
    opCmpChrStr,        //                  -2  +1
    opCmpPodVec,        //                  -2  +1 - only EQ or NE
    opCmpPtr,           //                  -2  +1 - only EQ or NE

    // TODO: opCmpInt0, opCmpLarge0, opStrIsNull

    // compare the stack top with 0 and replace it with a bool value;
    // the order of these opcodes is in sync with tokEqual..tokNotEq
    opEQ,               //                  -1  +1
    opLT,               //                  -1  +1
    opLE,               //                  -1  +1
    opGE,               //                  -1  +1
    opGT,               //                  -1  +1
    opNE,               //                  -1  +1
    
    // typecasts
    opLargeToInt,       //                  -1  +1
    opIntToLarge,       //                  -1  +1
    opIntToStr,         // [temp-offs]      -1  +1
    opLargeToStr,       // [temp-offs]      -1  +1

    // binary
    opMkSubrange,       //                  -2  +1

    opAdd,              //                  -2  +1
    opAddLarge,         //                  -2  +1
    opSub,              //                  -2  +1
    opSubLarge,         //                  -2  +1
    opMul,              //                  -2  +1
    opMulLarge,         //                  -2  +1
    opDiv,              //                  -2  +1
    opDivLarge,         //                  -2  +1
    opMod,              //                  -2  +1
    opModLarge,         //                  -2  +1
    opBitAnd,           //                  -2  +1
    opBitAndLarge,      //                  -2  +1
    opBitOr,            //                  -2  +1
    opBitOrLarge,       //                  -2  +1
    opBitXor,           //                  -2  +1
    opBitXorLarge,      //                  -2  +1
    opBitShl,           //                  -2  +1
    opBitShlLarge,      //                  -2  +1
    opBitShr,           //                  -2  +1
    opBitShrLarge,      //                  -2  +1

/*
    // string/vector operators
    // [elem-size]: if < 4 int_ is taken from the stack, otherwise - large_
    opPodVecCat,        //                  -2  +1   vec + vec
    opPodVecElemCat,    // [elem-size]      -2  +1   vec + elem
    opPodElemVecCat,    // [elem-size]      -2  +1   elem + vec
    opPodElemElemCat,   // [elem-size]      -2  +1   elem + elem
*/
    // unary
    opNeg,              //                  -1  +1
    opNegLarge,         //                  -1  +1
    opBitNot,           //                  -1  +1
    opBitNotLarge,      //                  -1  +1
    opBoolNot,          //                  -1  +1
    
    // jumps
    //   short bool evaluation: pop if jump, leave it otherwise
    opJumpOr,           // [dst]            -1/0
    opJumpAnd,          // [dst]            -1/0

    // helpers
    opEcho,             // [ShType*]        -1
    opEchoSp,           //
    opEchoLn,           //
    opAssert,           // [string* fn, linenum] -1

    // TODO: linenum, rangecheck opcodes

    opMaxCode,
    
    opCmpFirst = opEQ, opCmpLast = opNE,
    opRetFirst = opRetByte, opRetLast = opRetVoid,
    opLoadThisFirst = opLoadThisByte, opLoadThisLast = opLoadThisVoid,
    opStoreThisFirst = opStoreThisByte, opStoreThisLast = opStoreThisVoid,
    opLoadLocFirst = opLoadLocByte, opLoadLocLast = opLoadLocVoid,
    opStoreLocFirst = opStoreLocByte, opStoreLocLast = opStoreLocVoid,

    opInv = -1,
};


inline bool isJump(OpCode op) { return op >= opJumpOr && op <= opJumpAnd; }


class VmStack: public PodStack<VmQuant>
{
public:
    VmQuant& push()           { return PodStack<VmQuant>::pushr(); }
    void  pushInt(int v)      { push().int_ = v; }
    void  pushPtr(ptr v)      { push().ptr_ = v; }
    void  pushOffs(offs o)    { push().offs_ = o; }
    int   popInt()            { return pop().int_; }
    ptr   popPtr()            { return pop().ptr_; }
    offs  popOffs()           { return pop().offs_; }
    int   topInt() const      { return top().int_; }
    ptr   topPtr() const      { return top().ptr_; }
    offs  topOffs() const     { return top().offs_; }
    int*  topIntRef()         { return &top().int_; }
    ptr*  topPtrRef()         { return &top().ptr_; }
    offs* topOffsRef()        { return &top().offs_; }

#ifdef PTR64
    void   pushLarge(large v) { push().large_ = v; }
    large  popLarge()         { return pop().large_; }
    large  topLarge() const   { return top().large_; }
    large* topLargeRef()      { return &top().large_; }
#else
    void  pushLarge(large v)
        { push().int_ = int(v); push().int_ = int(v >> 32); }
    void  pushLarge(int lo, int hi)
        { push().int_ = lo; push().int_ = hi; }
    large popLarge()
        { int hi = popInt(); return (large(hi) << 32) | unsigned(popInt()); }
    large topLarge() const
        { return (large(at(-1).int_) << 32) | unsigned(at(-2).int_); }
#endif
};


class VmCodeGen: public noncopyable
{
protected:
    struct GenStackInfo
    {
        ShType* type;
        int codeOffs;
        GenStackInfo(ShType* iType, int iCodeOffs)
            : type(iType), codeOffs(iCodeOffs)  { }
    };

    VmCodeSegment codeseg;
    VmCodeSegment finseg;
    PodStack<GenStackInfo> genStack;
    int genStackSize;
    bool needsRuntimeContext;
    ShVariable* deferredVar;
    
    void genPush(ShType* v);
    const GenStackInfo& genTop()        { return genStack.top(); }
    const GenStackInfo& genPop();
    ShVariable* genPopDeferred();
    ShType* genPopType()                { return genPop().type; }

    void genOp(OpCode op)               { codeseg.add()->op_ = op; }
    void genInt(int v)                  { codeseg.add()->int_ = v; }
    void genOffs(offs v)                { codeseg.add()->offs_ = v; }
    void genPtr(ptr v)                  { codeseg.add()->ptr_ = v; }

#ifdef PTR64
    void genLarge(large v)  { codeseg.add()->large_ = v; }
#else
    void genLarge(large v)  { genInt(int(v)); genInt(int(v >> 32)); }
#endif

    void genNop()           { genOp(opNop); }
    void genCmpOp(OpCode op, OpCode cmp);
    void genEnd();
    void runFinCode();
    void verifyClean();

public:
    VmCodeGen();
    
    void clear();
    
    ShType* resultTypeHint; // used by the parser for vector/array constructors

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
    void genBoolXor()
            { genPop(); genOp(opBitXor); }
    void genBoolNot()
            { genOp(opBoolNot); }
    void genBitNot(ShInteger* type)
            { genOp(OpCode(opBitNot + type->isLargeInt())); }
    offs genElemToVec(ShVector*);
    int  genForwardBoolJump(OpCode op);
    void genResolveJump(int jumpOffset);
    void genLoadVar(ShVariable*);
    void genLoadVarRef(ShVariable*);
    void genStore();
    void genInitVar(ShVariable*);
    void genFinVar(ShVariable*);
    offs genCopyToTempVec();
    void genVecCat(offs tempVar);
    void genVecElemCat(offs tempVar);
    void genIntToStr();
    void genEcho()
            { ShType* type = genPopType(); genOp(opEcho); genPtr(type); }
    void genAssert(Parser& parser);
    void genOther(OpCode op)
            { genOp(op); }
    void genReturn();

    int  genOffset() const
            { return codeseg.size(); }
    ShType* genTopType()
            { return genTop().type; }
    offs genReserveTempVar(ShType*);

    void runConstExpr(ShValue& result);
    ShType* runTypeExpr(ShValue& value, bool anyObj);
    void genFinalizeTemps();
    VmCodeSegment getCodeSeg();
};


#ifdef SINGLE_THREADED

extern VmStack stk;

#endif


#endif
