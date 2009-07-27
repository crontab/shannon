
#include "vm.h"


typedef void (*argfunc)(uchar*& ip);


enum ArgType
    { argNone, argChar, argInt, argType, argConst, argConst16, argIndex,
        argModIndex, argLevelIndex, argJump16, argIntInt, argFile16Line16 };


struct OpInfo
{
    const char* name;
    OpCode op;
    ArgType arg;
};


#define OP(o,a)  { #o, op##o, arg##a }


static OpInfo opTable[] = 
{
    OP(Inv, None),
    OP(End, None),
    OP(Nop, None),
    OP(Exit, None),
    OP(LoadNull, None),         // +null
    OP(LoadFalse, None),        // +false
    OP(LoadTrue, None),         // +true
    OP(LoadChar, Char),         // [8] +char
    OP(Load0, None),            // +0
    OP(Load1, None),            // +1
    OP(LoadInt, Int),           // [int] +int
    OP(LoadNullRange, Type),    // [Range*] +range
    OP(LoadNullDict, Type),     // [Dict*] +dict
    OP(LoadNullStr, None),      // +str
    OP(LoadNullVec, Type),      // [Vector*] +vec
    OP(LoadNullArray, Type),    // [Array*] +array
    OP(LoadNullOrdset, Type),   // [Ordset*] +ordset
    OP(LoadNullSet, Type),      // [Set*] +set
    OP(LoadConst, Const),       // [const-index: 8] +var // compound values only
    OP(LoadConst2, Const16),    // [const-index: 16] +var // compound values only
    OP(LoadTypeRef, Type),      // [Type*] +typeref
    OP(Pop, None),              // -var
    OP(Swap, None),
    OP(Dup, None),              // +var
    OP(ToBool, None),           // -var, +bool
    OP(ToStr, None),            // -var, +str
    OP(ToType, Type),           // [Type*] -var, +var
    OP(ToTypeRef, None),        // -type, -var, +var
    OP(IsType, Type),           // [Type*] -var, +bool
    OP(IsTypeRef, None),        // -type, -var, +bool
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
    OP(BoolXor, None),          // -bool, -bool, +bool
    OP(Neg, None),              // -int, +int
    OP(BitNot, None),           // -int, +int
    OP(Not, None),              // -bool, +bool
    OP(MkRange, Type),          // [Ordinal*] -right-int, -left-int, +range
    OP(InRange, None),          // -range, -int, +bool
    OP(CmpOrd, None),           // -ord, -ord, +{-1,0,1}
    OP(CmpStr, None),           // -str, -str, +{-1,0,1}
    OP(CmpVar, None),           // -var, -var, +{0,1}
    OP(Equal, None),            // -int, +bool
    OP(NotEq, None),            // -int, +bool
    OP(LessThan, None),         // -int, +bool
    OP(LessEq, None),           // -int, +bool
    OP(GreaterThan, None),      // -int, +bool
    OP(GreaterEq, None),        // -int, +bool
    OP(InitRet, Index),         // [ret-index] -var
    OP(InitLocal, Index),       // [stack-index: 8]
    OP(InitThis, Index),        // [this-index: 8]
    OP(LoadRet, Index),         // [ret-index] +var
    OP(LoadLocal, Index),       // [stack-index: 8] +var
    OP(LoadThis, Index),        // [this-index: 8] +var
    OP(LoadArg, Index),         // [stack-neg-index: 8] +var
    OP(LoadStatic, ModIndex),   // [Module*, var-index: 8] +var
    OP(LoadMember, Index),      // [var-index: 8] -obj, +val
    OP(LoadOuter, LevelIndex),  // [level: 8, var-index: 8] +var
    OP(LoadDictElem, None),     // -key, -dict, +val
    OP(InDictKeys, None),       // -dict, -key, +bool
    OP(LoadStrElem, None),      // -index, -str, +char
    OP(LoadVecElem, None),      // -index, -vector, +val
    OP(LoadArrayElem, None),    // -index, -array, +val
    OP(InOrdset, None),         // -ordset, -ord, +bool
    OP(InSet, None),            // -ordset, -key, +bool
    OP(StoreRet, Index),        // [ret-index] -var
    OP(StoreLocal, Index),      // [stack-index: 8] -var
    OP(StoreThis, Index),       // [this-index: 8] -var
    OP(StoreArg, Index),        // [stack-neg-index: 8] -var
    OP(StoreStatic, ModIndex),  // [Module*, var-index: 8] -var
    OP(StoreMember, Index),     // [var-index: 8] -val, -obj
    OP(StoreOuter, LevelIndex), // [level: 8, var-index: 8] -var
    OP(StoreDictElem, None),    // -val, -key, -dict
    OP(DelDictElem, None),      // -key, -dict
    OP(StoreVecElem, None),     // -val, -index, -vector
    OP(StoreArrayElem, None),   // -val, -index, -array
    OP(AddToOrdset, None),      // -ord, -ordset
    OP(AddToSet, None),         // -key, -set
    OP(CharToStr, None),        // -char, +str
    OP(CharCat, None),          // -char, -str, +str
    OP(StrCat, None),           // -str, -str, +str
    OP(VarToVec, Type),         // [Vector*] -var, +vec
    OP(VarCat, None),           // -var, -vec, +vec
    OP(VecCat, None),           // -var, -vec, +vec
    OP(Empty, None),            // -var, +bool
    OP(StrLen, None),           // -str, +int
    OP(VecLen, None),           // -vec, +int
    OP(RangeDiff, None),        // -range, +int
    OP(RangeLow, None),         // -range, +ord
    OP(RangeHigh, None),        // -range, +ord
    OP(Jump, Jump16),           // [dst 16]
    OP(JumpTrue, Jump16),       // [dst 16] -bool
    OP(JumpFalse, Jump16),      // [dst 16] -bool
    OP(JumpOr, Jump16),         // [dst 16] (-)bool
    OP(JumpAnd, Jump16),        // [dst 16] (-)bool
    OP(CaseInt, Int),           // [int], +bool
    OP(CaseRange, IntInt),      // [int, int], +bool
    OP(CaseStr, None),          // -str, +bool
    OP(CaseTypeRef, None),      // -typeref, +bool
    OP(Call, Type),             // [Type*]
    OP(Echo, None),             // -var
    OP(EchoSpace, None),
    OP(EchoLn, None),
    OP(LineNum, File16Line16),  // [file-id: 16, line-num: 16]
    OP(Assert, None),           // -bool
    OP(MaxCode, None),
};


#ifdef DEBUG
static struct vmdebuginit
{
    vmdebuginit()
    {
        for (int i = 0; i <= opMaxCode; i++)
        {
            if (opTable[i].op != i)
                fatal(0x5100, "VM debug table inconsistency");
        }
    }
} _vmdebuginit;
#endif


void CodeSeg::listing(fifo_intf& stm) const
{
    if (code.empty())
        return;
    const uchar* saveip = (const uchar*)code.data();
    const uchar* ip = saveip;
    while (1)
    {
        const OpInfo& info = opTable[*ip];
        stm << to_string(ip - saveip, 16, 4, '0') << ":\t";
        ip++;
        if (info.op == opLineNum)
        {
            stm << ";--- " << hostModule->fileNames[ADV<uint16_t>(ip)];
            stm << '(' << ADV<uint16_t>(ip) << ')';
        }
        else
        {
            stm << info.name;
            if (info.arg != argNone)
            {
                stm << '\t';
                if (strlen(info.name) < 8)
                    stm << '\t';
                switch (info.arg)
                {
                    case argNone:       break;
                    case argChar:       stm << mkQuotedPrintable(ADV<char>(ip)); break;
                    case argInt:        stm << ADV<integer>(ip); break;
                    case argType:       stm << *ADV<Type*>(ip); break;
                    case argConst:      stm << consts[ADV<uchar>(ip)]; break;
                    case argConst16:    stm << consts[ADV<uint16_t>(ip)]; break;
                    case argIndex:      stm << '.' << integer(ADV<uchar>(ip)); break;
                    case argModIndex:   stm << ADV<Module*>(ip)->name; stm << '.' << int(ADV<uchar>(ip)); break;
                    case argLevelIndex: stm << '.' << ADV<uchar>(ip); stm << ':' << int(ADV<uchar>(ip)); break;
                    case argJump16:
                        {
                            mem o = ADV<joffs_t>(ip);
                            stm << to_string(ip - saveip + o, 16, 4, '0');
                        }
                        break;
                    case argIntInt:     stm << ADV<integer>(ip); stm << ',' << ADV<integer>(ip); break;
                    case argFile16Line16: break;
                }
            }
        }
        stm << endl;
        if (info.op == opEnd)
            break;
    }
    stm.flush();
}

