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
    
    // opLoadZero,     // []                   +1
    // opLoadLargeZero,// []                   +1
    // opLoadOne
    // opLoadLargeOne
    opLoadInt,      // [int]                +1
    opLoadLarge,    // [large]            +2/1
    opLoadChar,     // [int]                +1
    opLoadFalse,    // []                   +1
    opLoadTrue,     // []                   +1
    opLoadNull,     // []                   +1
    opLoadNullStr,  // []                   +1
    opLoadStr,      // [str-data-ptr]       +1
    opLoadTypeRef,  // [ShType*]            +1
    
    opCmpInt,       // []               -2  +1
    opCmpIntLarge,  // []               -2  +1
    opCmpLarge,     // []               -2  +1
    opCmpLargeInt,  // []               -2  +1
    opCmpStr,       // []               -2  +1
    opCmpStrChr,    // []               -2  +1
    opCmpChrStr,    // []               -2  +1
    opCmpIntZero,   // []               -1  +1
    // opCmpLargeZero, // []               -1  +1
    // opCmpStrNull,   // []               -1  +1

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

    opAddInt,       // []               -2  +1
    opAddLarge,     // []               -2  +1
    opSubInt,       // []               -2  +1
    opSubLarge,     // []               -2  +1
    opMulInt,       // []               -2  +1
    opMulLarge,     // []               -2  +1
    opDivInt,       // []               -2  +1
    opDivLarge,     // []               -2  +1

    // unary
    opIntNeg,       // []               -1  +1
    opLargeNeg,     // []               -1  +1

    // TODO: linenum, rangecheck opcodes
    
    opInv = -1,
    
    opCmpFirst = opEQ
};


union VmQuant
{
    OpCode op_;
    int int_;
    ptr ptr_;
#ifdef PTR64
    large large_;
#endif
};


typedef twins<ShType*> ShTypePair;


class VmStack: public noncopyable
{
    PodStack<VmQuant> stack;

public:
    VmStack(): stack()        { }
    int size() const          { return stack.size(); }
    bool empty() const        { return stack.empty(); }
    void clear()              { return stack.clear(); }
    VmQuant& push()           { return stack.push(); }
    VmQuant  pop()            { return stack.pop(); }
    void  pushInt(int v)      { push().int_ = v; }
    void  pushPtr(ptr v)      { push().ptr_ = v; }
    int   popInt()            { return pop().int_; }
    ptr   popPtr()            { return pop().ptr_; }
    int   topInt() const      { return stack.top().int_; }
    ptr   topPtr() const      { return stack.top().ptr_; }
    int&  topIntRef()         { return stack.top().int_; }

#ifdef PTR64
    void   pushLarge(large v) { push().large_ = v; }
    large  popLarge()         { return pop().large_; }
    large& topLarge()         { return stack.top().large_; }
    large& topLargeRef()      { return stack.top().large_; }
#else
    void  pushLarge(large v)
        { push().int_ = int(v); push().int_ = int(v >> 32); }
    large popLarge()
        { int hi = popInt(); return largerec(popInt(), hi); }
    large topLarge()
        { return (large(stack.at(-1).int_) << 32) | unsigned(stack.at(-2).int_); }
#endif
};


class VmCode: public noncopyable
{
protected:
    struct EmulStackInfo: ShValue
    {
        int opIndex;
        bool isValue;
        EmulStackInfo(const ShValue& iValue, int iOpIndex);
        EmulStackInfo(ShType* iType, int iOpIndex);
    };

    PodArray<VmQuant> code;
    PodStack<EmulStackInfo> emulStack;
    ShScope* compilationScope;
    int lastOpIndex;

    void emulPush(const ShValue& v);
    void emulPush(ShType* v);
    ShType* emulPopType()           { return emulStack.pop().type; }
    EmulStackInfo& emulTop()        { return emulStack.top(); }
    void emulPop()                  { emulStack.pop(); }

    void genOp(OpCode o)            { code.add().op_ = o; }
    void genInt(int v)              { code.add().int_ = v; }
    void genPtr(ptr v)              { code.add().ptr_ = v; }
    void genCmpOp(OpCode op, OpCode cmp);
    int  updateLastOpIndex()
        { int t = lastOpIndex; lastOpIndex = code.size(); return t; }
    void verifyClean();

#ifdef PTR64
    void genLarge(large v)  { code.add().large_ = v; }
#else
    void genLarge(large v)  { genInt(int(v)); genInt(int(v >> 32)); }
#endif

    VM_STATIC void runtimeError(int code, const char*);
    VM_STATIC void run(VmQuant* p);

public:
    VmCode(ShScope* iCompilationScope);
    
    void genLoadConst(const ShValue&);
    void genLoadTypeRef(ShType*);
    void genMkSubrange();
    void genComparison(OpCode);
    void genStaticCast(ShType*);
    void endGeneration();
    
    ShValue runConstExpr();
    ShType* runTypeExpr();
    ShType* topType() const  { return emulStack.top().type; }
};


#ifdef SINGLE_THREADED

extern VmStack vmStack;

#endif


#endif
