

#include "codegen.h"


VmCodeGen::VmCodeGen()
    : codeseg(), finseg(), genStack(), genStackSize(0), needsRuntimeContext(false), 
      deferredVar(NULL), resultTypeHint(NULL)  { }

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
    codeseg.addOp(op);
#ifdef DEBUG
    if (cmp < opCmpFirst || cmp > opCmpLast)
        internal(60);
#endif
    codeseg.addOp(cmp);
}

void VmCodeGen::genPush(ShType* t)
{
    genStack.push(GenStackInfo(t));
    genStackSize += t->staticSizeAligned;
    codeseg.reserveStack = imax(codeseg.reserveStack, genStackSize);
    resultTypeHint = NULL;
}

const VmCodeGen::GenStackInfo& VmCodeGen::genPop()
{
    const GenStackInfo& t = genTop();
    genStackSize -= t.type->staticSizeAligned;
    genStack.pop();
    return t;
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

void VmCodeGen::genLoadIntConst(ShOrdinal* type, int value)
{
    genPush(type);
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

void VmCodeGen::genLoadLargeConst(ShOrdinal* type, large value)
{
    genPush(type);
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
    genPush(queenBee->defaultTypeRef);
    codeseg.addOp(opLoadTypeRef);
    codeseg.addPtr(type);
}

void VmCodeGen::genLoadVecConst(ShType* type, const char* s)
{
    genPush(type);
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
    codeseg.addInt(0);
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

void VmCodeGen::genLoadVar(ShVariable* var)
{
    needsRuntimeContext = true;
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
    needsRuntimeContext = true;
    if (deferredVar != NULL)
        internal(66);
    deferredVar = var;
    genPush(var->type->deriveRefType());
}

void VmCodeGen::genStoreVar(ShVariable* var)
{
    needsRuntimeContext = true;
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
    needsRuntimeContext = true;
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


