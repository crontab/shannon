

#include "vm.h"


class ENoContext: public Exception
{
public:
    virtual string what() const { return "No execution context"; }
};



VmCode::VmCode(ShScope* iCompilationScope)
    : code(), compilationScope(iCompilationScope)  { }


#ifdef SINGLE_THREADED

VmStack stk;

#endif


void VmCode::runtimeError(int code, const char* msg)
{
    fatal(RUNTIME_FIRST + code, msg);
}


static int compareInt(int a, int b)
    { if (a < b) return -1; else if (a == b) return 0; return 1; }

static int compareLarge(large a, large b)
    { if (a < b) return -1; else if (a == b) return 0; return 1; }

static int compareStr(ptr a, ptr b)
    { return PTR_TO_STRING(a).compare(PTR_TO_STRING(b)); }

static int compareStrChr(ptr a, int b)
    { return PTR_TO_STRING(a).compare(string(char(b))); }

static int compareChrStr(int a, ptr b)
    { return string(char(a)).compare(PTR_TO_STRING(b)); }

static ptr podVecCat(ptr a, ptr b)
{
    ptr result = PTR_TO_STRING(a)._initialize();
    PTR_TO_STRING(result).append(PTR_TO_STRING(b));
    return result;
}


void VmCode::run(VmQuant* p)
{
    while (1)
    {
        switch ((p++)->op_)
        {
        case opEnd: return;
        case opNop: break;

        // --- LOADERS ----------------------------------------------------- //
        case opLoadZero: stk.pushInt(0); break;
        case opLoadLargeZero: stk.pushLarge(0); break;
        case opLoadOne: stk.pushInt(1); break;
        case opLoadLargeOne: stk.pushLarge(1); break;
        case opLoadInt: stk.pushInt((p++)->int_); break;
#ifdef PTR64
        case opLoadLarge: stk.pushLarge((p++)->large_); break;
#else
        case opLoadLarge: stk.pushInt((p++)->int_); stk.pushInt((p++)->int_); break;
#endif
        case opLoadFalse: stk.pushInt(0); break;
        case opLoadTrue: stk.pushInt(1); break;
        case opLoadNull: stk.pushInt(0); break;
        case opLoadNullStr: stk.pushPtr(emptystr); break;
        case opLoadStr: stk.pushPtr((p++)->ptr_); break;
        case opLoadTypeRef: stk.pushPtr((p++)->ptr_); break;

        // --- COMPARISONS ------------------------------------------------- //
        case opCmpInt:
            {
                int r = stk.popInt();
                int* t = stk.topIntRef();
                *t = compareInt(*t, r);
            }
            break;
        case opCmpLarge:
            {
                large r = stk.popLarge();
                large l = stk.popLarge();
                stk.pushInt(compareLarge(l, r));
            }
            break;
        case opCmpStr:
            {
                ptr r = stk.popPtr();
                ptr l = stk.popPtr();
                stk.pushInt(compareStr(l, r));
            }
            break;
        case opCmpStrChr:
            {
                int r = stk.popInt();
                ptr l = stk.popPtr();
                stk.pushInt(compareStrChr(l, r));
            }
            break;
        case opCmpChrStr:
            {
                ptr r = stk.popPtr();
                int* t = stk.topIntRef();
                *t = compareChrStr(*t, r);
            }
            break;
        case opEQ: { int* t = stk.topIntRef(); *t = *t == 0; } break;
        case opLT: { int* t = stk.topIntRef(); *t = *t < 0; } break;
        case opLE: { int* t = stk.topIntRef(); *t = *t <= 0; } break;
        case opGE: { int* t = stk.topIntRef(); *t = *t >= 0; } break;
        case opGT: { int* t = stk.topIntRef(); *t = *t > 0; } break;
        case opNE: { int* t = stk.topIntRef(); *t = *t != 0; } break;

        // typecasts
        case opLargeToInt: stk.pushInt(stk.popLarge()); break;
        case opIntToLarge: stk.pushLarge(stk.popInt()); break;

        // --- BINARY OPERATORS -------------------------------------------- //
#ifdef PTR64
        case opMkSubrange:
            {
                large hi = large(stk.popInt()) << 32;
                stk.pushLarge(unsigned(stk.popInt()) | hi);
            }
            break;
#else
        case opMkSubrange: /* two ints become a subrange, haha! */ break;
#endif

        case opAdd: { int r = stk.popInt(); *stk.topIntRef() += r; } break;
        case opAddLarge: { stk.pushLarge(stk.popLarge() + stk.popLarge()); } break;
        case opSub: { int r = stk.popInt(); *stk.topIntRef() -= r; } break;
        case opSubLarge: { stk.pushLarge(stk.popLarge() - stk.popLarge()); } break;
        case opMul: { int r = stk.popInt(); *stk.topIntRef() *= r; } break;
        case opMulLarge: { stk.pushLarge(stk.popLarge() * stk.popLarge()); } break;
        case opDiv: { int r = stk.popInt(); *stk.topIntRef() /= r; } break;
        case opDivLarge: { stk.pushLarge(stk.popLarge() / stk.popLarge()); } break;
        case opMod: { int r = stk.popInt(); *stk.topIntRef() %= r; } break;
        case opModLarge: { stk.pushLarge(stk.popLarge() % stk.popLarge()); } break;
        case opBitAnd: { int r = stk.popInt(); *stk.topIntRef() &= r; } break;
        case opBitAndLarge: { stk.pushLarge(stk.popLarge() & stk.popLarge()); } break;
        case opBitOr: { int r = stk.popInt(); *stk.topIntRef() |= r; } break;
        case opBitOrLarge: { stk.pushLarge(stk.popLarge() | stk.popLarge()); } break;
        case opBitXor: { int r = stk.popInt(); *stk.topIntRef() ^= r; } break;
        case opBitXorLarge: { stk.pushLarge(stk.popLarge() ^ stk.popLarge()); } break;
        case opBitShl: { int r = stk.popInt(); *stk.topIntRef() <<= r; } break;
        case opBitShlLarge: { stk.pushLarge(stk.popLarge() << stk.popInt()); } break;
        case opBitShr: { int r = stk.popInt(); *stk.topIntRef() >>= r; } break;
        case opBitShrLarge: { stk.pushLarge(stk.popLarge() >> stk.popInt()); } break;

        case opPodVecCat: { ptr r = stk.popPtr(); ptr* l = stk.topPtrRef(); *l = podVecCat(*l, r); } break;

/*        
    opVec1Cat,      // []               -2  +1   vec + vec
    opVec1AddElem,  // []               -2  +1   vec + elem
    opElemAddVec1,  // []               -2  +1   elem + vec
*/
        case opNeg: { int* t = stk.topIntRef(); *t = -*t; } break;
        case opNegLarge: { stk.pushLarge(-stk.popLarge()); } break;
        case opBitNot: { int* t = stk.topIntRef(); *t = ~*t; } break;
        case opBitNotLarge: { stk.pushLarge(~stk.popLarge()); } break;
        case opBoolNot: { int* t = stk.topIntRef(); *t = !*t; } break;

        case opJumpOr: if (stk.topInt()) p += p->int_; else stk.popInt(); break;
        case opJumpAnd: if (stk.topInt()) stk.popInt(); else p += p->int_; break;

        default: fatal(CRIT_FIRST + 50, ("[VM] Unknown opcode " + itostring((--p)->op_, 16, 8, '0')).c_str());
        }
    }
}


void VmCode::verifyClean()
{
    if (!genStack.empty())
        fatal(CRIT_FIRST + 52, "[VM] Emulation stack in undefined state");
    if (!stk.empty())
        fatal(CRIT_FIRST + 53, "[VM] Stack in undefined state");
}


void VmCode::runConstExpr(ShValue& result)
{
    endGeneration();
    run(&code._at(0));
    ShType* type = genPopType();
    if (type->isLargePod())
        result.assignLarge(type, stk.popLarge());
    else if (type->isStrBased())
    {
        ptr p = stk.popPtr();
        result.assignString(type, PTR_TO_STRING(p));
    }
    else if (type->isPodPointer())
        result.assignPtr(type, stk.popPtr());
    else
        result.assignInt(type, stk.popInt());
    verifyClean();
}


ShType* VmCode::runTypeExpr()
{
    endGeneration();
    ShType* type = genPopType();
    verifyClean();
    return type;
}


void VmCode::genCmpOp(OpCode op, OpCode cmp)
{
    genOp(op);
    genOp(cmp);
}


void VmCode::genPush(ShType* t)
{
    genStack.push(GenStackInfo(t));
}


void VmCode::genLoadIntConst(ShOrdinal* type, int value)
{
    genPush(type);
    if (type->isBool())
    {
        genOp(value ? opLoadTrue : opLoadFalse);
    }
    else
    {
        if (value == 0)
            genOp(opLoadZero);
        else if (value == 1)
            genOp(opLoadOne);
        else
        {
            genOp(opLoadInt);
            genInt(value);
        }
    }
}


void VmCode::genLoadLargeConst(ShOrdinal* type, large value)
{
    genPush(type);
    if (value == 0)
        genOp(opLoadLargeZero);
    else if (value == 1)
        genOp(opLoadLargeOne);
    else
    {
        genOp(opLoadLarge);
        genLarge(value);
    }
}


void VmCode::genLoadNull()
{
    genPush(queenBee->defaultVoid);
    genOp(opLoadNull);
}


void VmCode::genLoadTypeRef(ShType* type)
{
    genPush(queenBee->defaultTypeRef);
    genOp(opLoadTypeRef);
    genPtr(type);
}


void VmCode::genLoadStrConst(const char* s)
{
    genPush(queenBee->defaultStr);
    if (*s == 0)
        genOp(opLoadNullStr);
    else
    {
        genOp(opLoadStr);
        genPtr(ptr(s));
    }
}


void VmCode::genLoadConst(ShType* type, podvalue value)
{
    if (type->isOrdinal())
    {
        if (POrdinal(type)->isLargeInt())
            genLoadLargeConst(POrdinal(type), value.large_);
        else
            genLoadIntConst(POrdinal(type), value.int_);
    }
    else if (type->isStrBased())
        genLoadStrConst(pconst(value.ptr_));
    else if (type->isTypeRef())
        genLoadTypeRef((ShType*)value.ptr_);
    else if (type->isVoid())
        genLoadNull();
    else
        internal(50);
}


void VmCode::genMkSubrange()
{
    genPop();
    ShType* type = genPopType();
    if (!type->isOrdinal())
        internal(51);
    genPush(POrdinal(type)->deriveRangeType(compilationScope));
    type = genTopType();
    genOp(opMkSubrange);
}


void VmCode::genComparison(OpCode cmp)
{
    OpCode op = opInv;
    ShType* right = genPopType();
    ShType* left = genPopType();

    bool leftStr = left->isString();
    bool rightStr = right->isString();
    if (leftStr)
    {
        if (right->isChar())
            op = opCmpStrChr;
        else if (rightStr)
            op = opCmpStr;
    }
    else if (rightStr && left->isChar())
    {
        op = opCmpChrStr;
    }

    else if (left->isOrdinal() && right->isOrdinal())
    {
        // TODO: check if one of the operands is 0 and generate CmpZero*
        // If even one of the operands is 64-bit, we generate 64-bit ops
        // with the hope that the parser took care of the rest.
        op = POrdinal(left)->isLargeInt() ? opCmpLarge : opCmpInt;
    }

    if (op == opInv)
        internal(52);

    genPush(queenBee->defaultBool);
    genCmpOp(op, cmp);
}


void VmCode::genStaticCast(ShType* type)
{
    ShType* fromType = genPopType();
    genPush(type);
    bool isDstLarge = type->isLargePod();
    bool isSrcLarge = fromType->isLargePod();
    if (isSrcLarge && !isDstLarge)
        genOp(opLargeToInt);
    else if (!isSrcLarge && isDstLarge)
        genOp(opIntToLarge);
}


void VmCode::genBinArithm(OpCode op, ShInteger* resultType)
{
    genPop();
    genPop();
    genPush(resultType);
    genOp(OpCode(op + resultType->isLargeInt()));
}


void VmCode::genUnArithm(OpCode op, ShInteger* resultType)
{
    genPop();
    genPush(resultType);
    genOp(OpCode(op + resultType->isLargeInt()));
}


void VmCode::genPodVecCat(ShType* resultType)
{
    if (!genPopType()->equals(resultType))
        internal(54);
    if (!genTopType()->equals(resultType))
        internal(54);
    genOp(opPodVecCat);
}


int VmCode::genForwardBoolJump(OpCode op)
{
    genPop(); // because bool jump pops an item if it doesn't jump
    int t = genOffset();
    genOp(op);
    genInt(0);
    return t;
}


void VmCode::genResolveJump(int jumpOffset)
{
    const VmQuant* p = &code[jumpOffset];
    if (!isJump(p->op_))
        internal(53);
    p++;
    ((VmQuant*)p)->int_ = genOffset() - (jumpOffset + 1);
}


void VmCode::endGeneration()
{
    genOp(opEnd);
}


