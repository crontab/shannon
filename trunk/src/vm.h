#ifndef __VM_H
#define __VM_H


#include "common.h"


DEF_EXCEPTION(EInvOpcode, "Invalid code")


// TODO: implement safe typecasts from any type to any type (for opToXXX)

enum OpCode
{
    opInv,  // to detect corrupt code segments
    opEnd,
    opNop,
    
    // Arithmetic
    opAdd, opSub, opMul, opDiv, opMod, opBitAnd, opBitOr, opBitXor, opBitShl, opBitShr,
    opNeg, opBitNot, opNot,
    
    // Vector/string concatenation
    opCharToStr,        // -char, +str
    opCharCat,          // -char, -str, +str
    opStrCat,           // -str, -str, +str
    opVarToVec,         // -var, +vec
    opVarCat,           // -var, -vec, +vec
    opVecCat,           // -var, -vec, +vec

    // Range operations (work for all ordinals)
    opMkRange,          // -right-int, -left-int, +range
    opInRange,          // -range, -int, +{0,1}

    // Safe typecasts
    opToBool,
    opToStr,
    opToType,           // [Type*]
    opDynCast,          // [State*]

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
    opLoadNullTuple,
    opLoadNullDict,
    opLoadNullOrdset,
    opLoadNullSet,
    opLoadNullFifo,
    opLoadConst,        // [const-index: 8] // compound values only
    opLoadConst2,       // [const-index: 16] // compound values only
    opLoadTypeRef,      // [Type*]

    // Loaders: each of these can be replaced by a corresponding storer if
    // the object turns out to be a L-value.
    opLoadLocal,        // [stack-index: signed-8 (retval(-N), args(-N), temp(N))]
    opLoadThis,         // [var-index: 8]
    opLoadOuter,        // [level: 8, var-index: 8]
    opLoadStatic,       // [module: 8, var-index: 8]
    opLoadStrElem,      // -index, -str, +char
    opLoadTupleElem,    // -index, -tuple, +val
    opLoadDictElem,     // -key, -dict, +val
    opLoadMember,       // [var-index: 8] -obj, +val

    // Storers
    // TODO: versions of storers where the destination object is left on the stack
    opStoreLocal,       // [stack-index]
    opStoreThis,        // [var-index: 8]
    opStoreOuter,       // [level: 8, var-index: 8]
    opStoreStatic,      // [module: 8, var-index: 8]
    opStoreStrElem,     // -char, -index, -str
    opStoreTupleElem,   // -val, -index, -tuple
    opStoreDictElem,    // -val, -key, -dict
    opStoreMember,      // [var-index: 8] -val, -obj

    // Comparators
    opCmp,              // -var, -var, +{-1,0,1}
    opCmpNull,          // -var, +{0,1}
    opCmpFalse,         // -bool, +{0,1}
    opCmpTrue,          // -bool, +{-1,0}
    opCmpCharStr,       // -str, -char, +{-1,0,1}
    opCmpStrChar,       // -char, -str, +{-1,0,1}
    opCmpInt0,          // -int, +{-1,0,1}
    opCmpInt1,          // -int, +{-1,0,1}
    opCmpNullStr,       // -str, +{0,1}
    opCmpNullTuple,     // -str, +{0,1}
    opCmpNullDict,      // -str, +{0,1}
    opCmpNullOrdset,    // -ordset, +{0,1}
    opCmpNullSet,       // -set, +{0,1}
    opCmpNullFifo,      // -str, +{0,1}

    // Compare the stack top with 0 and replace it with a bool value.
    // The order of these opcodes is in sync with tokEqual..tokNotEq
    opEqual, opLessThan, opLessEq, opGreaterEq, opGreaterThan, opNotEq,
    
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


#endif // __VM_H
