

#include "codegen.h"


void noRuntimeContext()
{
    throw ENoContext();
}


VmCodeGen::VmCodeGen(ShScope* iHostScope)
    : codeseg(), finseg(), genStack(), genStackSize(0),
      deferredVar(NULL), hostScope(iHostScope), resultTypeHint(NULL)  { }

void VmCodeGen::clear()
{
    codeseg.clear();
    genStack.clear();
}

void VmCodeGen::verifyClean()
{
    if (!genStack.empty() || genStackSize != 0)
        fatal(CRIT_FIRST + 52, "[VM] Emulation stack in undefined state");
    if (stk.bytesize() != 0)
        fatal(CRIT_FIRST + 53, "[VM] Stack in undefined state");
}


void VmCodeGen::runConstExpr(ShValue& result)
{
    const GenStackInfo& i = genTop();
    if (i.isValue)
    {
        // help return the result quicker, without evaluating the whole expr
        result.assignValue(i.type, i.value);
        return;
    }

    result.clear();
    result.type = genTopType();

    genReturn();
    genFinalizeTemps();
    genEnd();
    codeseg.execute(NULL, &result.value);

#ifdef DEBUG
    verifyClean();
#endif
}


ShType* VmCodeGen::runTypeExpr(bool anyObj)
{
    const GenStackInfo& i = genTop();
    if (i.isValue)
    {
        // help return the result quicker, without evaluating the whole expr
        if (i.type->isTypeRef())
            return PType(i.value.ptr_);
    }

    ShValue value;
    runConstExpr(value);

    if (value.type->isTypeRef())
        return PType(value.value.ptr_);
    else if (value.type->isRange())
        return ((ShRange*)value.type)->base->deriveOrdinalFromRange(value);
    else
        return anyObj ? value.type : NULL;
}


void VmCodeGen::genCmpOp(OpCode op, OpCode cmp)
{
    codeseg.addOp(op);
#ifdef DEBUG
    if (cmp < opCmpFirst || cmp > opCmpLast)
        internal(60);
#endif
    codeseg.addOp(cmp);
}

VmCodeGen::GenStackInfo& VmCodeGen::genPush(ShType* t)
{
    GenStackInfo& i = genStack.push();
    i.type = t;
    i.isValue = false;
    genStackSize += t->staticSizeAligned;
    codeseg.reserveStack = imax(codeseg.reserveStack, genStackSize);
    resultTypeHint = NULL;
    return i;
}

void VmCodeGen::genPushIntValue(ShType* type, int v)
{
    GenStackInfo& i = genPush(type);
    i.isValue = true;
    i.value.int_ = v;
}


void VmCodeGen::genPushLargeValue(ShType* type, large v)
{
    GenStackInfo& i = genPush(type);
    i.isValue = true;
    i.value.large_ = v;
}


void VmCodeGen::genPushPtrValue(ShType* type, ptr v)
{
    GenStackInfo& i = genPush(type);
    i.isValue = true;
    i.value.ptr_ = v;
}


const VmCodeGen::GenStackInfo& VmCodeGen::genPop()
{
    const GenStackInfo& t = genTop();
    genStackSize -= t.type->staticSizeAligned;
    genStack.pop();
    return t;
}

ptr VmCodeGen::genTopPtrValue()
{
    const GenStackInfo& i = genTop();
    if (i.isValue)
        return i.value.ptr_;
    else
        return NULL;
}

ShVariable* VmCodeGen::genPopDeferred()
{
    ShVariable* var = deferredVar;
    if (var == NULL)
        internal(67);
    deferredVar = NULL;
    genPop();
    return var;
}

void VmCodeGen::genLoadIntConst(ShType* type, int value)
{
    genPushIntValue(type, value);
    if (type->isBool())
    {
        codeseg.addOp(value ? opLoadTrue : opLoadFalse);
    }
    else
    {
        if (value == 0)
            codeseg.addOp(opLoadZero);
        else if (value == 1)
            codeseg.addOp(opLoadOne);
        else
        {
            codeseg.addOp(opLoadIntConst);
            codeseg.addInt(value);
        }
    }
}

void VmCodeGen::genLoadLargeConst(ShType* type, large value)
{
    genPushLargeValue(type, value);
    if (value == 0)
        codeseg.addOp(opLoadLargeZero);
    else if (value == 1)
        codeseg.addOp(opLoadLargeOne);
    else
    {
        codeseg.addOp(opLoadLargeConst);
        codeseg.addLarge(value);
    }
}

void VmCodeGen::genLoadTypeRef(ShType* type)
{
    genPushPtrValue(queenBee->defaultTypeRef, type);
    codeseg.addOp(opLoadTypeRef);
    codeseg.addPtr(type);
}

void VmCodeGen::genLoadVecConst(ShType* type, const char* s)
{
    genPushVecValue(type, ptr(s));
    if (PTR_TO_STRING(s).empty())
        codeseg.addOp(opLoadNullVec);
    else
    {
        codeseg.addOp(opLoadVecConst);
        codeseg.addPtr(ptr(s));
    }
}

void VmCodeGen::genLoadConst(ShType* type, podvalue value)
{
    switch (type->storageModel)
    {
        case stoByte:
        case stoInt: genLoadIntConst(type, value.int_); break;
        case stoLarge: genLoadLargeConst(type, value.large_); break;
        case stoPtr:
            if (type->isTypeRef())
                genLoadTypeRef(PType(value.ptr_));
            else
                internal(50);
            break;
        case stoVec: genLoadVecConst(type, pconst(value.ptr_)); break;
        default: internal(50);
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
    codeseg.addOp(opMkSubrange);
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
            op = opCmpPodVec;
    }
    else if (rightStr && left->isChar())
    {
        op = opCmpChrStr;
    }

    else if (left->isOrdinal() && right->isOrdinal())
    {
        // If even one of the operands is 64-bit, we generate 64-bit ops
        // with the hope that the parser took care of the rest.
        op = POrdinal(left)->isLargeInt() ? opCmpLarge : opCmpInt;
    }
    
    else if (left->isRange() && right->isRange())
        op = opCmpLarge;

    else if (left->isVector() && right->isVector()
            && (cmp == opEQ || cmp == opNE))
        op = opCmpPodVec;

    else if (left->isTypeRef() && right->isTypeRef())
        op = opCmpTypeRef;

    if (op == opInv)
        internal(52);

    genPush(queenBee->defaultBool);
    genCmpOp(op, cmp);
}


void VmCodeGen::genStaticCast(ShType* type)
{
    ShType* fromType = genPopType();
    genPush(type);
    StorageModel stoFrom = fromType->storageModel;
    StorageModel stoTo = type->storageModel;
    if (stoFrom == stoLarge && stoTo < stoLarge)
        codeseg.addOp(opLargeToInt);
    else if (stoFrom < stoLarge && stoTo == stoLarge)
        codeseg.addOp(opIntToLarge);
    else if (stoFrom < stoLarge && stoTo < stoLarge)
        ;
    else if (stoFrom == stoPtr && stoTo == stoPtr)
        ;
    else if (stoFrom == stoVec && stoTo == stoVec)
        ;
    else
        internal(59);
}

void VmCodeGen::genBinArithm(OpCode op, ShInteger* resultType)
{
    genPop();
    genPop();
    genPush(resultType);
    codeseg.addOp(OpCode(op + resultType->isLargeInt()));
}

void VmCodeGen::genUnArithm(OpCode op, ShInteger* resultType)
{
    genPop();
    genPush(resultType);
    codeseg.addOp(OpCode(op + resultType->isLargeInt()));
}

offs VmCodeGen::genElemToVec(ShVector* vecType)
{
    genPop();
    genPush(vecType);
    offs tmpOffset = genReserveTempVar(vecType);
    codeseg.addOp(opElemToVec);
    codeseg.addPtr(vecType->elementType);
    // stores a copy of the pointer to be finalized later
    codeseg.addOffs(tmpOffset);
    return tmpOffset;
}

offs VmCodeGen::genForwardBoolJump(OpCode op)
{
    if (!genPopType()->isBool())
        internal(69);
    return genForwardJump(op);
}

offs VmCodeGen::genForwardJump(OpCode op)
{
    int t = genOffset();
    codeseg.addOp(op);
    codeseg.addOffs(0);
    return t;
}

void VmCodeGen::genResolveJump(offs jumpOffset)
{
    VmQuant* q = codeseg.at(jumpOffset);
    if (!isJump(OpCode(q->op_)))
        internal(53);
    q++;
    q->offs_ = genOffset() - (jumpOffset + 2);
}

void VmCodeGen::genJump(offs target)
{
    offs o = target - (genOffset() + 2);
    codeseg.addOp(opJump);
    codeseg.addOffs(o);
}

void VmCodeGen::genLoadVar(ShVariable* var)
{
    verifyContext(var);
    genPush(var->type);
    OpCode op = OpCode(opLoadThisFirst + int(var->type->storageModel));
#ifdef DEBUG
    if (op < opLoadThisFirst || op > opLoadThisLast)
        internal(61);
#endif
    if (var->isLocal())
        op = OpCode(op - opStoreThisFirst + opStoreLocFirst);
    codeseg.addOp(op);
    codeseg.addOffs(var->dataOffset);
}

void VmCodeGen::genLoadVarRef(ShVariable* var)
{
    verifyContext(var);
    if (deferredVar != NULL)
        internal(66);
    deferredVar = var;
    genPush(var->type->deriveRefType());
}

void VmCodeGen::genStoreVar(ShVariable* var)
{
    verifyContext(var);
    OpCode op = OpCode(opStoreThisFirst + int(var->type->storageModel));
    if (var->isLocal())
        op = OpCode(op - opStoreThisFirst + opStoreLocFirst);
    codeseg.addOp(op);
    codeseg.addOffs(var->dataOffset);
}

void VmCodeGen::genStore()
{
    ShVariable* var = genPopDeferred();
    genFinVar(var);
    genStoreVar(var);
}

void VmCodeGen::genInitVar(ShVariable* var)
{
    genPop();
    genStoreVar(var);
}

static void genFin(VmCodeSegment& codeseg, ShType* type, offs offset, bool isLocal)
{
    switch (type->storageModel)
    {
        case stoVec:
        {
            if (PVector(type)->isPodVector())
                codeseg.addOp(isLocal ? opFinLocPodVec : opFinThisPodVec);
            else
            {
                codeseg.addOp(isLocal ? opFinLoc : opFinThis);
                codeseg.addPtr(type);
            }
            codeseg.addOffs(offset);
        }
        break;
        default: ;
    }
}

void VmCodeGen::genFinVar(ShVariable* var)
{
    verifyContext(var);
    genFin(codeseg, var->type, var->dataOffset, var->isLocal());
}

offs VmCodeGen::genReserveTempVar(ShType* type)
{
    // note that we are not setting needsRuntimeContext, as this can be
    // executed when evaluating, e.g., const string concatenation
    offs offset = codeseg.reserveLocalVar(type->staticSizeAligned);
    genFin(finseg, type, offset, true);
    return offset;
}

offs VmCodeGen::genCopyToTempVec()
{
    ShType* type = genTopType();
#ifdef DEBUG
    if (!type->isVector())
        internal(63);
#endif
    offs tmpOffset = genReserveTempVar(type);
    codeseg.addOp(opCopyToTmpVec);
    codeseg.addOffs(tmpOffset);
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
    codeseg.addOp(opVecCat);
    codeseg.addPtr(PVector(vecType)->elementType);
    codeseg.addOffs(tempVar);
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
    codeseg.addOp(opVecElemCat);
    codeseg.addPtr(PVector(vecType)->elementType);
    codeseg.addOffs(tempVar);
}

void VmCodeGen::genIntToStr()
{
    ShType* type = genPopType();
    if (!type->isOrdinal())
        internal(68);
    genPush(queenBee->defaultStr);
    offs tmpOffset = genReserveTempVar(queenBee->defaultStr);
    codeseg.addOp(POrdinal(type)->isLargeInt() ? opLargeToStr : opIntToStr);
    codeseg.addOffs(tmpOffset);
}

offs VmCodeGen::genReserveLocalVar(ShType* type)
{
    return codeseg.reserveLocalVar(type->staticSizeAligned);
}

void VmCodeGen::genAssert(Parser& parser)
{
    genPop();
    codeseg.addOp(opAssert);
    codeseg.addPtr(ptr(parser.getFileName().c_str()));
    codeseg.addInt(parser.getLineNum());
}

void VmCodeGen::genLinenum(Parser& parser)
{
    codeseg.addOp(opLinenum);
    codeseg.addPtr(ptr(parser.getFileName().c_str()));
    codeseg.addInt(parser.getLineNum());
}

void VmCodeGen::genReturn()
{
    ShType* returnType = genPopType();
    OpCode op = OpCode(opRetFirst + int(returnType->storageModel));
#ifdef DEBUG
    if (op < opRetFirst || op > opRetLast)
        internal(62);
#endif
    codeseg.addOp(op);
}

void VmCodeGen::genEnd()
{
    if (!codeseg.empty())
        codeseg.addOp(opEnd);
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

void VmCodeGen::verifyContext(ShVariable* var)
{
    if (hostScope == NULL)
        noRuntimeContext();
    if (var->ownerScope != hostScope)
        internal(70);
}
