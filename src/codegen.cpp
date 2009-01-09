

#include "codegen.h"


ENoContext::ENoContext()
    : Exception("Expression can't be evaluated at compile time")  { }


void noRuntimeContext()
    { throw ENoContext(); }


VmCodeGen::VmCodeGen(ShScope* iDataScope)
    : codeseg(), genStack(), genStackSize(0),
      dataScope(iDataScope), resultTypeHint(NULL)
{
    if (dataScope == NULL)
        internal(79);
}

void VmCodeGen::clear()
{
    codeseg.clear();
    genStack.clear();
}

void VmCodeGen::verifyClean()
{
    if (!genStack.empty() || genStackSize != 0)
        fatal(CRIT_FIRST + 52, "[VM] Emulation stack in undefined state");
    if (!stk.empty())
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
        // help to return the result quicker, without evaluating the whole expr
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
    if (cmp < opCmpFirst || cmp > opCmpLast)
        internal(60);
    codeseg.addOp(cmp);
}

VmCodeGen::GenStackInfo& VmCodeGen::genPush(ShType* type)
{
    GenStackInfo& i = genStack.push();
    i.type = type;
    i.opOffset = codeseg.size();
    i.isValue = false;
    i.isFuncCall = false;
    genStackSize += type->staticSizeAligned;
    codeseg.reserveStack = imax(codeseg.reserveStack, genStackSize);
    resultTypeHint = NULL;
    return i;
}

VmCodeGen::GenStackInfo& VmCodeGen::genPushValue(ShType* type)
{
    GenStackInfo& i = genPush(type);
    i.isValue = true;
    return i;
}

void VmCodeGen::genPushIntValue(ShType* type, int v)
    { genPushValue(type).value.int_ = v; }

void VmCodeGen::genPushLargeValue(ShType* type, large v)
    { genPushValue(type).value.large_ = v; }

void VmCodeGen::genPushPtrValue(ShType* type, ptr v)
    { genPushValue(type).value.ptr_ = v; }

void VmCodeGen::genPushVarRef(ShVariable* var)
    { genPushValue(var->type->deriveRefType()).value.ptr_ = var; }

void VmCodeGen::genPushFuncCall(ShFunction* func)
    { genPush(func->returnVar->type).isFuncCall = true; }

const VmCodeGen::GenStackInfo& VmCodeGen::genPop()
{
    const GenStackInfo& t = genStack.pop();
    genStackSize -= t.type->staticSizeAligned;
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

ShTypeRef* VmCodeGen::genLoadTypeRef(ShType* type)
{
    genPushPtrValue(queenBee->defaultTypeRef, type);
    codeseg.addOp(opLoadTypeRef);
    codeseg.addPtr(type);
    return queenBee->defaultTypeRef;
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
    if (!type->isOrdinal())
        internal(51);
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
        op = left->isLargeInt() ? opCmpLarge : opCmpInt;
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

void VmCodeGen::genElemToVec(ShVariable* tempVar)
{
    verifyContext(tempVar);
    genPop();
    if (!tempVar->type->isVector())
        internal(77);
    genPush(tempVar->type);
    codeseg.addOp(opElemToVec);
    codeseg.addPtr(PVector(tempVar->type)->elementType);
    codeseg.addOffs(tempVar->dataOffset);
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

void VmCodeGen::genLoadVarRef(ShVariable* var)
{
    genPushVarRef(var);
    codeseg.addOp(opLoadRef);
    codeseg.addOffs(var->dataOffset);
}

ShType* VmCodeGen::genDerefVar()
{
    const GenStackInfo& i = genPop();
    if (!i.type->isReference() || !i.isValue || i.opOffset != codeseg.lastOpOffset)
        internal(74);
    ShVariable* var = PVariable(i.value.ptr_);
    // now replace the LoadRef opcode with an opcode that actually loads the
    // value from that var
    verifyContext(var);
    genPush(var->type);
    OpCode op = OpCode(opLoadThisFirst + int(var->type->storageModel));
    if (op < opLoadThisFirst || op > opLoadThisLast)
        internal(61);
    if (var->isLocal())
        op = OpCode(op - opStoreThisFirst + opStoreLocFirst);
    codeseg.at(codeseg.lastOpOffset)->op_ = op;
    return var->type;
}

ShVariable* VmCodeGen::genUndoVar()
{
    const GenStackInfo& i = genPop();
    if (!i.type->isReference() || !i.isValue || i.opOffset != codeseg.lastOpOffset)
        internal(75);
    codeseg.removeLast();
    return PVariable(i.value.ptr_);
}

ShType* VmCodeGen::genUndoTypeRef()
{
    const GenStackInfo& i = genPop();
    if (!i.type->isTypeRef() || !i.isValue || i.opOffset != codeseg.lastOpOffset)
        internal(76);
    codeseg.removeLast();
    return PType(i.value.ptr_);
}

void VmCodeGen::genStoreVar(ShVariable* var)
{
    verifyContext(var);
    genFinVar(var);
    genInitVar(var);
}

offs VmCodeGen::genCase(const ShValue& value, OpCode jumpOp)
{
    genPush(queenBee->defaultBool);
    if (value.type->isOrdinal() && !value.type->isLargeInt())
    {
        codeseg.addOp(opCaseInt);
        codeseg.addInt(value.value.int_);
    }
    else if (value.type->isRange())
    {
        codeseg.addOp(opCaseRange);
        codeseg.addInt(value.rangeMin());
        codeseg.addInt(value.rangeMax());
    }
    else if (value.type->isString())
    {
        codeseg.addOp(opCaseStr);
        codeseg.addPtr(value.value.ptr_);
    }
    else if (value.type->isTypeRef())
    {
        codeseg.addOp(opCaseTypeRef);
        codeseg.addPtr(value.value.ptr_);
    }
    else
        internal(72);
    return genForwardBoolJump(jumpOp);
}

void VmCodeGen::genPopValue(bool finalize)
{
    ShType* type = genPopType();
    switch (type->storageModel)
    {
        case stoByte:
        case stoInt: codeseg.addOp(opPopInt); break;
        case stoLarge: codeseg.addOp(opPopLarge); break;
        case stoPtr: codeseg.addOp(opPopPtr); break;
        case stoVec:
            {
                if (finalize)
                {
                    codeseg.addOp(opPopVec);
                    codeseg.addPtr(type);
                }
                else
                    codeseg.addOp(opPopPtr);
            }
            break;
        case stoVoid: break;
        default: internal(71);
    }
}

void VmCodeGen::genInitVar(ShVariable* var)
{
    verifyContext(var);
    genPop();
    OpCode op = OpCode(opStoreThisFirst + int(var->type->storageModel));
    if (var->isLocal())
        op = OpCode(op - opStoreThisFirst + opStoreLocFirst);
    codeseg.addOp(op);
    codeseg.addOffs(var->dataOffset);
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

void VmCodeGen::genCopyToVec(ShVariable* var)
{
    verifyContext(var);
    if (!var->type->isVector() || !var->isLocal())
        internal(63);
    codeseg.addOp(opCopyToLocVec);
    codeseg.addOffs(var->dataOffset);
}

void VmCodeGen::genVecCat(ShVariable* tempVar)
{
    verifyContext(tempVar);
    genPop();
    if (!genPopType()->isVector() || !tempVar->type->isVector())
        internal(64);
    genPush(tempVar->type);
    codeseg.addOp(opVecCat);
    codeseg.addPtr(PVector(tempVar->type)->elementType);
    codeseg.addOffs(tempVar->dataOffset);
}

void VmCodeGen::genVecElemCat(ShVariable* tempVar)
{
    verifyContext(tempVar);
    genPop();
    if (!genPopType()->isVector() || !tempVar->type->isVector())
        internal(64);
    genPush(tempVar->type);
    codeseg.addOp(opVecElemCat);
    codeseg.addPtr(PVector(tempVar->type)->elementType);
    codeseg.addOffs(tempVar->dataOffset);
}

void VmCodeGen::genIntToStr(ShVariable* tempVar)
{
    verifyContext(tempVar);
    if (!genPopType()->isOrdinal() || !tempVar->type->isString())
        internal(68);
    genPush(tempVar->type);
    codeseg.addOp(tempVar->type->isLargeInt() ? opLargeToStr : opIntToStr);
    codeseg.addOffs(tempVar->dataOffset);
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
    if (op < opRetFirst || op > opRetLast)
        internal(62);
    codeseg.addOp(op);
}

void VmCodeGen::genCall(ShFunction* func)
{
    if (dataScope == NULL)
        internal(78);
    if (func->parent != dataScope) // TODO:
        noRuntimeContext();
    for (int i = func->args.size() - 1; i >= 0; i--)
        genPop();
    genPushFuncCall(func);
    if (func->parent->isModule())
    {
        codeseg.addOp(opCallThis);
        codeseg.addPtr(func);
    }
    else
        internal(73);
}

void VmCodeGen::genEnd()
{
    codeseg.addOp(opEnd);
}

VmCodeSegment VmCodeGen::getCodeSeg()
{
    genEnd();
    return codeseg;
}

void VmCodeGen::verifyContext(ShVariable* var)
{
    if (dataScope == NULL)
        internal(70);
    if (var->ownerScope != dataScope)
        noRuntimeContext();
}
