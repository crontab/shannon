#ifndef __VM_H
#define __VM_H


#include "common.h"


DEF_EXCEPTION(EInvOpcode, "Invalid code")


// TODO: "unsafe" versions for operations that expect particular types
// TODO: implement safe typecasts from any type to any type (for opToXXX)

enum OpCode
{
    opInv,  // to detect corrupt code segments
    opEnd,
    opNop,
    
    // Arithmetic
    opAdd, opSub, opMul, opDiv, opMod, opBitAnd, opBitOr, opBitXor, opBitShl, opBitShr,
    opNeg, opBitNot, opNot,
    
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
    opLoadConst,        // [const-index: 8]
    opLoadTypeRef,      // [Type*]

    // Loaders: each of these can be replaced by a corresponding storer if
    // the object turns out to be a L-value.
    opLoadLocal,        // [stack-index: signed-8 (retval(-N), args(-N), temp(N))]
    opLoadThis,         // [var-index: 8]
    opLoadOuter,        // [level: 8, var-index: 8]
    opLoadStatic,       // [module: 8, var-index: 8]
    opLoadStrElem,      // pop index, pop str, push char
    opLoadTupleElem,    // pop index, pop tuple, push val
    opLoadDictElem,     // pop key, pop dict, push val
    opLoadMember,       // [var-index: 8] pop obj, push val

    // Storers
    opStoreLocal,       // [stack-index]
    opStoreThis,        // [var-index: 8]
    opStoreOuter,       // [level: 8, var-index: 8]
    opStoreStatic,      // [module: 8, var-index: 8]
    opStoreStrElem,     // pop char, pop index, pop str
    opStoreTupleElem,   // pop val, pop index, pop tuple
    opStoreDictElem,    // pop val, pop key, pop dict
    opStoreMember,      // [var-index: 8] pop val, pop obj

    // Vector/string concatenation
    opCatStrChar,       // pop char
    opCatStrs,          // pop str
    opCatTupleElem,     // pop elem
    opCatTuples,        // pop tuple
    
    // Range operations
    opMkBoolRange,      // stupid but accepted by the compiler
    opMkCharRange,      // pop right-char, pop left-char, push range
    opMkIntRange,       // pop right-int, pop left-int, push range
    opBoolInRange,      // pop range, pop bool, push {0,1}
    opCharInRange,      // pop range, pop int, push {0,1}
    opIntInRange,       // pop range, pop int, push {0,1}
    
    // Comparators
    opCmp,              // pop var, pop var, push {-1,0,1}
    opCmpNull,          // pop var, push {0,1}
    opCmpFalse,         // pop bool, push {0,1}
    opCmpTrue,          // pop bool, push {-1,0}
    opCmpCharStr,       // pop str, pop char, push {-1,0,1}
    opCmpStrChar,       // pop char, pop str, push {-1,0,1}
    opCmpInt0,          // pop int, push {-1,0,1}
    opCmpInt1,          // pop int, push {-1,0,1}
    opCmpNullStr,       // pop str, push {0,1}
    opCmpNullTuple,     // pop str, push {0,1}
    opCmpNullDict,      // pop str, push {0,1}
    opCmpNullOrdset,    // pop ordset, push {0,1}
    opCmpNullSet,       // pop set, push {0,1}
    opCmpNullFifo,      // pop str, push {0,1}

    // Compare the stack top with 0 and replace it with a bool value.
    // The order of these opcodes is in sync with tokEqual..tokNotEq
    opEqual, opLessThan, opLessEq, opGreaterEq, opGreaterThan, opNotEq,
    
    // Case labels: cmp the top element with the arg and leave 0 or 1 for
    // further boolean jump
    opCase,             // pop var, push {0,1}
    opCaseRange,        // pop int, push {0,1}
    
    // Safe typecasts
    opToBool,
    opToChar,
    opToInt,
    opToStr,
    opCharToStr,
    opElemToTuple,
    opToType,           // [Type*]
    
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
