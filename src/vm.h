#ifndef __VM_H
#define __VM_H


#include "common.h"
#include "typesys.h"

#include <stack>


// TODO: implement safe typecasts from any type to any type (for opToXXX)

enum OpCode
{
    opInv,  // to detect corrupt code segments
    opEnd,
    opNop,
    
    // Const loaders
    opLoadNull,
    opLoadFalse,
    opLoadTrue,
    opLoadChar,         // [8]
    opLoad0,
    opLoad1,
    opLoadInt,          // [int]
    opLoadNullStr,
    opLoadNullRange,
    opLoadNullVec,
    opLoadNullDict,
    opLoadNullOrdset,
    opLoadNullSet,
    opLoadConst,        // [const-index: 8] // compound values only
    opLoadConst2,       // [const-index: 16] // compound values only
    opLoadTypeRef,      // [Type*]

    // Safe typecasts
    opToBool,
    opToStr,
    opToType,           // [Type*]
    opDynCast,          // [State*]

    // Arithmetic
    opAdd, opSub, opMul, opDiv, opMod, opBitAnd, opBitOr, opBitXor, opBitShl, opBitShr,
    opNeg, opBitNot, opNot,
    
    // Vector/string concatenation
    opCharToStr,        // -char, +str
    opCharCat,          // -char, -str, +str
    opStrCat,           // -str, -str, +str
    opVarToVec,         // [Vector*] -var, +vec
    opVarCat,           // [Vector*] -var, -vec, +vec
    opVecCat,           // -var, -vec, +vec

    // Range operations (work for all ordinals)
    opMkRange,          // [Ordinal*] -right-int, -left-int, +range
    opInRange,          // -range, -int, +{0,1}

    // Comparators
    opCmpOrd,           // -ord, -ord, +{-1,0,1}
    opCmpStr,           // -str, -str, +{-1,0,1}
    opCmpVar,           // -var, -var, +{0,1}

    // Compare the stack top with 0 and replace it with a bool value.
    // The order of these opcodes is in sync with tokens tokEqual..tokGreaterEq
    opEqual, opNotEq, opLessThan, opLessEq, opGreaterThan, opGreaterEq,
    
    // Loaders: each of these can be replaced by a corresponding storer if
    // the object turns out to be a L-value.
    opLoadLocal,        // [stack-index: signed-8 (retval(-N), args(-N), temp(N))]
    opLoadThis,         // [var-index: 8]
    opLoadOuter,        // [level: 8, var-index: 8]
    opLoadStatic,       // [module: 8, var-index: 8]
    opLoadStrElem,      // -index, -str, +char
    opLoadVecElem,      // -index, -vector, +val
    opLoadDictElem,     // -key, -dict, +val
    opLoadMember,       // [var-index: 8] -obj, +val

    // Storers
    // TODO: versions of storers where the destination object is left on the stack
    opStoreLocal,       // [stack-index]
    opStoreThis,        // [var-index: 8]
    opStoreOuter,       // [level: 8, var-index: 8]
    opStoreStatic,      // [module: 8, var-index: 8]
    opStoreStrElem,     // -char, -index, -str
    opStoreVecElem,     // -val, -index, -vector
    opStoreDictElem,    // -val, -key, -dict
    opStoreMember,      // [var-index: 8] -val, -obj

    // Case labels: cmp the top element with the arg and leave 0 or 1 for
    // further boolean jump
    opCase,             // -var, +{0,1}
    opCaseRange,        // -int, +{0,1}
    
    // Jumps; [dst] is a relative offset
    //   short bool evaluation: pop if jump, leave it otherwise
    opJumpOr, opJumpAnd,
    opJumpTrue, opJumpFalse, opJump,

    // Function call
    opCall,             // [Type*]

    // Helpers
    opEcho, opEchoLn,
    opAssert,           // [line-num: 16]
    opLinenum,          // [line-num: 16]
    
    opMaxCode
};


// --- CODE GENERATOR ------------------------------------------------------ //


class CodeGen: noncopyable
{
protected:

    struct stkinfo
    {
        Type* type;
        stkinfo(Type* t): type(t) { }
    };

    CodeSeg& codeseg;

    std::stack<stkinfo> genStack;
    mem stkMax;
#ifdef DEBUG
    mem stkSize;
#endif

    void stkPush(Type* t, const variant& v);
    void stkPush(Type* t)
            { stkPush(t, null); }
    void stkPush(Constant* c)
            { stkPush(c->type, c->value); }
    const stkinfo& stkTop() const;
    Type* stkTopType() const
            { return stkTop().type; }
    Type* stkPop();

    bool tryCast(Type*, Type*);
    
public:
    CodeGen(CodeSeg&);
    ~CodeGen();

    void endConstExpr(Type* expectType);
    void loadNone();
    void loadBool(bool b)
            { loadConst(queenBee->defBool, b); }
    void loadChar(uchar c)
            { loadConst(queenBee->defChar, c); }
    void loadInt(integer i)
            { loadConst(queenBee->defInt, i); }
    void loadConst(Type*, const variant&);
    void explicitCastTo(Type*);
    void implicitCastTo(Type*);
    void arithmBinary(OpCode);
    void arithmUnary(OpCode);
    void elemToVec();
    void elemCat();
    void cat();
    void mkRange();
    void inRange();
    void cmp(OpCode op);
};


#endif // __VM_H
