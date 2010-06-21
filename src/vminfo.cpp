
#include "vm.h"


#define OP(o,a)  { #o, op##o, arg##a }


umemint ArgSizes[argMax] =
    {
      0, sizeof(Type*), sizeof(uchar), sizeof(integer), sizeof(str), 
      sizeof(uchar), sizeof(Definition*),
      sizeof(uchar), sizeof(char), sizeof(uchar),
      sizeof(jumpoffs), sizeof(str) + sizeof(str) + sizeof(integer), sizeof(str) + sizeof(Type*),
    };


OpInfo opTable[] = 
{
    OP(End, None),              // end execution and return
    OP(Nop, None),              // not used currently
    OP(Exit, None),             // throws eexit()

    // --- 2. CONST LOADERS
    // sync with isUndoableLoadOp()
    OP(LoadTypeRef, Type),      // [Type*] +obj
    OP(LoadNull, None),         // +null
    OP(Load0, None),            // +int
    OP(Load1, None),            // +int
    OP(LoadByte, UInt8),        // [int:u8] +int
    OP(LoadOrd, Int),           // [int] +int
    OP(LoadStr, Str),           // [str] +str
    OP(LoadEmptyVar, VarType8), // [variant::Type:8] + var
    OP(LoadConst, Definition),  // [Definition*] +var

    // --- 3. DESIGNATOR LOADERS
    // sync with isDesignatorLoader()
    OP(LoadSelfVar, SelfIdx),   // [self-idx:u8] +var
    OP(LoadStkVar, StkIdx),     // [stk-idx:s8] +var
    // --- end undoable loaders
    OP(LoadMember, StateIdx),   // [stateobj-idx:u8] -stateobj +var
    OP(Deref, None),            // -ref +var
    OP(StrElem, None),          // -idx -str +int
    OP(VecElem, None),          // -idx -vec +var
    OP(DictElem, None),         // -var -dict +var
    OP(ByteDictElem, None),     // -int -dict +var
    // --- end designator loaders

    // --- 4. STORERS
    OP(InitSelfVar, SelfIdx),   // [self-idx:u8] -var
    OP(StoreSelfVar, SelfIdx),  // [self-idx:u8] -var
    OP(InitStkVar, StkIdx),     // [stk-idx:s8] -var
    OP(StoreStkVar, StkIdx),    // [stk-idx:s8] -var
    OP(StoreMember, StateIdx),  // [stateobj-idx:u8] -var -stateobj
    OP(StoreRef, None),         // -var -ref

    // --- 5. DESIGNATOR OPS, MISC
    OP(MkSubrange, Type),       // [Ordinal*] -int -int +type  -- compile-time only
    OP(MkRef, None),            // -var +ref
    OP(NonEmpty, None),         // -var +bool
    OP(Pop, None),              // -var

    // --- 6. STRINGS, VECTORS
    OP(ChrToStr, None),         // -int +str
    OP(ChrCat, None),           // -int -str +str
    OP(StrCat, None),           // -str -str +str
    OP(VarToVec, None),         // -var +vec
    OP(VarCat, None),           // -var -vec +vec
    OP(VecCat, None),           // -vec -vec +vec
    OP(StrLen, None),           // -str +int
    OP(VecLen, None),           // -str +int
    OP(ReplStrElem, None),      // -int -int -str +str
    OP(ReplVecElem, None),      // -var -int -vec +vec

    // --- 7. SETS
    OP(ElemToSet, None),        // -var +set
    OP(SetAddElem, None),       // -var -set + set
    OP(ElemToByteSet, None),    // -int +set
    OP(RngToByteSet, None),     // -int -int +set
    OP(ByteSetAddElem, None),   // -int -set +set
    OP(ByteSetAddRng, None),    // -int -int -set +set

    // --- 8. DICTIONARIES
    OP(PairToDict, None),       // -var -var +dict
    OP(DictAddPair, None),      // -var -var -dict +dict
    OP(PairToByteDict, None),   // -var -int +vec
    OP(ByteDictAddPair, None),  // -var -int -vec +vec

    // --- 9. ARITHMETIC
    OP(Add, None),              // -int, +int, +int
    OP(Sub, None),              // -int, +int, +int
    OP(Mul, None),              // -int, +int, +int
    OP(Div, None),              // -int, +int, +int
    OP(Mod, None),              // -int, +int, +int
    OP(BitAnd, None),           // -int, +int, +int
    OP(BitOr, None),            // -int, +int, +int
    OP(BitXor, None),           // -int, +int, +int
    OP(BitShl, None),           // -int, +int, +int
    OP(BitShr, None),           // -int, +int, +int

    // Arithmetic unary: -int, +int
    OP(Neg, None),              // -int, +int
    OP(BitNot, None),           // -int, +int
    OP(Not, None),              // -bool, +bool

    // --- 10. BOOLEAN
    OP(CmpOrd, None),           // -int, -int, +{-1,0,1}
    OP(CmpStr, None),           // -str, -str, +{-1,0,1}
    OP(CmpVar, None),           // -var, -var, +{0,1}
    // see isCmpOp()
    OP(Equal, None),            // -int, +bool
    OP(NotEq, None),            // -int, +bool
    OP(LessThan, None),         // -int, +bool
    OP(LessEq, None),           // -int, +bool
    OP(GreaterThan, None),      // -int, +bool
    OP(GreaterEq, None),        // -int, +bool

    // --- 11. JUMPS
    // Jumps; [dst] is a relative 16-bit offset
    OP(Jump, Jump16),           // [dst 16]
    OP(JumpFalse, Jump16),      // [dst 16] -bool
    OP(JumpTrue, Jump16),       // [dst 16] -bool
    // Short bool evaluation: pop if jump, leave it otherwise
    OP(JumpAnd, Jump16),        // [dst 16] (-)bool
    OP(JumpOr, Jump16),         // [dst 16] (-)bool

    // Misc. builtins
    // TODO: set filename and linenum in a separate op?
    OP(Assert, StrStrInt),      // [cond:str, fn:str, linenum:int] -bool
    OP(Dump, StrType),          // [expr:str, type:Type*] -var
    OP(Inv, None),              // not used
};


#ifdef DEBUG
static struct vmdebuginit
{
    vmdebuginit()
    {
        for (int i = 0; i <= opInv; i++)
        {
            if (opTable[i].op != i)
            {
                fprintf(stderr, "VMInfo table inconsistency");
                exit(201);
            }
        }
    }
} _vmdebuginit;
#endif


