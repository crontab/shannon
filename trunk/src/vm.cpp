
#include <stdlib.h>
#include <stdio.h>

#include "source.h"
#include "vm.h"

FILE* echostm = stdout;


#ifdef SINGLE_THREADED

VmStack stk;

#endif


static void runtimeError(int code, const char* msg)
{
    fprintf(stderr, "\nRuntime error [%d]: %s\n", code, msg);
    throw code;
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


static void popByType(ShType* type, ShValue& result)
{
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
            }
            break;
        case stoVoid: result.assignVoid(type); stk.popInt(); break;
        default: internal(58);
    }
}

static void doEcho(ShType* type)
{
    ShValue value;
    popByType(type, value);
    string s;
    if (type->isString())
        s = PTR_TO_STRING(value.value.ptr_);
    else
        s = type->displayValue(value);
    fwrite(s.c_bytes(), s.size(), 1, echostm);
}


static void doAssert(const char* fn, int linenum)
{
    if (!stk.popInt())
    {
        string s = string("Assertion failed: ") + fn + '(' + itostring(linenum) + ')';
        runtimeError(1, s.c_str());
    }
}


#define GEN_LOADERS(KIND,PTR) \
    case opLoad##KIND##Byte: stk.pushInt(*puchar(PTR)); break; \
    case opLoad##KIND##Int: stk.pushInt(*pint(PTR)); break; \
    case opLoad##KIND##Large: stk.pushLarge(*plarge(PTR)); break; \
    case opLoad##KIND##Ptr: \
    case opLoad##KIND##Vec: stk.pushPtr(*pptr(PTR)); break; \
    case opLoad##KIND##Void: stk.pushPtr(NULL); break;

#define GEN_STORERS(KIND,PTR) \
    case opStore##KIND##Byte: { int t = stk.popInt(); *puchar(PTR) = t; } break; \
    case opStore##KIND##Int: { int t = stk.popInt(); *pint(PTR) = t; } break; \
    case opStore##KIND##Large: { large t = stk.popLarge(); *plarge(PTR) = t; } break; \
    case opStore##KIND##Ptr: { ptr t  = stk.popPtr(); *pptr(PTR) = t; } break; \
    case opStore##KIND##Vec: { ptr t = stk.popPtr(); pptr s = pptr(PTR); \
            string::_finalize(*s); *s = string::_initialize(t); } break; \
    case opStore##KIND##Void: break; \
    case opInit##KIND##Vec: { ptr t = stk.popPtr(); *pptr(PTR) = string::_initialize(t); } break; \
    case opFin##KIND##Vec: string::_finalize(*pptr(PTR)); break;


// TODO: try to pass the stack pointer as an arg to this func and see the difference in asm.

void VmCodeSegment::run(VmQuant* p, pchar dataseg, pchar stkbase, ptr retval)
{
    while (1)
    {
        switch ((p++)->op_)
        {
        case opEnd: return;
        case opNop: break;

        // --- FUNCTION RETURN VALUE---------------------------------------- //

        case opRetByte:
        case opRetInt: *pint(retval) = stk.popInt(); break;
        case opRetLarge: *plarge(retval) = stk.popLarge(); break;
        case opRetPtr: *pptr(retval) = stk.popPtr(); break;
        case opRetVec: *pptr(retval) = string::_initialize(stk.popPtr());
        case opRetVoid: break;

        // --- LOAD/STORE -------------------------------------------------- //

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
        case opLoadVecConst: stk.pushPtr((p++)->ptr_); break;
        case opLoadTypeRef: stk.pushPtr((p++)->ptr_); break;

        // --- VAR LOAD/STORE ---------------------------------------------- //

        GEN_LOADERS(This, dataseg + (p++)->offs_)
        GEN_STORERS(This, dataseg + (p++)->offs_)

        GEN_LOADERS(Loc, stkbase + (p++)->offs_)
        GEN_STORERS(Loc, stkbase + (p++)->offs_)
        
        // --- SOME VECTOR MAGIC ------------------------------------------- //

        case opCopyToTmpVec:
            *pptr(stkbase + (p++)->offs_) = string::_initialize(stk.topPtr());
            break;
        case opElemToVec:
            {
                int size = (p++)->int_;
                char* vec = pchar(string::_initializen(size));
                if (size > 4)
                    *plarge(vec) = stk.popLarge();
                else if (size > 1)
                    *pint(vec) = stk.popInt();
                else
                    *puchar(vec) = uchar(stk.popInt());
                stk.pushPtr(vec);
                *pptr(stkbase + (p++)->offs_) = vec;
            }
            break;
        case opVecCat:
            {
                ptr r = stk.popPtr();
                ptr* l = stk.topPtrRef();
                PTR_TO_STRING(*l).append(PTR_TO_STRING(r));
                *pptr(stkbase + (p++)->offs_) = *l;
            }
            break;
        case opVecElemCat:
            {
                int size = (p++)->int_;
                ptr* l;
                if (size > 4)
                {
                    large elem = stk.popLarge();
                    l = stk.topPtrRef();
                    *plarge(PTR_TO_STRING(*l).appendn(8)) = elem;
                }
                else
                {
                    int elem = stk.popInt();
                    l = stk.topPtrRef();
                    if (size > 1)
                        *pint(PTR_TO_STRING(*l).appendn(4)) = elem;
                    else
                        PTR_TO_STRING(*l).append(char(elem));
                }
                *pptr(stkbase + (p++)->offs_) = *l;
            }
            break;


        // --- COMPARISONS ------------------------------------------------- //

        case opCmpInt: { int r = stk.popInt(); int* t = stk.topIntRef(); *t = compareInt(*t, r); } break;
        case opCmpLarge: { large r = stk.popLarge(); stk.pushInt(compareLarge(stk.popLarge(), r)); } break;
        case opCmpStr: { ptr r = stk.popPtr(); ptr l = stk.popPtr(); stk.pushInt(PTR_TO_STRING(l).compare(PTR_TO_STRING(r))); } break;
        case opCmpStrChr: { int r = stk.popInt(); ptr l = stk.popPtr(); stk.pushInt(compareStrChr(l, r)); } break;
        case opCmpChrStr: { ptr r = stk.popPtr(); int* t = stk.topIntRef(); *t = compareChrStr(*t, r); } break;
        case opCmpPodVec: { ptr r = stk.popPtr(); ptr l = stk.popPtr(); stk.pushInt(!PTR_TO_STRING(l).equal(PTR_TO_STRING(r))); } break;
        case opCmpPtr: stk.pushInt(!(stk.popPtr() == stk.popPtr())); break;

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

        // --- UNARY OPERATORS --------------------------------------------- //

        case opNeg: { int* t = stk.topIntRef(); *t = -*t; } break;
        case opNegLarge: { stk.pushLarge(-stk.popLarge()); } break;
        case opBitNot: { int* t = stk.topIntRef(); *t = ~*t; } break;
        case opBitNotLarge: { stk.pushLarge(~stk.popLarge()); } break;
        case opBoolNot: { int* t = stk.topIntRef(); *t = !*t; } break;

        // --- JUMPS ------------------------------------------------------- //

        case opJumpOr: if (stk.topInt()) p += p->int_; else stk.popInt(); p++; break;
        case opJumpAnd: if (stk.topInt()) stk.popInt(); else p += p->int_; p++; break;

        // --- MISC. ------------------------------------------------------- //

        case opEcho: doEcho((ShType*)(p++)->ptr_); break;
        case opEchoSp: putc(' ', echostm); break;
        case opEchoLn: fputs("\n", echostm); break;
        case opAssert: { pconst fn = pconst((p++)->ptr_); doAssert(fn, (p++)->int_); } break;

        default: fatal(CRIT_FIRST + 50, ("[VM] Unknown opcode " + itostring((--p)->op_, 16, 8, '0')).c_str());
        }
    }
}



VmCodeSegment::VmCodeSegment()
        : code(), reserveStack(0), reserveLocals(0)  { }


void VmCodeSegment::append(const VmCodeSegment& seg)
{
    reserveStack = imax(reserveStack, seg.reserveStack);
    reserveLocals = imax(reserveLocals, seg.reserveLocals);
    code.append(seg.code);
}


void VmCodeSegment::execute(pchar dataseg, ptr retval)
{
    if (code.empty())
        return;

    // TODO: function arguments (no need for finalization)

    // make room on the stack for this function so that no reallocations are needed
    stk.reservebytes(reserveStack + reserveLocals);

    // reserve space for locals and temps; to be restored to this point later
    char* savebase = stk.pushrbytes(reserveLocals);

    // run, rabbit, run!
    run(getCode(), dataseg, savebase, retval);

    // restore stack base
    stk.restoreendr(savebase);
}


VmCodeGen::VmCodeGen()
    : codeseg(), genStack(), genStackSize(0), needsRuntimeContext(false), resultTypeHint(NULL)  { }


void VmCodeGen::verifyClean()
{
    if (!genStack.empty() || genStackSize != 0)
        fatal(CRIT_FIRST + 52, "[VM] Emulation stack in undefined state");
    if (stk.bytesize() != 0)
        fatal(CRIT_FIRST + 53, "[VM] Stack in undefined state");
}


void VmCodeGen::runConstExpr(ShValue& result)
{
    result.type = NULL;
    if (needsRuntimeContext)
        return;
    result.type = genTopType();
    genReturn();
    genFinalizeTemps();
    genEnd();
    codeseg.execute(NULL, &result.value);
#ifdef DEBUG
    verifyClean();
#endif
}


ShType* VmCodeGen::runTypeExpr(ShValue& value, bool anyObj)
{
    runConstExpr(value);
    if (value.type == NULL)
        return NULL;
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
    // TODO: this is not accurate, takes more than needed:
    genStackSize += t->staticSizeAligned();
    codeseg.reserveStack = imax(codeseg.reserveStack, genStackSize);
    resultTypeHint = NULL;
}


const VmCodeGen::GenStackInfo& VmCodeGen::genPop()
{
    const GenStackInfo& t = genTop();
    genStackSize -= t.type->staticSizeAligned();
    genStack.pop();
    return t;
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

    else if (left->isTypeRef() && right->isTypeRef())
        op = opCmpPtr;

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


offs VmCodeGen::genElemToVec(ShVector* vecType)
{
    genPop();
    genPush(vecType);
    offs tmpOffset = genReserveTempVar(vecType);
    genOp(opElemToVec);
    genInt(vecType->elementType->staticSize());
    // stores a copy of the pointer to be finalized later
    genOffs(tmpOffset);
    return tmpOffset;
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
    q->int_ = genOffset() - (jumpOffset + 2);
}


void VmCodeGen::genLoadThisVar(ShVariable* var)
{
    needsRuntimeContext = true;
    genPush(var->type);
    OpCode op = OpCode(opLoadThisFirst + int(var->type->storageModel()));
#ifdef DEBUG
    if (op < opLoadThisFirst || op > opLoadThisLast)
        internal(61);
#endif
    genOp(op);
    genOffs(var->dataOffset);
}


void VmCodeGen::genInitVar(ShVariable* var)
{
    needsRuntimeContext = true;
    genPop();
    OpCode op = OpCode(opStoreThisFirst + int(var->type->storageModel()));
    if (op == opStoreThisVec)
        op = opInitThisVec;
    if (var->isLocal())
        op = OpCode(op - opStoreThisFirst + opStoreLocFirst);
    genOp(op);
    genOffs(var->dataOffset);
}


offs VmCodeGen::genCopyToTempVec()
{
    ShType* type = genTopType();
#ifdef DEBUG
    if (!type->isVector())
        internal(63);
#endif
    offs tmpOffset = genReserveTempVar(type);
    genOp(opCopyToTmpVec);
    genOffs(tmpOffset);
    return tmpOffset;
}


void VmCodeGen::genVecCat(offs tempVar)
{
    genPop();
    ShType* vecType = genPopType();
#ifdef DEBUG
    if (!vecType->isVector())
        internal(64);
#endif
    genPush(vecType);
    genOp(opVecCat);
    genOffs(tempVar);
}


void VmCodeGen::genVecElemCat(offs tempVar)
{
    genPop();
    ShType* vecType = genPopType();
#ifdef DEBUG
    if (!vecType->isVector())
        internal(64);
#endif
    genPush(vecType);
    genOp(opVecElemCat);
    genInt(PVector(vecType)->elementType->staticSize());
    genOffs(tempVar);
}


void VmCodeGen::genFinVar(ShVariable* var)
{
    needsRuntimeContext = true;
    if (var->type->storageModel() == stoVec)
    {
        genOp(var->isLocal() ? opFinLocVec : opFinThisVec);
        genOffs(var->dataOffset);
    }
}


offs VmCodeGen::genReserveTempVar(ShType* type)
{
    offs offset = codeseg.reserveLocals;
    codeseg.reserveLocals += type->staticSizeAligned();
    if (type->storageModel() == stoVec)
    {
        finseg.add()->op_ = opFinLocVec;
        finseg.add()->offs_ = offset;
    }
    return offset;
}


void VmCodeGen::genAssert(Parser& parser)
{
    genPop();
    genOp(opAssert);
    genPtr(ptr(parser.getFileName().c_str()));
    genInt(parser.getLineNum());
}


void VmCodeGen::genReturn()
{
    ShType* returnType = genPopType();
    OpCode op = OpCode(opRetFirst + int(returnType->storageModel()));
#ifdef DEBUG
    if (op < opRetFirst || op > opRetLast)
        internal(62);
#endif
    genOp(op);
}


void VmCodeGen::genEnd()
{
    if (!codeseg.empty())
        genOp(opEnd);
}


void VmCodeGen::genFinalizeTemps()
{
    if (!finseg.empty())
    {
        codeseg.append(finseg);
        finseg.clear();
    }
}


VmCodeSegment VmCodeGen::getCodeSeg()
{
    genFinalizeTemps();
    genEnd();
    return codeseg;
}

