

#include "vm.h"


class ENoContext: public Exception
{
public:
    virtual string what() const { return "No execution context"; }
};


VmCode::EmulStackInfo::EmulStackInfo(const ShValue& iValue, int iOpIndex)
    : ShValue(iValue), opIndex(iOpIndex), isValue(true)  { }

VmCode::EmulStackInfo::EmulStackInfo(ShType* iType, int iOpIndex)
    : ShValue(iType), opIndex(iOpIndex), isValue(false)  { }


VmCode::VmCode(ShScope* iCompilationScope)
    : code(), compilationScope(iCompilationScope), lastOpIndex(0)  { }


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
        // case opLoadZero: vmStack.pushInt(0); break;
        // case opLoadLargeZero: vmStack.pushLarge(0); break;
        case opLoadInt: vmStack.pushInt((p++)->int_); break;
#ifdef PTR64
        case opLoadLarge: vmStack.pushLarge((p++)->large_); break;
#else
        case opLoadLarge: vmStack.pushInt((p++)->int_); vmStack.pushInt((p++)->int_); break;
#endif
        case opLoadChar: vmStack.pushInt((p++)->int_); break;
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
                int& t = vmStack.topIntRef();
                t = compareInt(t, r);
            }
            break;
        case opCmpIntLarge:
            {
                large r = vmStack.popLarge();
                int& t = vmStack.topIntRef();
                t = compareLarge(t, r);
            }
            break;
        case opCmpLarge:
            {
                large r = vmStack.popLarge();
                large l = vmStack.popLarge();
                vmStack.pushInt(compareLarge(l, r));
            }
            break;
        case opCmpLargeInt:
            {
                int r = vmStack.popInt();
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
                int& t = vmStack.topIntRef();
                t = compareChrStr(t, r);
            }
            break;
        case opEQ: { int& t = vmStack.topIntRef(); t = t == 0; } break;
        case opLT: { int& t = vmStack.topIntRef(); t = t < 0; } break;
        case opLE: { int& t = vmStack.topIntRef(); t = t <= 0; } break;
        case opGE: { int& t = vmStack.topIntRef(); t = t >= 0; } break;
        case opGT: { int& t = vmStack.topIntRef(); t = t > 0; } break;
        case opNE: { int& t = vmStack.topIntRef(); t = t != 0; } break;

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

        default: fatal(CRIT_FIRST + 50, ("[VM] Unknown opcode " + itostring((--p)->op_, 16, 8, '0')).c_str());
        }
    }
}


void VmCode::verifyClean()
{
    if (!emulStack.empty())
        fatal(CRIT_FIRST + 52, "[VM] Emulation stack in undefined state");
    if (!vmStack.empty())
        fatal(CRIT_FIRST + 53, "[VM] Stack in undefined state");
}


ShValue VmCode::runConstExpr()
{
    endGeneration();
    run(&code._at(0));
    ShType* type = emulPopType();
    if (type->isLargePod())
        return ShValue(type, vmStack.popLarge());
    if (type->isPointer())
        return ShValue(type, vmStack.popPtr());
    else
        return ShValue(type, vmStack.popInt());
    verifyClean();
}


ShType* VmCode::runTypeExpr()
{
    endGeneration();
    EmulStackInfo& e = emulTop();
    ShType* type = e.type;
    if (e.type->isTypeRef() && e.isValue)
        type = (ShType*)e.value.ptr_;
    emulPop();
    verifyClean();
    return type;
}


void VmCode::genCmpOp(OpCode op, OpCode cmp)
{
    genOp(op);
    genOp(cmp);
}


void VmCode::emulPush(const ShValue& v)
{
    emulStack.push(EmulStackInfo(v, code.size()));
}


void VmCode::emulPush(ShType* t)
{
    emulStack.push(EmulStackInfo(t, code.size()));
}


void VmCode::genLoadConst(const ShValue& v)
{
    updateLastOpIndex();
    emulPush(v);
    if (v.type->isOrdinal())
    {
        if (v.type->isBool())
            genOp(v.value.int_ ? opLoadTrue : opLoadFalse);
        else if (v.type->isChar())
        {
            genOp(opLoadChar);
            genInt(v.value.int_);
        }
        else if (POrdinal(v.type)->isLarge())
        {
            genOp(opLoadLarge);
            genLarge(v.value.large_);
        }
        else
        {
            genOp(opLoadInt);
            genInt(v.value.int_);
        }
    }
    else if (v.type->isString())
    {
        const string& s = PTR_TO_STRING(v.value.ptr_);
        if (s.empty())
            genOp(opLoadNullStr);
        else
        {
            genOp(opLoadStr);
            genPtr(ptr(s.c_bytes()));
        }
    }
    else if (v.type->isVoid())
        genOp(opLoadNull);
    else
        throw EInternal(50, "Unknown type in VmCode::genLoadConst()");
}


void VmCode::genLoadTypeRef(ShType* type)
{
    updateLastOpIndex();
    emulPush(ShValue(queenBee->defaultTypeRef, type));
    genOp(opLoadTypeRef);
    genPtr(type);
}


void VmCode::genMkSubrange()
{
    updateLastOpIndex();
    emulPop();
    ShType* type = emulPopType();
    if (!type->isOrdinal())
        throw EInternal(51, "Ordinal type expected");
    emulPush(POrdinal(type)->deriveRangeType(compilationScope));
    genOp(opMkSubrange);
}


void VmCode::genComparison(OpCode cmp)
{
    updateLastOpIndex();

    OpCode op = opInv;
    ShType* right = emulPopType();
    ShType* left = emulPopType();

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
        // TODO: check if one of the operands is 0
        static OpCode alchemy[2][2] =
            {{opCmpInt, opCmpIntLarge}, {opCmpLargeInt, opCmpLarge}};
        op = alchemy[POrdinal(left)->isLarge()][POrdinal(right)->isLarge()];
    }

    if (op == opInv)
        throw EInternal(52, "Invalid operand types");

    emulPush(queenBee->defaultBool);
    genCmpOp(op, cmp);
}


void VmCode::genStaticCast(ShType* type)
{
    ShType* fromType = emulPopType();
    emulPush(type);
    bool isDstLarge = type->isLargePod();
    bool isSrcLarge = fromType->isLargePod();
    if (isSrcLarge && !isDstLarge)
        genOp(opLargeToInt);
    else if (!isSrcLarge && isDstLarge)
        genOp(opIntToLarge);
}


void VmCode::endGeneration()
{
    genOp(opEnd);
}


