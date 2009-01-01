#ifndef __VM_H
#define __VM_H


#include "str.h"
#include "contain.h"


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

    opFinThisPodVec,    // [offs]
    opFinThis,          // [ShType*, offs]  -1

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

    opFinLocPodVec,     // [local-offs]
    opFinLoc,           // [ShType*, local-offs]  -1
    
    opLoadThisRef,      // [offs]               +1
    opLoadLocRef,       // [offs]               +1

    // --------------------------------------------------------------------- //

    // vector concatenation
    opCopyToTmpVec,     // [temp-offs]
    opElemToVec,        // [elem-type] [temp-offs]  -1  +1
    opVecCat,           // [elem-type] [temp-offs]  -2  +1
    opVecElemCat,       // [elem-type] [temp-offs]  -2  +1

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

    // unary
    opNeg,              //                  -1  +1
    opNegLarge,         //                  -1  +1
    opBitNot,           //                  -1  +1
    opBitNotLarge,      //                  -1  +1
    opBoolNot,          //                  -1  +1
    
    // jumps; [dst] is a relative offset
    //   short bool evaluation: pop if jump, leave it otherwise
    opJumpOr,           // [dst]            -1/0
    opJumpAnd,          // [dst]            -1/0
    opJumpTrue,         // [dst]            -1
    opJumpFalse,        // [dst]            -1
    opJump,             // [dst]

    // helpers
    opEcho,             // [ShType*]        -1
    opEchoSp,           //
    opEchoLn,           //
    opAssert,           // [string* fn, linenum] -1
    opLinenum,          // [string* fn, linenum]

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


inline bool isJump(OpCode op) { return op >= opJumpOr && op <= opJump; }


typedef int offs;

union VmQuant
{
    OpCode op_;
    int   int_;
    ptr   ptr_;
    offs  offs_;    // offsets within datasegs or stack frames, negative for args
#ifdef PTR64
    large large_;   // since ptr's are 64-bit, we can fit 64-bit ints here, too
                    // otherwise large ints are moved around in 2 ops
#endif
};


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


#ifdef SINGLE_THREADED

extern VmStack stk;

#endif


class VmCodeSegment
{
protected:
    PodArray<VmQuant> code;

    static void run(VmQuant* codeseg, pchar dataseg, pchar stkbase, ptr retval);

public:
    offs reserveStack;
    offs reserveLocals;

    VmCodeSegment();

    int size() const       { return code.size(); }
    bool empty() const     { return code.empty(); }
    void clear()           { code.clear(); }
    VmQuant* getCode()     { return (VmQuant*)code.c_bytes(); }
    VmQuant* add()         { return &code.add(); }
    VmQuant* at(int i)     { return (VmQuant*)&code[i]; }

    void addOp(OpCode op)  { code.add().op_ = op; }
    void addInt(int v)     { code.add().int_ = v; }
    void addOffs(offs v)   { code.add().offs_ = v; }
    void addPtr(ptr v)     { code.add().ptr_ = v; }
#ifdef PTR64
    void addLarge(large v)  { code.add().large_ = v; }
#else
    void addLarge(large v)  { addInt(int(v)); addInt(int(v >> 32)); }
#endif


    offs reserveLocalVar(offs size)
        { offs t = reserveLocals; reserveLocals += size; return t; }
    void append(const VmCodeSegment& seg);
    
    void execute(pchar dataseg, ptr retval);
};


#endif

