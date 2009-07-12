

#include "runtime.h"
#include "source.h"
#include "symbols.h"
#include "typesys.h"


// TODO: reimplement with a simple dynamic buffer
class varstack: protected tuple_impl, public noncopyable
{
public:
    varstack() { }
    ~varstack() { }
    void push(const variant& v)     { push_back(v); }
    void pushn(mem n)               { resize(size() + n); }
    variant& top()                  { return back(); }
    variant& top(mem n)             { return *(end() - n); }
    void pop()                      { pop_back(); }
    void popn(mem n)                { resize(size() - n); }
};



// TODO: "unsafe" versions for operations that expect particular types
// TODO: implement safe typecasts from any type to any type (for opToXXX)

enum OpCode
{
    opInv,  // to detect corrupt code segments
    opEnd,
    opNop,
    
    // Load constants
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
    opLoadConst,        // [const-index: 16]
    opLoadTypeRef,      // [type-repo-index: 16]

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
    opCharToStr,        // pop char, push str
    opCatCharStr,       // pop char
    opVarToTuple,       // pop val, push tuple
    opCatElemTuple,     // pop elem
    
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
    
    // Safe variant typecasts
    opToBool,
    opToChar,
    opToInt,
    opToStr,
    opToType,           // [type-repo-index: 16]
    
    // Arithmetic
    opAdd, opSub, opMul, opDiv, opMod, opBitAnd, opBitOr, opBitXor, opBitShl, opBitShr,
    opNeg, opBitNot, opNot,
    
    // Jumps; [dst] is a relative offset
    //   short bool evaluation: pop if jump, leave it otherwise
    opJumpOr, opJumpAnd,
    opJumpTrue, opJumpFalse, opJump,

    // Function call
    opCall,             // [type-repo-index: 16]
    
    // Helpers
    opEcho, opEchoLn,
    opAssert,           // [line-num: 16]
    opLinenum,          // [line-num: 16]
    
    opMaxCode
};


// --- tests --------------------------------------------------------------- //

#define check(x) assert(x)


int main()
{
    initTypeSys();
    try
    {
        Parser parser("x", new in_text("x"));
        fout << "opcodes: " << opMaxCode << endl;
    }
    catch (std::exception& e)
    {
        ferr << "Exception: " << e.what() << endl;
    }
    doneTypeSys();
#ifdef DEBUG
    assert(object::alloc == 0);
#endif
}

