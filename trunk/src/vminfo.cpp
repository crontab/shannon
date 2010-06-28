
#include "vm.h"


#define OP(o,a)  { #o, op##o, arg##a }


umemint ArgSizes[argMax] =
    {
      0, sizeof(Type*), sizeof(uchar), sizeof(integer), sizeof(str), 
      sizeof(uchar), sizeof(Definition*),
      sizeof(uchar), sizeof(char), sizeof(uchar),
      sizeof(jumpoffs), sizeof(integer), sizeof(str), sizeof(str) + sizeof(Type*),
      sizeof(Type*) + sizeof(State*),
    };


OpInfo opTable[] = 
{
    OP(End, None),
    OP(ConstExprErr, None),
    OP(Exit, None),

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
    OP(LeaSelfVar, SelfIdx),    // [self-idx:u8] +obj(0) +ptr
    OP(LoadStkVar, StkIdx),     // [stk-idx:s8] +var
    OP(LeaStkVar, StkIdx),      // [stk-idx:s8] +obj(0) +ptr
    // --- end undoable loaders
    OP(LoadMember, StateIdx),   // [stateobj-idx:u8] -stateobj +var
    OP(LeaMember, StateIdx),    // [stateobj-idx:u8] -stateobj +stateobj +ptr
    OP(Deref, None),            // -ref +var
    OP(LeaRef, None),           // -ref +ref +ptr
    // --- end designator loaders

    // --- 4. STORERS
    OP(InitSelfVar, SelfIdx),   // [self-idx:u8] -var
    OP(InitStkVar, StkIdx),     // [stk-idx:s8] -var
    // --- begin grounded storers
    OP(StoreSelfVar, SelfIdx),  // [self-idx:u8] -var
    OP(StoreStkVar, StkIdx),    // [stk-idx:s8] -var
    OP(StoreMember, StateIdx),  // [stateobj-idx:u8] -var -stateobj
    OP(StoreRef, None),         // -var -ref
    // --- end grounded storers

    // --- 5. DESIGNATOR OPS, MISC
    OP(MkSubrange, MkSubrange), // [Ordinal*, State*] -int -int +type  -- compile-time only
    OP(MkRef, None),            // -var +ref
    OP(NonEmpty, None),         // -var +bool
    OP(Pop, None),              // -var
    OP(PopPod, None),           // -int
    OP(Cast, Type),             // [Type*] -var +var
    OP(IsType, Type),           // [Type*] -var +bool

    // --- 6. STRINGS, VECTORS
    OP(ChrToStr, None),         // -int +str
    OP(ChrCat, None),           // -int -str +str
    OP(StrCat, None),           // -str -str +str
    OP(VarToVec, None),         // -var +vec
    OP(VarCat, None),           // -var -vec +vec
    OP(VecCat, None),           // -vec -vec +vec
    OP(StrLen, None),           // -str +int
    OP(VecLen, None),           // -str +int
    OP(StrElem, None),          // -idx -str +int
    OP(VecElem, None),          // -idx -vec +var
    OP(StoreStrElem, None),     // -char -int -ptr -obj
    OP(StoreVecElem, None),     // -var -int -ptr -obj

    // --- 7. SETS
    OP(ElemToSet, None),        // -var +set
    OP(SetAddElem, None),       // -var -set + set
    OP(ElemToByteSet, None),    // -int +set
    OP(RngToByteSet, None),     // -int -int +set
    OP(ByteSetAddElem, None),   // -int -set +set
    OP(ByteSetAddRng, None),    // -int -int -set +set
    OP(InSet, None),            // -set -var +bool
    OP(InByteSet, None),        // -set -int +bool
    OP(InBounds, Type),         // [Ordinal*] -int +bool
    OP(InRange, None),          // -int -int -int +bool

    // --- 8. DICTIONARIES
    OP(PairToDict, None),       // -var -var +dict
    OP(DictAddPair, None),      // -var -var -dict +dict
    OP(PairToByteDict, None),   // -var -int +vec
    OP(ByteDictAddPair, None),  // -var -int -vec +vec
    OP(DictElem, None),         // -var -dict +var
    OP(StoreDictElem, None),    // -var -var -ptr -obj
    OP(ByteDictElem, None),     // -int -dict +var
    OP(InDict, None),           // -dict -var +bool
    OP(InByteDict, None),       // -dict -int +bool

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
    // case label helpers
    OP(CaseOrd,  None),         // -int -int +int +bool
    OP(CaseRange, None),        // -int -int -int +int +bool
    OP(CaseStr, None),          // -str -str +str +bool
    OP(CaseVar, None),          // -var -var +var +bool

    // --- 11. JUMPS
    // Jumps; [dst] is a relative 16-bit offset
    OP(Jump, Jump16),           // [dst 16]
    OP(JumpFalse, Jump16),      // [dst 16] -bool
    OP(JumpTrue, Jump16),       // [dst 16] -bool
    // Short bool evaluation: pop if jump, leave it otherwise
    OP(JumpAnd, Jump16),        // [dst 16] (-)bool
    OP(JumpOr, Jump16),         // [dst 16] (-)bool

    // Misc. builtins
    OP(LineNum, LineNum),       // [linenum:int]
    OP(Assert, AssertCond),     // [cond:str] -bool
    OP(Dump, Dump),             // [expr:str, type:Type*] -var
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


#define ADV(T) \
    (ip += sizeof(T), *(T*)(ip - sizeof(T)))


static const char* varTypeStr(variant::Type type)
{
#define _C(t) case variant::t: return #t;
    switch(type)
    {
        _C(VOID)
        _C(ORD)
        _C(REAL)
        _C(VARPTR)
        _C(STR)
        _C(VEC)
        _C(SET)
        _C(ORDSET)
        _C(DICT)
        _C(REF)
        _C(RTOBJ)
    }
    return false;
}


void CodeSeg::dump(fifo& stm) const
{
    if (code.empty())
        return;
    const uchar* beginip = (const uchar*)code.data();
    const uchar* ip = beginip;
    while (1)
    {
        if (*ip >= opMaxCode)
            fatal(0x5101, "Corrupt code");
        const OpInfo& info = opTable[*ip];
        if (*ip == opLineNum)
        {
            ip++;
            stm << "#LINENUM " << ADV(integer);
        }
        else
        {
            stm << to_string(ip - beginip, 16, 4, '0') << ":\t";
            ip++;
            stm << info.name;
            if (info.arg != argNone)
            {
                stm << '\t';
                if (strlen(info.name) < 8)
                    stm << '\t';
            }
            switch (info.arg)
            {
                case argNone:       break;
                case argType:       ADV(Type*)->dumpDef(stm); break;
                case argUInt8:      stm << to_quoted(*ip); stm << " (" << int(ADV(uchar)) << ')'; break;
                case argInt:        stm << ADV(integer); break;
                case argStr:        stm << to_quoted(ADV(str)); break;
                case argVarType8:   stm << varTypeStr(variant::Type(ADV(uchar))); break;
                case argDefinition: stm << "const " << ADV(Definition*)->name; break;
                case argSelfIdx:    stm << "self." << state->selfVars[ADV(uchar)]->name; break;
                case argStkIdx:     stm << (*(char*)ip < 0 ? "arg." : "local."); stm << int(ADV(char)); break;
                case argStateIdx:   stm << "state." << int(ADV(uchar)); break;
                case argJump16:     stm << to_string(ip - beginip + ADV(jumpoffs), 16, 4, '0');
                case argLineNum:    break; // handled above
                case argAssertCond: stm << '"' << ADV(str) << '"'; break;
                case argDump:       stm << ADV(str) << ": "; ADV(Type*)->dumpDef(stm); break;
                case argMkSubrange: notimpl();
                case argMax:        break;
            }
        }
        stm << endl;
        if (info.op == opEnd)
            break;
    }
}

