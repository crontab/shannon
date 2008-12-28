

#include "vm.h"


#ifdef SINGLE_THREADED

VmStack stk;

#endif


void VmCodeSegment::runtimeError(int code, const char* msg)
{
    fatal(RUNTIME_FIRST + code, msg);
}

// TODO: probably only <= is enough (i.e. return only 0 or < 0), the rest can 
// be optimized by the code generator.
static int compareInt(int a, int b)
    { if (a < b) return -1; else if (a == b) return 0; return 1; }

static int compareLarge(large a, large b)
    { if (a < b) return -1; else if (a == b) return 0; return 1; }

static int compareStrChr(ptr a, int b)
    { return PTR_TO_STRING(a).compare(string(char(b))); }

static int compareChrStr(int a, ptr b)
    { return string(char(a)).compare(PTR_TO_STRING(b)); }


// TODO: ref counts are incremented and decremented every time a value is
// pushed/popped from the stack, which won't be efficient in the multithreaded
// version. Need to do the same way as C++ does with overloaded operators with
// const arguments (create temp vars). Also, it would be easier to implement
// exceptions, as temp vars will be finalized like all others during unwind.
void VmCodeSegment::run(VmQuant* p, char* dataseg)
{
//    int sktBase = stk.size();
    while (1)
    {
        switch ((p++)->op_)
        {
        case opEnd: return;
        case opNop: break;
        case opStkFrame: stk.reserve((p++)->int_); break;

        // --- LOADERS ----------------------------------------------------- //
        case opLoadZero: stk.pushInt(0); break;
        case opLoadLargeZero: stk.pushLarge(0); break;
        case opLoadOne: stk.pushInt(1); break;
        case opLoadLargeOne: stk.pushLarge(1); break;
        case opLoadIntConst: stk.pushInt((p++)->int_); break;
#ifdef PTR64
        case opLoadLargeConst: stk.pushLarge((p++)->large_); break;
#else
        case opLoadLargeConst:
            { int lo = (p++)->int_; stk.pushLarge(lo, (p++)->int_); } break;
#endif
        case opLoadFalse: stk.pushInt(0); break;
        case opLoadTrue: stk.pushInt(1); break;
        case opLoadNull: stk.pushPtr(NULL); break;
        case opLoadNullVec: stk.pushPtr(emptystr); break;
        case opLoadVecConst: stk.pushPtr(string::_initialize((p++)->ptr_)); break;
        case opLoadTypeRef: stk.pushPtr((p++)->ptr_); break;

        case opLoadThisRef: stk.pushOffs((p++)->offs_); break;
        case opLoadThisByte: stk.pushInt(*puchar(dataseg + (p++)->offs_)); break;
        case opLoadThisInt: stk.pushInt(*pint(dataseg + (p++)->offs_)); break;
        case opLoadThisLarge: stk.pushLarge(*plarge(dataseg + (p++)->offs_)); break;
        case opLoadThisPtr: stk.pushPtr(*pptr(dataseg + (p++)->offs_)); break;
        case opLoadThisVec: stk.pushPtr(string::_initialize(*pptr(dataseg + (p++)->offs_))); break;
        case opLoadThisVoid: stk.pushPtr(NULL); break;

        case opStoreThisByte: { uchar v = stk.popInt(); *puchar(dataseg + stk.popOffs()) = v; } break;
        case opStoreThisInt: { int v = stk.popInt(); *pint(dataseg + stk.popOffs()) = v; } break;
        case opStoreThisLarge: { large v = stk.popLarge(); *pint(dataseg + stk.popOffs()) = v; } break;
        case opStoreThisPtr: { ptr v = stk.popPtr(); *pptr(dataseg + stk.popOffs()) = v; } break;
        case opStoreThisVec:
            {
                ptr v = stk.popPtr();
                pptr s = pptr(dataseg + stk.popOffs());
                string::_finalize(*s);
                *s = v;
            }
            break;
        case opInitThisVec: { ptr v = stk.popPtr(); *pptr(dataseg + stk.popOffs()) = v; } break;
        case opFinThisPodVec: { string::_finalize(*pptr(dataseg + (p++)->offs_)); } break;

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
                stk.pushInt(PTR_TO_STRING(l).compare(PTR_TO_STRING(r)));
                string::_finalize(r);
                string::_finalize(l);
            }
            break;
        case opCmpStrChr:
            {
                int r = stk.popInt();
                ptr l = stk.popPtr();
                stk.pushInt(compareStrChr(l, r));
                string::_finalize(l);
            }
            break;
        case opCmpChrStr:
            {
                ptr r = stk.popPtr();
                int* t = stk.topIntRef();
                *t = compareChrStr(*t, r);
                string::_finalize(r);
            }
            break;
        case opCmpPodVec:
            {
                ptr r = stk.popPtr();
                ptr l = stk.popPtr();
                stk.pushInt(!PTR_TO_STRING(l).equal(PTR_TO_STRING(r)));
                string::_finalize(r);
                string::_finalize(l);
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

        case opPodVecCat:
            {
                ptr r = stk.popPtr();
                PTR_TO_STRING(*stk.topPtrRef()).append(PTR_TO_STRING(r));
                string::_finalize(r);
            }
            break;

        case opPodVecElemCat:
            {
                int size = (p++)->int_;
                if (size > 4)
                {
                    large elem = stk.popLarge();
                    *plarge(PTR_TO_STRING(*stk.topPtrRef()).appendn(8)) = elem;
                }
                else if (size > 1)
                {
                    int elem = stk.popInt();
                    *pint(PTR_TO_STRING(*stk.topPtrRef()).appendn(4)) = elem;
                }
                else
                {
                    int elem = stk.popInt();
                    PTR_TO_STRING(*stk.topPtrRef()).append(char(elem));
                }
            }
            break;

        case opPodElemVecCat:
            {
                int size = (p++)->int_;
                ptr vec = stk.popPtr();
                if (size > 4)
                    *plarge(PTR_TO_STRING(vec).ins(0, 8)) = stk.popLarge();
                else if (size > 1)
                    *pint(PTR_TO_STRING(vec).ins(0, 4)) = stk.popInt();
                else
                    *PTR_TO_STRING(vec).ins(0, 1) = char(stk.popInt());
                stk.pushPtr(vec);
            }
            break;

        case opPodElemElemCat:
            {
                int size = (p++)->int_;
                char* vec = pchar(string::_initializen(size * 2));
                char* vec1 = vec + size;
                if (size > 4)
                {
                    *plarge(vec1) = stk.popLarge();
                    *plarge(vec) = stk.popLarge();
                }
                else if (size > 1)
                {
                    *pint(vec1) = stk.popInt();
                    *pint(vec) = stk.popInt();
                }
                else
                {
                    *pchar(vec1) = char(stk.popInt());
                    *pchar(vec) = char(stk.popInt());
                }
                stk.pushPtr(vec);
            }
            break;

        case opPodElemToVec:
            {
                int size = (p++)->int_;
                char* vec = pchar(string::_initializen(size));
                if (size > 4)
                    *plarge(vec) = stk.popLarge();
                else if (size > 1)
                    *pint(vec) = stk.popInt();
                else
                    *pchar(vec) = char(stk.popInt());
                stk.pushPtr(vec);
            }
            break;

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


VmCodeGen::VmCodeGen()
    : codeseg(), genStack(), stackMax(0), reserveLocals(0),
      needsRuntimeContext(false), resultTypeHint(NULL)
{
    genOp(opStkFrame);
    genInt(0);
}


void VmCodeGen::verifyClean()
{
    if (!genStack.empty())
        fatal(CRIT_FIRST + 52, "[VM] Emulation stack in undefined state");
    if (!stk.empty())
        fatal(CRIT_FIRST + 53, "[VM] Stack in undefined state");
}


void VmCodeGen::runConstExpr(ShValue& result)
{
    genEnd();
    if (needsRuntimeContext)
    {
        result.type = NULL;
        return;
    }
    codeseg.execute(NULL);
    ShType* type = genPopType();
    switch (type->storageModel())
    {
        case stoByte:
        case stoInt: result.assignInt(type, stk.popInt()); break;
        case stoLarge: result.assignLarge(type, stk.popLarge()); break;
        case stoPtr: result.assignPtr(type, stk.popPtr()); break;
        case stoVec: 
            {
                ptr p = stk.popPtr();
                result.assignVec(type, PTR_TO_STRING(p));
                string::_finalize(p);
            }
            break;
        case stoVoid: result.assignVoid(type); stk.popInt(); break;
        default: internal(58);
    }
    verifyClean();
}


ShType* VmCodeGen::runTypeExpr(bool anyObj = true)
{
    ShValue value;
    runConstExpr(value);
    if (value.type->isTypeRef())
        return (ShType*)value.value.ptr_;
    else if (value.type->isRange())
        return ((ShRange*)value.type)->base->deriveOrdinalFromRange(value);
    return anyObj ? value.type : NULL;
}


void VmCodeGen::genCmpOp(OpCode op, OpCode cmp)
{
    genOp(op);
#ifdef DEBUG
    if (cmp < opCmpFirst || cmp > opCmpLast)
        internal(60);
#endif
    genOp(cmp);
}


void VmCodeGen::genPush(ShType* t)
{
    genStack.push(GenStackInfo(t, genOffset()));
    stackMax = imax(stackMax, genStack.size());
    resultTypeHint = NULL;
}


const VmCodeGen::GenStackInfo& VmCodeGen::genPop()
{
    const GenStackInfo& i = genStack.pop();
    VmQuant* q = codeseg.at(i.codeOffs);
    if (q->op_ == opLoadThisRef)
    {
        OpCode op = OpCode(opLoadThisFirst + int(i.type->storageModel()));
#ifdef DEBUG
        if (op < opLoadThisFirst || op > opLoadThisLast)
            internal(61);
#endif
        q->op_ = op;
        needsRuntimeContext = true;
    }
    return i;
}


void VmCodeGen::genLoadIntConst(ShOrdinal* type, int value)
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
            genOp(opLoadIntConst);
            genInt(value);
        }
    }
}


void VmCodeGen::genLoadLargeConst(ShOrdinal* type, large value)
{
    genPush(type);
    if (value == 0)
        genOp(opLoadLargeZero);
    else if (value == 1)
        genOp(opLoadLargeOne);
    else
    {
        genOp(opLoadLargeConst);
        genLarge(value);
    }
}


void VmCodeGen::genLoadNull()
{
    genPush(queenBee->defaultVoid);
    genOp(opLoadNull);
}


void VmCodeGen::genLoadTypeRef(ShType* type)
{
    genPush(queenBee->defaultTypeRef);
    genOp(opLoadTypeRef);
    genPtr(type);
}


void VmCodeGen::genLoadVecConst(ShType* type, const char* s)
{
    genPush(type);
    if (PTR_TO_STRING(s).empty())
        genOp(opLoadNullVec);
    else
    {
        genOp(opLoadVecConst);
        genPtr(ptr(s));
    }
}


void VmCodeGen::genLoadConst(ShType* type, podvalue value)
{
    if (type->isOrdinal())
    {
        if (POrdinal(type)->isLargeInt())
            genLoadLargeConst(POrdinal(type), value.large_);
        else
            genLoadIntConst(POrdinal(type), value.int_);
    }
    else if (type->isVector())
        genLoadVecConst(type, pconst(value.ptr_));
    else if (type->isTypeRef())
        genLoadTypeRef((ShType*)value.ptr_);
    else if (type->isVoid())
        genLoadNull();
    else
        internal(50);
}


void VmCodeGen::genLoadThisVar(ShVariable* var)
{
    // NOTE: var->type at this point can be void if it's a typeless decl.
    genPush(var->type);
    genOp(opLoadThisRef);
    genOffs(var->dataOffset);
}


void VmCodeGen::genInitThisVar(ShType* type)
{
    genPop();
    genPopStore();
    int s = genStack.size();
    if (s != 0)
        internal(99);
    OpCode op = OpCode(opStoreThisFirst + int(type->storageModel()));
    if (op == opStoreThisVec)
        op = opInitThisVec;
    genOp(op);
}


void VmCodeGen::genFinThisVar(ShVariable* var)
{
    StorageModel sto = var->type->storageModel();
    if (sto == stoVec)
    {
        // TODO: non-POD vectors
        genOp(opFinThisPodVec);
        genOffs(var->dataOffset);
    }
}


void VmCodeGen::genMkSubrange()
{
    genPop();
    ShType* type = genPopType();
#ifdef DEBUG
    if (!type->isOrdinal())
        internal(51);
#endif
    genPush(POrdinal(type)->deriveRangeType());
    genOp(opMkSubrange);
}


void VmCodeGen::genComparison(OpCode cmp)
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
    
    else if (left->isVector() && right->isVector()
            && (cmp == opEQ || cmp == opNE))
        op = opCmpPodVec;

    if (op == opInv)
        internal(52);

    genPush(queenBee->defaultBool);
    genCmpOp(op, cmp);
}


void VmCodeGen::genStaticCast(ShType* type)
{
    ShType* fromType = genPopType();
    genPush(type);
    StorageModel stoFrom = fromType->storageModel();
    StorageModel stoTo = type->storageModel();
    if (stoFrom == stoLarge && stoTo < stoLarge)
        genOp(opLargeToInt);
    else if (stoFrom < stoLarge && stoTo == stoLarge)
        genOp(opIntToLarge);
    // We generate opNop because genPush() stores the position of the next
    // opcode, and it should be something. NOPs will be removed in the future
    // during the optimization phase.
    else if (stoFrom < stoLarge && stoTo < stoLarge)
        genNop();
    else if (stoFrom == stoPtr && stoTo == stoPtr)
        genNop();
    else if (stoFrom == stoVec && stoTo == stoVec)
        genNop();
    else
        internal(59);
}


void VmCodeGen::genBinArithm(OpCode op, ShInteger* resultType)
{
    genPop();
    genPop();
    genPush(resultType);
    genOp(OpCode(op + resultType->isLargeInt()));
}


void VmCodeGen::genUnArithm(OpCode op, ShInteger* resultType)
{
    genPop();
    genPush(resultType);
    genOp(OpCode(op + resultType->isLargeInt()));
}

void VmCodeGen::genVecCat()
{
    ShType* right = genPopType();
    ShType* left = genPopType();
    // TODO: non-POD vectors
    if (left->isVector())
    {
        genPush(left);
        if (right->isVector())
            genOp(opPodVecCat);
        else
        {
            genOp(opPodVecElemCat);
            genInt(right->staticSizeRequired());
        }
    }
    else if (right->isVector())
    {
        genPush(right);
        genOp(opPodElemVecCat);
        genInt(left->staticSizeRequired());
    }
    else
    {
#ifdef DEBUG
        if (left->staticSize() != right->staticSize())
            internal(54);
#endif
        genPush(left->deriveVectorType());
        genOp(opPodElemElemCat);
        genInt(left->staticSizeRequired());
    }
}


void VmCodeGen::genElemToVec(ShVector* vecType)
{
    genPop();
    genPush(vecType);
    genOp(opPodElemToVec);
    genInt(vecType->elementType->staticSizeRequired());
}


int VmCodeGen::genForwardBoolJump(OpCode op)
{
    genPop(); // because bool jump pops an item if it doesn't jump
    int t = genOffset();
    genOp(op);
    genInt(0);
    return t;
}


void VmCodeGen::genResolveJump(int jumpOffset)
{
    VmQuant* q = codeseg.at(jumpOffset);
    if (!isJump(OpCode(q->op_)))
        internal(53);
    q++;
    q->int_ = genOffset() - (jumpOffset + 1);
}


void VmCodeGen::genEnd()
{
    genOp(opEnd);
#ifdef DEBUG
    if (codeseg.at(0)->op_ != opStkFrame)
        internal(56);
#endif
    codeseg.at(1)->int_ = stackMax + reserveLocals;
}


