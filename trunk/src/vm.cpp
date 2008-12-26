

#include "vm.h"


class ENoContext: public Exception
{
public:
    virtual string what() const { return "No execution context"; }
};



VmCode::GenStackInfo::GenStackInfo(const ShValue& iValue, int iOpOffset)
    : value(iValue), opOffset(iOpOffset), isValue(true)  { }

VmCode::GenStackInfo::GenStackInfo(ShType* iType, int iOpOffset)
    : value(iType), opOffset(iOpOffset), isValue(false)  { }


VmCode::VmCode(ShScope* iCompilationScope)
    : code(), compilationScope(iCompilationScope)  { }


#ifdef SINGLE_THREADED

VmStack vmStack;

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


void VmCode::run(VmQuant* p)
{
    while (1)
    {
        switch ((p++)->op_)
        {
        case opEnd: return;
        case opNop: break;

        // --- LOADERS ----------------------------------------------------- //
        case opLoadZero: vmStack.pushInt(0); break;
        case opLoadLargeZero: vmStack.pushLarge(0); break;
        case opLoadOne: vmStack.pushInt(1); break;
        case opLoadLargeOne: vmStack.pushLarge(1); break;
        case opLoadInt: vmStack.pushInt((p++)->int_); break;
#ifdef PTR64
        case opLoadLarge: vmStack.pushLarge((p++)->large_); break;
#else
        case opLoadLarge: vmStack.pushInt((p++)->int_); vmStack.pushInt((p++)->int_); break;
#endif
        case opLoadFalse: vmStack.pushInt(0); break;
        case opLoadTrue: vmStack.pushInt(1); break;
        case opLoadNull: vmStack.pushInt(0); break;
        case opLoadNullStr: vmStack.pushPtr(emptystr); break;
        case opLoadStr: vmStack.pushPtr((p++)->ptr_); break;
        case opLoadTypeRef: vmStack.pushPtr((p++)->ptr_); break;

        // --- COMPARISONS ------------------------------------------------- //
        case opCmpInt:
            {
                int r = vmStack.popInt();
                int* t = vmStack.topIntRef();
                *t = compareInt(*t, r);
            }
            break;
        case opCmpLarge:
            {
                large r = vmStack.popLarge();
                large l = vmStack.popLarge();
                vmStack.pushInt(compareLarge(l, r));
            }
            break;
        case opCmpStr:
            {
                ptr r = vmStack.popPtr();
                ptr l = vmStack.popPtr();
                vmStack.pushInt(compareStr(l, r));
            }
            break;
        case opCmpStrChr:
            {
                int r = vmStack.popInt();
                ptr l = vmStack.popPtr();
                vmStack.pushInt(compareStrChr(l, r));
            }
            break;
        case opCmpChrStr:
            {
                ptr r = vmStack.popPtr();
                int* t = vmStack.topIntRef();
                *t = compareChrStr(*t, r);
            }
            break;
        case opEQ: { int* t = vmStack.topIntRef(); *t = *t == 0; } break;
        case opLT: { int* t = vmStack.topIntRef(); *t = *t < 0; } break;
        case opLE: { int* t = vmStack.topIntRef(); *t = *t <= 0; } break;
        case opGE: { int* t = vmStack.topIntRef(); *t = *t >= 0; } break;
        case opGT: { int* t = vmStack.topIntRef(); *t = *t > 0; } break;
        case opNE: { int* t = vmStack.topIntRef(); *t = *t != 0; } break;

        // typecasts
        case opLargeToInt: vmStack.pushInt(vmStack.popLarge()); break;
        case opIntToLarge: vmStack.pushLarge(vmStack.popInt()); break;

        // --- BINARY OPERATORS -------------------------------------------- //
#ifdef PTR64
        case opMkSubrange:
            {
                large hi = large(vmStack.popInt()) << 32;
                vmStack.pushLarge(unsigned(vmStack.popInt()) | hi);
            }
            break;
#else
        case opMkSubrange: /* two ints become a subrange, haha! */ break;
#endif

        case opAdd: { int r = vmStack.popInt(); *vmStack.topIntRef() += r; } break;
        case opAddLarge: { vmStack.pushLarge(vmStack.popLarge() + vmStack.popLarge()); } break;
        case opSub: { int r = vmStack.popInt(); *vmStack.topIntRef() -= r; } break;
        case opSubLarge: { vmStack.pushLarge(vmStack.popLarge() - vmStack.popLarge()); } break;
        case opMul: { int r = vmStack.popInt(); *vmStack.topIntRef() *= r; } break;
        case opMulLarge: { vmStack.pushLarge(vmStack.popLarge() * vmStack.popLarge()); } break;
        case opDiv: { int r = vmStack.popInt(); *vmStack.topIntRef() /= r; } break;
        case opDivLarge: { vmStack.pushLarge(vmStack.popLarge() / vmStack.popLarge()); } break;
        case opMod: { int r = vmStack.popInt(); *vmStack.topIntRef() %= r; } break;
        case opModLarge: { vmStack.pushLarge(vmStack.popLarge() % vmStack.popLarge()); } break;
        case opBitAnd: { int r = vmStack.popInt(); *vmStack.topIntRef() &= r; } break;
        case opBitAndLarge: { vmStack.pushLarge(vmStack.popLarge() & vmStack.popLarge()); } break;
        case opBitOr: { int r = vmStack.popInt(); *vmStack.topIntRef() |= r; } break;
        case opBitOrLarge: { vmStack.pushLarge(vmStack.popLarge() | vmStack.popLarge()); } break;
        case opBitXor: { int r = vmStack.popInt(); *vmStack.topIntRef() ^= r; } break;
        case opBitXorLarge: { vmStack.pushLarge(vmStack.popLarge() ^ vmStack.popLarge()); } break;
        case opBitShl: { int r = vmStack.popInt(); *vmStack.topIntRef() <<= r; } break;
        case opBitShlLarge: { vmStack.pushLarge(vmStack.popLarge() << vmStack.popInt()); } break;
        case opBitShr: { int r = vmStack.popInt(); *vmStack.topIntRef() >>= r; } break;
        case opBitShrLarge: { vmStack.pushLarge(vmStack.popLarge() >> vmStack.popInt()); } break;

        case opNeg: { int* t = vmStack.topIntRef(); *t = -*t; } break;
        case opNegLarge: { vmStack.pushLarge(-vmStack.popLarge()); } break;
        case opBitNot: { int* t = vmStack.topIntRef(); *t = ~*t; } break;
        case opBitNotLarge: { vmStack.pushLarge(~vmStack.popLarge()); } break;
        case opBoolNot: { int* t = vmStack.topIntRef(); *t = !*t; } break;
/*        
    opVec1Cat,      // []               -2  +1   vec + vec
    opVec1AddElem,  // []               -2  +1   vec + elem
    opElemAddVec1,  // []               -2  +1   elem + vec
*/

        case opJumpOr: if (vmStack.topInt()) p += p->int_; else vmStack.popInt(); break;
        case opJumpAnd: if (vmStack.topInt()) vmStack.popInt(); else p += p->int_; break;

        default: fatal(CRIT_FIRST + 50, ("[VM] Unknown opcode " + itostring((--p)->op_, 16, 8, '0')).c_str());
        }
    }
}


void VmCode::verifyClean()
{
    if (!genStack.empty())
        fatal(CRIT_FIRST + 52, "[VM] Emulation stack in undefined state");
    if (!vmStack.empty())
        fatal(CRIT_FIRST + 53, "[VM] Stack in undefined state");
}


void VmCode::runConstExpr(ShValue& result)
{
    endGeneration();
    run(&code._at(0));
    ShType* type = genPopType();
    if (type->isLargePod())
        result.assignLarge(type, vmStack.popLarge());
    else if (type->isStrBased())
        result.assignStr(type, pconst(vmStack.popPtr()));
    else if (type->isPodPointer())
        result.assignPtr(type, vmStack.popPtr());
    else
        result.assignInt(type, vmStack.popInt());
    verifyClean();
}


ShType* VmCode::runTypeExpr()
{
    endGeneration();
    GenStackInfo& e = genTop();
    ShType* type = e.value.type;
    if (e.value.type->isTypeRef() && e.isValue)
        type = (ShType*)e.value.value.ptr_;
    genPop();
    verifyClean();
    return type;
}


void VmCode::genCmpOp(OpCode op, OpCode cmp)
{
    genOp(op);
    genOp(cmp);
}


void VmCode::genPush(const ShValue& value)
{
    genStack.push(GenStackInfo(value, genOffset()));
}


void VmCode::genPush(ShType* t)
{
    genStack.push(GenStackInfo(t, genOffset()));
}


void VmCode::genLoadIntConst(ShOrdinal* type, int value)
{
    genPush(ShValue(type, value));
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
    genPush(ShValue(type, value));
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
    genPush(ShValue(queenBee->defaultVoid, 0));
    genOp(opLoadNull);
}


void VmCode::genLoadTypeRef(ShType* type)
{
    genPush(ShValue(queenBee->defaultTypeRef, type));
    genOp(opLoadTypeRef);
    genPtr(type);
}


void VmCode::genLoadStrConst(const char* s)
{
    genPush(ShValue(queenBee->defaultStr, s));
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
    type = topType();
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


