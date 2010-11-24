
#include "vm.h"


#define OP(o,a)  { #o, op##o, arg##a }


umemint ArgSizes[argMax] =
    {
      0, sizeof(Type*), sizeof(State*), sizeof(uchar) + sizeof(State*), sizeof(uchar), sizeof(integer), sizeof(str), 
      sizeof(uchar), sizeof(Definition*),
      sizeof(uchar), sizeof(uchar), sizeof(uchar), sizeof(uchar), sizeof(uchar),
      sizeof(jumpoffs), sizeof(integer),
      sizeof(State*) + sizeof(integer) + sizeof(str), // argAssert
      sizeof(str) + sizeof(Type*), // argDump
    };


OpInfo opTable[] = 
{
    OP(End, None),              //
    OP(ConstExprErr, None),     //
    OP(Exit, None),             //
    OP(EnterFunc, State),       // [State*]
    OP(LeaveFunc, State),       // [State*]
    OP(EnterCtor, State),       // [State*]

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
    OP(LoadOuterObj, None),     // +stateobj
    OP(LoadDataSeg, None),      // +module-obj
    OP(LoadOuterFuncPtr, State),// [State*] +funcptr
    OP(LoadInnerFuncPtr, State),// [State*] +funcptr
    OP(LoadFarFuncPtr, FarState),   // [datasegidx:u8, State*] +funcptr
    OP(LoadNullFuncPtr, State), // [State*] +funcptr -- used in const expressions

    // --- 3. DESIGNATOR LOADERS
    OP(LoadInnerVar, InnerIdx), // [inner.idx:u8] +var
    OP(LoadOuterVar, OuterIdx), // [outer.idx:u8] +var
    OP(LoadStkVar, StkIdx),     // [stk.idx:u8] +var
    OP(LoadArgVar, ArgIdx),     // [arg.idx:u8] +var
    // --- end undoable loaders
    OP(LoadMember, StateIdx),   // [stateobj.idx:u8] -stateobj +var
    OP(Deref, None),            // -ref +var

    OP(LeaInnerVar, InnerIdx),  // [inner.idx:u8] +obj(0) +ptr
    OP(LeaOuterVar, OuterIdx),  // [outer.idx:u8] +obj(0) +ptr
    OP(LeaStkVar, StkIdx),      // [stk.idx:u8] +obj(0) +ptr
    OP(LeaArgVar, ArgIdx),      // [arg.idx:u8] +obj(0) +ptr
    OP(LeaMember, StateIdx),    // [stateobj.idx:u8] -stateobj +stateobj +ptr
    OP(LeaRef, None),           // -ref +ref +ptr

    // --- 4. STORERS
    OP(InitInnerVar, InnerIdx), // [inner.idx:u8] -var
    OP(InitStkVar, StkIdx),     // [stk.idx:u8] -var
    // --- begin grounded storers
    OP(StoreInnerVar, InnerIdx),// [inner.idx:u8] -var
    OP(StoreOuterVar, OuterIdx),// [outer.idx:u8] -var
    OP(StoreStkVar, StkIdx),    // [stk.idx:u8] -var
    OP(StoreArgVar, ArgIdx),    // [arg.idx:u8] -var
    OP(StoreMember, StateIdx),  // [stateobj.idx:u8] -var -stateobj
    OP(StoreRef, None),         // -var -ref
    // --- end grounded storers
    OP(IncStkVar, StkIdx),      // [stk.idx:u8]

    // --- 5. DESIGNATOR OPS, MISC
    OP(MkRange, None),          // -int -int +range
    OP(MkRef, None),            // -var +ref
    OP(MkFuncPtr, State),       // [State*] -obj +funcptr
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
    OP(StrElem, None),          // -int -str +int
    OP(VecElem, None),          // -int -vec +var
    OP(Substr, None),           // -{int,void} -int -str +str
    OP(Subvec, None),           // -{int,void} -int -vec +vec
    OP(StoreStrElem, None),     // -char -int -ptr -obj
    OP(StoreVecElem, None),     // -var -int -ptr -obj
    OP(DelStrElem, None),       // -int -ptr -obj
    OP(DelVecElem, None),       // -int -ptr -obj
    OP(DelSubstr, None),        // -{int,void} -int -ptr -obj
    OP(DelSubvec, None),        // -{int,void} -int -ptr -obj
    OP(StrIns, None),           // -char -int -ptr -obj
    OP(VecIns, None),           // -var -int -ptr -obj
    OP(SubstrIns, None),        // -str -void -int -ptr -obj
    OP(SubvecIns, None),        // -vec -void -int -ptr -obj
    OP(ChrCatAssign, None),     // -char -ptr -obj
    OP(StrCatAssign, None),     // -str -ptr -obj
    OP(VarCatAssign, None),     // -var -ptr -obj
    OP(VecCatAssign, None),     // -vec -ptr -obj

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
    OP(InRange, None),          // -range -int +bool
    OP(InRange2, None),         // -int -int -int +bool
    OP(SetElem, None),          // -var -set +void
    OP(ByteSetElem, None),      // -int -set +void
    OP(DelSetElem, None),       // -var -ptr -obj
    OP(DelByteSetElem, None),   // -int -ptr -obj
    OP(SetLen, None),           // -set +int
    OP(SetKey, None),           // -int -set +var

    // --- 8. DICTIONARIES
    OP(PairToDict, None),       // -var -var +dict
    OP(DictAddPair, None),      // -var -var -dict +dict
    OP(PairToByteDict, None),   // -var -int +vec
    OP(ByteDictAddPair, None),  // -var -int -vec +vec
    OP(DictElem, None),         // -var -dict +var
    OP(ByteDictElem, None),     // -int -dict +var
    OP(InDict, None),           // -dict -var +bool
    OP(InByteDict, None),       // -dict -int +bool
    OP(StoreDictElem, None),    // -var -var -ptr -obj
    OP(StoreByteDictElem, None),// -var -int -ptr -obj
    OP(DelDictElem, None),      // -var -ptr -obj
    OP(DelByteDictElem, None),  // -int -ptr -obj
    OP(DictLen, None),          // -dict +int
    OP(DictElemByIdx, None),    // -int -dict +var
    OP(DictKeyByIdx, None),     // -int -dict +var

    // --- 9. ARITHMETIC
    OP(Add, None),              // -int -int +int
    OP(Sub, None),              // -int -int +int
    OP(Mul, None),              // -int -int +int
    OP(Div, None),              // -int -int +int
    OP(Mod, None),              // -int -int +int
    OP(BitAnd, None),           // -int -int +int
    OP(BitOr, None),            // -int -int +int
    OP(BitXor, None),           // -int -int +int
    OP(BitShl, None),           // -int -int +int
    OP(BitShr, None),           // -int -int +int
    OP(Neg, None),              // -int +int
    OP(BitNot, None),           // -int +int
    OP(Not, None),              // -bool +bool
    OP(AddAssign, None),        // -int -ptr -obj
    OP(SubAssign, None),        // -int -ptr -obj
    OP(MulAssign, None),        // -int -ptr -obj
    OP(DivAssign, None),        // -int -ptr -obj
    OP(ModAssign, None),        // -int -ptr -obj

    // --- 10. BOOLEAN
    OP(CmpOrd, None),           // -int, -int, +{-1,0,1}
    OP(CmpStr, None),           // -str, -str, +{-1,0,1}
    OP(CmpVar, None),           // -var, -var, +{0,1}
    OP(Equal, None),            // -int, +bool
    OP(NotEq, None),            // -int, +bool
    OP(LessThan, None),         // -int, +bool
    OP(LessEq, None),           // -int, +bool
    OP(GreaterThan, None),      // -int, +bool
    OP(GreaterEq, None),        // -int, +bool
    OP(CaseOrd,  None),         // -int -int +int +bool
    OP(CaseRange, None),        // -int -int -int +int +bool
    OP(CaseStr, None),          // -str -str +str +bool
    OP(CaseVar, None),          // -var -var +var +bool
    OP(StkVarGt, StkIdx),       // [stk.idx:u8] -int +bool
    OP(StkVarGe, StkIdx),       // [stk.idx:u8] -int +bool

    // --- 11. JUMPS, CALLS
    OP(Jump, Jump16),           // [dst:s16]
    OP(JumpFalse, Jump16),      // [dst:s16] -bool
    OP(JumpTrue, Jump16),       // [dst:s16] -bool
    OP(JumpAnd, Jump16),        // [dst:s16] (-)bool
    OP(JumpOr, Jump16),         // [dst:s16] (-)bool

    OP(ChildCall, State),       // [State*] -var -var ... +var
    OP(SiblingCall, State),     // [State*] -var -var ... +var
    OP(MethodCall, State),      // [State*] -var -var -obj ... +var
    OP(FarMethodCall, FarState),// [datasegidx:u8, State*] -var -var -obj ... +var
    OP(Call, UInt8),            // [argcount:u8] -var -var -funcptr +var

    // Misc. builtins
    OP(LineNum, LineNum),       // [linenum:int]
    OP(Assert, Assert),         // [State*, linenum:int, cond:str] -bool
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
        _C(RANGE)
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
                case argState:      ADV(State*)->fqName(stm); break;
                case argFarState:   stm << "[ds:" << ADV(uchar) << ']'; ADV(State*)->fqName(stm); break;
                case argUInt8:      stm << to_quoted(*ip); stm << " (" << int(ADV(uchar)) << ')'; break;
                case argInt:        stm << ADV(integer); break;
                case argStr:        stm << to_quoted(ADV(str)); break;
                case argVarType8:   stm << varTypeStr(variant::Type(ADV(uchar))); break;
                case argDefinition: stm << "const " << ADV(Definition*)->name; break;
                case argInnerIdx:   stm << "inner."  << state->innerVars.at(ADV(uchar))->name; break;
                case argOuterIdx:   stm << "outer."  << state->parent->innerVars.at(ADV(uchar))->name; break;
                case argStkIdx:     stm << "local." << int(ADV(uchar)); break;
                case argArgIdx:     stm << "arg." << int(ADV(uchar)); break;
                case argStateIdx:   stm << "state." << int(ADV(uchar)); break;
                case argJump16:     stm << to_string(ip - beginip + ADV(jumpoffs), 16, 4, '0');
                case argLineNum:    break; // handled above
                case argAssert:
                    stm << ADV(State*)->parentModule->filePath;
                    stm << " (" << ADV(integer) << "): ";
                    stm << " \"" << ADV(str) << '"';
                    break;
                case argDump:       stm << ADV(str) << ": "; ADV(Type*)->dumpDef(stm); break;
                case argMax:        break;
            }
        }
        stm << endl;
        if (info.op == opEnd)
            break;
    }
}

