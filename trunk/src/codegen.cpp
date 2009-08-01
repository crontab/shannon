

#include "vm.h"


CodeGen::CodeGen(CodeSeg* _codeseg)
  : codeseg(_codeseg), state(_codeseg->ownState),
    lastOpOffs(mem(-1)), stkMax(0), locals(0)
#ifdef DEBUG
    , stkSize(0)
#endif    
{
    assert(codeseg->empty());
}


CodeGen::~CodeGen()  { }


mem CodeGen::addOp(OpCode op)
{
    assert(!codeseg->closed);
    lastOpOffs = codeseg->size();
    add8(op);
    return lastOpOffs;
}


void CodeGen::addOpPtr(OpCode op, void* p)
    { addOp(op); addPtr(p); }

void CodeGen::add8(uchar i)
    { codeseg->push_back(i); }

void CodeGen::add16(uint16_t i)
    { codeseg->append(&i, 2); }

void CodeGen::addJumpOffs(joffs_t i)
    { codeseg->append(&i, sizeof(i)); }

void CodeGen::addInt(integer i)
    { codeseg->append(&i, sizeof(i)); }

void CodeGen::addPtr(void* p)
    { codeseg->append(&p, sizeof(p)); }


void CodeGen::close()
{
    if (!codeseg->empty())
        add8(opEnd);
    codeseg->close(stkMax);
}


void CodeGen::discardCode(mem from)
{
    codeseg->resize(from);
    if (lastOpOffs >= from)
        lastOpOffs = mem(-1);
}


void CodeGen::revertLastLoad()
{
    if (isLoadOp(lastOp()))
        discardCode(lastOpOffs);
    else
        // discard();
        notimpl();
}


OpCode CodeGen::lastOp()
{
    if (lastOpOffs == mem(-1))
        return opInv;
    return OpCode((*codeseg)[lastOpOffs]);
}


void CodeGen::stkPush(Type* type, const variant& value)
{
    genStack.push_back(stkinfo(type, value));
    if (genStack.size() > stkMax)
        stkMax = genStack.size();
#ifdef DEBUG
    stkSize++;
#endif
}


void CodeGen::stkPush(Type* type)
{
    genStack.push_back(stkinfo(type));
    if (genStack.size() > stkMax)
        stkMax = genStack.size();
#ifdef DEBUG
    stkSize++;
#endif
}


CodeGen::stkinfo& CodeGen::stkTop()
    { return genStack.back(); }


CodeGen::stkinfo& CodeGen::stkTop(mem i)
    { return *(genStack.rbegin() + i); }


Type* CodeGen::stkPop()
{
    Type* result = genStack.back().type;
    genStack.pop_back();
#ifdef DEBUG
    stkSize--;
#endif
    return result;
}


void CodeGen::stkReplace(Type* type)
{
    stkinfo& info = genStack.back();
    info.type = type;
    info.hasValue = false;
}


void CodeGen::stkReplace(Type* type, mem i)
{
    stkinfo& info = *(genStack.rbegin() + i);
    info.type = type;
    info.hasValue = false;
}


void CodeGen::end()
{
    close();
    assert(genStack.size() - locals == 0);
}


Type* CodeGen::getTopTypeRefValue()
{
    const stkinfo& info = stkTop();
    if (!info.type->isTypeRef() || !info.hasValue)
        return NULL;
    Type* result = CAST(Type*, info.value._obj());
    stkPop();
    revertLastLoad();
    return result;
}


void CodeGen::endConstExpr(Type* expectType)
{
    if (expectType != NULL)
        implicitCastTo(expectType, "Constant expression type mismatch");
    initRetVal(NULL);
    close();
    assert(genStack.size() == 0);
}


void CodeGen::exit()
{
    storeVar(queenBee->sresultvar);
    addOp(opExit);
}


void CodeGen::loadNullComp(Type* type)
{
    if (type == NULL)
    {
        addOp(opLoadNullComp);
        stkPush(queenBee->defNullComp);
    }
    else
        loadConst(type, (object*)NULL);
}


void CodeGen::loadConstById(mem id)
{
    if (id < 256)
    {
        addOp(opLoadConst);
        add8(uchar(id));
    }
    else if (id < 65536)
    {
        addOp(opLoadConst2);
        add16(id);
    }
    else
        throw emessage("Maximum number of constants in a block reached");
}


mem CodeGen::loadCompoundConst(Type* type, const variant& value, OpCode nullOp)
{
    if (value.empty())
    {
        addOpPtr(nullOp, type);
        return mem(-1);
    }
    else
    {
        mem id = codeseg->consts.size();
        codeseg->consts.push_back(value);
        loadConstById(id);
        return id;
    }
}


void CodeGen::loadConst(Type* type, const variant& value, bool asVariant)
{
    // NONE, BOOL, CHAR, INT, ENUM, RANGE,
    //    DICT, ARRAY, VECTOR, SET, ORDSET, FIFO, VARIANT, TYPEREF, STATE
    switch (type->getTypeId())
    {
    case Type::NONE:
        addOp(opLoadNull);
        break;
    case Type::BOOL:
        addOp(value.as_bool() ? opLoadTrue : opLoadFalse);
        break;
    case Type::CHAR:
        addOp(opLoadChar);
        add8(value.as_char());
        break;
    case Type::INT:
    case Type::ENUM:
        {
            integer v = value.as_int();
            if (v == 0)
                addOp(opLoad0);
            else if (v == 1)
                addOp(opLoad1);
            else
            {
                addOp(opLoadInt);
                addInt(v);
            }
        }
        break;
    case Type::RANGE: loadCompoundConst(type, value, opLoadNullRange); break;
    case Type::DICT: loadCompoundConst(type, value, opLoadNullDict); break;
    case Type::STR:
        if (value.as_str().empty())
            addOp(opLoadNullStr);
        else
        {
            // Detect duplicate string consts. The reason this is done here and
            // not, say, in the host module is because of const expressions that
            // don't have an execution context.
            StringMap::iterator i = stringMap.find(value._str());
            if (i == stringMap.end())
            {
                mem id = loadCompoundConst(queenBee->defStr, value, opInv);
                stringMap.insert(i, std::pair<str, mem>(value._str(), id));
            }
            else
                loadConstById(i->second);
        }
        break;
    case Type::VEC: loadCompoundConst(type, value, opLoadNullVec); break;
    case Type::ARRAY: loadCompoundConst(type, value, opLoadNullArray); break;
    case Type::ORDSET: loadCompoundConst(type, value, opLoadNullOrdset); break;
    case Type::SET: loadCompoundConst(type, value, opLoadNullSet); break;
    case Type::VARFIFO: loadCompoundConst(type, value, opLoadNullVarFifo); break;
    case Type::CHARFIFO: loadCompoundConst(type, value, opLoadNullCharFifo); break;
    case Type::NULLCOMP: addOp(opLoadNullComp); break;
    case Type::VARIANT: loadConst(queenBee->typeFromValue(value), value, true); return;
    case Type::TYPEREF:
    case Type::STATE: addOpPtr(opLoadTypeRef, value._obj()); break;
    }

    stkPush(asVariant ? queenBee->defVariant : type, value);
}


void CodeGen::loadSymbol(Symbol* symbol)
{
    if (symbol->isDefinition())
        loadDefinition(PDef(symbol));
    else if (symbol->isVariable())
        loadVar(PVar(symbol));
    else
        fatal(0x6005, "Unknown symbol type");
}


void CodeGen::loadBool(bool b)
        { loadConst(queenBee->defBool, b); }

void CodeGen::loadChar(uchar c)
        { loadConst(queenBee->defChar, c); }

void CodeGen::loadInt(integer i)
        { loadConst(queenBee->defInt, i); }

void CodeGen::loadStr(const str& s)
        { loadConst(queenBee->defStr, s); }

void CodeGen::loadTypeRef(Type* t)
        { assert(t != NULL); loadConst(defTypeRef, t); }


void CodeGen::discard()
{
    stkPop();
    addOp(opPop);
}


void CodeGen::swap()
{
    Type* t1 = stkPop();
    Type* t2 = stkPop();
    stkPush(t1);
    stkPush(t2);
    addOp(opSwap);
}


void CodeGen::dup()
{
    stkPush(stkTopType());
    addOp(opDup);
}


void CodeGen::initRetVal(Type* expectType)
{
    if (expectType != NULL)
        implicitCastTo(expectType, "Return type mismatch");
    stkPop();
    addOp(opInitRet);
    add8(0);
}


void CodeGen::initLocalVar(Variable* var)
{
    assert(var->isLocalVar());
    if (locals != genStack.size() - 1 || var->id != locals)
        _fatal(0x6003);
    locals++;
    // Local var simply remains on the stack, so just check the types
    implicitCastTo(var->type, "Variable type mismatch");
}


void CodeGen::deinitLocalVar(Variable* var)
{
    // TODO: don't generate POPs if at the end of a function: just don't call
    // deinitLocalVar()
    assert(var->isLocalVar());
    if (locals != genStack.size() || var->id != locals - 1)
        _fatal(0x6004);
    locals--;
    discard();
}


void CodeGen::initThisVar(Variable* var)
{
    assert(var->isThisVar());
    assert(var->state == state);
    assert(var->id <= 255);
    implicitCastTo(var->type, "Variable type mismatch");
    stkPop();
    addOp(opInitThis);
    add8(var->id);
}


void CodeGen::doStaticVar(ThisVar* var, OpCode op)
{
    assert(var->state != NULL && var->state != state);
    if (!var->isThisVar())
        notimpl();
    Module* module = CAST(Module*, var->state);
    addOp(op);
    addPtr(module);
    add8(var->id);
}


void CodeGen::loadStoreVar(Variable* var, bool load)
{
    if (state == NULL)
        throw emessage("Not allowed in constant expressions");
    assert(var->id <= 255);
    assert(var->state != NULL);
    // If loval or self-var
    if (var->state == state || (var->state == state->selfPtr && var->isThisVar()))
    {
        OpCode base = load ? opLoadBase : opStoreBase;
        // opXxxRet, opXxxLocal, opXxxThis, opXxxArg
        addOp(OpCode(base + (var->symbolId - Symbol::FIRSTVAR)));
        add8(var->id);
    }
    else if (var->state != state && var->isThisVar() && var->state->isModule())
        // Static from another module
        doStaticVar(var, load ? opLoadStatic : opStoreStatic);
    else
        notimpl();
}


void CodeGen::loadVar(Variable* var)
{
    loadStoreVar(var, true);
    stkPush(var->type);
}


void CodeGen::storeVar(Variable* var)
{
    implicitCastTo(var->type, "Expression type mismatch");
    stkPop();
    loadStoreVar(var, false);
}


void CodeGen::loadMember(const str& ident)
{
    const stkinfo& info = stkTop();
    if (info.type->isTypeRef())
    {
        if (info.hasValue)
        {
            Type* type = CAST(Type*, info.value._obj());
            stkPop();
            if (type->isState())
            {
                Symbol* symbol = PState(type)->findShallow(ident);
                if (type->isModule())
                {
                    revertLastLoad();
                    loadSymbol(symbol);
                }
                else
                    // TODO: inherited call for states
                    notimpl();
            }
            else
                throw emessage("Invalid member selection");
        }
        else
            // TODO: typeref variable that points to a module
            notimpl();
    }
    // TODO: state member selection
    // TODO: do dictionary element selection?
    else
        throw emessage("Invalid member selection");
}


// This is square brackets op - can be string, vector, array or dictionary
void CodeGen::loadContainerElem()
{
    Type* contType = stkTopType(1);
    if (contType->isVector())
    {
        implicitCastTo(queenBee->defInt, "Vector index must be integer");
        addOp(contType->isString() ? opLoadStrElem : opLoadVecElem);
    }
    else if (contType->isDict())
    {
        implicitCastTo(PDict(contType)->index, "Dictionary key type mismatch");
        addOp(opLoadDictElem);
    }
    else if (contType->isArray())
    {
        // TODO: check the index at compile time if possible (compile time evaluation)
        implicitCastTo(PArray(contType)->index, "Array index type mismatch");
        addOp(opLoadArrayElem);
    }
    else
        throw emessage("Vector/array/dictionary type expected");
    stkPop();
    stkReplace(CAST(Container*, contType)->elem);
}


void CodeGen::storeContainerElem(bool pop)
{
    Type* contType = stkTopType(2);

    if (contType->isString())
        throw emessage("Operation not allowed on strings");

    OpCode op = opInv;
    Type* idxType = NULL;
    if (contType->isVector())
    {
        idxType = queenBee->defInt;
        op = opStoreVecElem;
    }
    else if (contType->isArray())
    {
        idxType = PCont(contType)->index;
        op = opStoreArrayElem;
    }
    else if (contType->isDict())
    {
        idxType = PCont(contType)->index;
        op = opStoreDictElem;
    }
    else
        throw emessage("Vector/array/dictionary type expected");
        
    implicitCastTo2(idxType, "Container index type mismatch");
    implicitCastTo(PCont(contType)->elem, "Container element type mismatch");

    addOp(op);
    add8(pop);
    stkPop();
    stkPop();
    if (pop)
        stkPop();
}


void CodeGen::delDictElem()
{
    Type* dictType = stkTopType(1);
    if (!dictType->isDict())
        throw emessage("Dictionary type expected");
    implicitCastTo(PDict(dictType)->index, "Dictionary key type mismatch");
    addOp(opDelDictElem);
    stkPop();
    stkPop();
}


void CodeGen::keyInDict()
{
    Type* dictType = stkTopType();
    if (!dictType->isDict())
        throw emessage("Dictionary type expected");
    implicitCastTo2(PDict(dictType)->index, "Dictionary key type mismatch");
    addOp(opKeyInDict);
    stkPop();
    stkReplace(queenBee->defBool);
}


void CodeGen::pairToDict(Dict* dictType)
{
    implicitCastTo(dictType->elem, "Dictionary element type mismatch");
    implicitCastTo2(dictType->index, "Dictionary key type mismatch");
    addOpPtr(opPairToDict, dictType);
    stkPop();
    stkReplace(dictType);
}


void CodeGen::setOp(OpCode ordsOp, OpCode sOp, bool pop)
{
    Type* setType = stkTopType(1);
    if (setType->isOrdset())
    {
        implicitCastTo(PCont(setType)->index, "Ordinal set element type mismatch");
        addOp(ordsOp);
        add8(pop);
    }
    else if (setType->isSet())
    {
        implicitCastTo(PCont(setType)->index, "Set element type mismatch");
        addOp(sOp);
        add8(pop);
    }
    else
        throw emessage("Set type expected");
    stkPop();
    if (pop)
        stkPop();
}


void CodeGen::inSet()
{
    Type* setType = stkTopType();
    OpCode op = opInv;
    if (setType->isOrdset())
        op = opInOrdset;
    else if (setType->isSet())
        op = opInSet;
    else
        throw emessage("Set type expected");
    implicitCastTo2(PCont(setType)->index, "Set element type mismatch");
    addOp(op);
    stkPop();
    stkPop();
    stkPush(queenBee->defBool);
}


void CodeGen::elemToSet(Container* setType)
{
    Type* elemType = stkTopType();
    if (setType == NULL)
        setType = elemType->deriveSet();
    else
        implicitCastTo(setType->index, "Set element type mismatch");
    if (setType->isOrdset())
        addOpPtr(opElemToOrdset, setType);
    else if (setType->isSet())
        addOpPtr(opElemToSet, setType);
    else
        throw emessage("Set type expected");
    stkPop();
    stkPush(setType);
}


void CodeGen::canAssign(Type* from, Type* to, const char* errmsg)
{
    if (!to->isVariant() && !from->canAssignTo(to))
        throw emessage(errmsg == NULL ? "Type mismatch" : errmsg);
}


bool CodeGen::tryImplicitCastTo(Type* to, bool under)
{
    Type* from = stkTopType(int(under));
    if (from == to || from->identicalTo(to))
        ;
    else if (from->isChar() && to->isString())
    {
        addOp(under ? opChrToStr2 : opChrToStr);
        stkReplace(to, int(under));
    }
    else if (from->canAssignTo(to) || to->isVariant())
        stkReplace(to, int(under));
    else if (from->isNullComp() && to->isCompound())
    {
        if (under)
            throw emessage("Type of null compound is undefined");
        stkPop();
        revertLastLoad();
        loadConst(to, (object*)NULL);
    }
    // TODO: container to fifo
    else
        return false;
    return true;
}


void CodeGen::implicitCastTo(Type* to, const char* errmsg)
{
    if (!tryImplicitCastTo(to, false))
        throw emessage(errmsg == NULL ? "Type mismatch" : errmsg);
}


void CodeGen::implicitCastTo2(Type* to, const char* errmsg)
{
    if (!tryImplicitCastTo(to, true))
        throw emessage(errmsg == NULL ? "Left operand type mismatch" : errmsg);
}


void CodeGen::explicitCastTo(Type* to, const char* errmsg)
{
    Type* from = stkTopType();
    if (tryImplicitCastTo(to, false))
        return;
    else if (to->isBool())
    {
        addOp(opToBool);
        stkReplace(queenBee->defBool);
    }
    else if (to->isString())
    {
        // explicit cast to string: any object goes
        addOp(from->isChar() ? opChrToStr : opToStr);
        stkReplace(queenBee->defStr);
    }
    else if (
        // Variants should be typecast'ed to other types explicitly
        from->isVariant()
        // Ordinals must be casted at runtime so that the variant type of the
        // value on the stack is correct for subsequent operations.
        || (from->isOrdinal() && to->isOrdinal())
        // States: implicit type cast wasn't possible, so try run-time typecast
        || (from->isState() && to->isState()))
    {
            addOpPtr(opToType, to);    // calls to->runtimeTypecast(v)
            stkReplace(to);
    }
}


void CodeGen::toBool()
{
    stkPop();
    addOp(opToBool);
    stkPush(queenBee->defBool);
}


void CodeGen::dynamicCast()
{
    Type* typeref = stkPop();
    stkPop();
    if (!typeref->isTypeRef())
        throw emessage("Typeref expected in dynamic typecast");
    addOp(opToTypeRef);
    stkPush(queenBee->defVariant);
}


void CodeGen::testType(Type* type)
{
    Type* varType = stkPop();
    if (varType->isVariant())
        addOpPtr(opIsType, type);
    else
    {
        // Can be determined at compile time
        revertLastLoad();
        addOp(varType->canAssignTo(type) ? opLoadTrue : opLoadFalse);
    }
    stkPush(queenBee->defBool);
}


void CodeGen::testType()
{
    Type* typeref = stkPop();
    stkPop();
    if (!typeref->isTypeRef())
        throw emessage("Typeref expected in dynamic typecast");
    addOp(opIsTypeRef);
    stkPush(queenBee->defBool);
}


void CodeGen::arithmBinary(OpCode op)
{
    assert(op >= opAdd && op <= opBitShr);
    Type* type = stkPop();
    if (!type->isInt() || !stkTopType()->isInt())
        throw emessage("Operand types do not match operator");
    addOp(op);
}


void CodeGen::arithmUnary(OpCode op)
{
    assert(op >= opNeg && op <= opNot);
    if (!stkTopType()->isInt())
        throw emessage("Operand type doesn't match operator");
    addOp(op);
}


void CodeGen::_not()
{
    Type* type = stkTopType();
    if (type->isInt())
        addOp(opBitNot);
    else
    {
        implicitCastTo(queenBee->defBool, "Boolean or integer operand expected");
        addOp(opNot);
    }
}


void CodeGen::boolXor()
{
    Type* type = stkPop();
    if (!type->isBool() || !stkTopType()->isBool())
        throw emessage("Operand types do not match operator");
    addOp(opBoolXor);
}


void CodeGen::elemToVec(Vec* vecType)
{
    Type* elemType = stkTopType();
    if (vecType == NULL)
        vecType = elemType->deriveVector();
    else
        implicitCastTo(vecType->elem, "Vector element type mismatch");
    if (elemType->isChar())
        addOp(opChrToStr);
    else
        addOpPtr(opVarToVec, vecType);
    stkPop();
    stkPush(vecType);
}


void CodeGen::elemCat()
{
    Type* elemType = stkTopType();
    Type* vecType = stkTopType(1);
    if (!vecType->isVector())
        throw emessage("Vector/string type expected");
    implicitCastTo(CAST(Vec*, vecType)->elem, "Vector/string element type mismatch");
    elemType = stkPop();
    addOp(elemType->isChar() ? opCharCat: opVarCat);
}


void CodeGen::cat()
{
    Type* left = stkTopType(1);
    if (!left->isVector())
        throw emessage("Left operand is not a vector");
    implicitCastTo(left, "Vector/string types do not match");
    stkPop();
    addOp(left->isString() ? opStrCat : opVecCat);
}


void CodeGen::mkRange(Range* rangeType)
{
    // TODO: calculate at compile time
    Type* left = stkTopType(1);
    if (rangeType == NULL)
    {
        if (!left->isOrdinal())
            throw emessage("Range boundaries must be ordinal");
        rangeType = POrdinal(left)->deriveRange();
    }
    else
        implicitCastTo2(rangeType->base, "Range element type mismatch");
    implicitCastTo(rangeType->base, "Range element type mismatch");
    addOpPtr(opMkRange, rangeType);
    stkPop();
    stkReplace(rangeType);
}


void CodeGen::inRange()
{
    Type* rangeType = stkTopType();
    if (!rangeType->isRange())
        throw emessage("Range type expected");
    implicitCastTo2(PRange(rangeType)->base, "Range element types do not match");
    addOp(opInRange);
    stkPop();
    stkReplace(queenBee->defBool);
}


void CodeGen::inBounds()
{
    // NOTE: the compiler should check compatibility of the left boundary
    Type* left = stkTopType(1);
    if (!left->isOrdinal())
        throw emessage("Ordinal type expected in range");
    implicitCastTo(left, "Right boundary type mismatch");
    addOp(opInBounds);
    stkPop();
    stkPop();
    stkReplace(queenBee->defBool);
}


void CodeGen::cmp(OpCode op)
{
    assert(isCmpOp(op));
    Type* left = stkTopType(1);
    implicitCastTo(left, "Type mismatch in comparison");
    Type* right = stkTopType();
    if (left->isOrdinal() && right->isOrdinal())
        addOp(opCmpOrd);
    else if (left->isString() && right->isString())
        addOp(opCmpStr);
    else
    {
        // Only == and != are allowed for all other types
        if (op != opEqual && op != opNotEq)
            throw emessage("Only equality can be tested for this type");
        addOp(opCmpVar);
    }
    addOp(op);
    stkPop();
    stkReplace(queenBee->defBool);
}


void CodeGen::empty()
{
    Type* type = stkPop();
    if (type->isNone())
    {
        revertLastLoad();
        loadBool(true);
    }
    else
        addOp(opEmpty);
    stkPush(queenBee->defBool);
}


void CodeGen::count()
{
    Type* type = stkPop();
    if (type->isNone())
    {
        revertLastLoad();
        loadInt(0);
    }
    else if (type->isOrdinal())
    {
        revertLastLoad();
        // May overflow and yield a negative number, but there is nothing we can do
        loadInt(POrdinal(type)->right - POrdinal(type)->left + 1);
    }
    else if (type->isRange())
        addOp(opRangeDiff);
    else if (type->isString())
        addOp(opStrLen);
    else if (type->isVector())
        addOp(opVecLen);
    else
        throw emessage("Operation not available for this type");
    stkPush(queenBee->defInt);
}


void CodeGen::lowHigh(bool high)
{
    Type* type = stkPop();
    if (type->isOrdinal())
    {
        revertLastLoad();
        loadInt(high ? POrdinal(type)->right : POrdinal(type)->left);
    }
    else if (type->isRange())
        addOp(high ? opRangeHigh : opRangeLow);
    else
        throw emessage("Operation not available for this type");
    stkPush(queenBee->defInt);
}


void CodeGen::jump(mem target)
{
    assert(target < getCurPos());
    integer offs = integer(target) - integer(getCurPos() + 1 + sizeof(joffs_t));
    if (offs < -32768)
        throw emessage("Jump target is too far away");
    addOp(opJump);
    addJumpOffs(offs);
}


mem CodeGen::boolJumpForward(OpCode op)
{
    assert(isBoolJump(op));
    implicitCastTo(queenBee->defBool, "Boolean expression expected");
    stkPop();
    return jumpForward(op);
}


mem CodeGen::jumpForward(OpCode op)
{
    assert(isJump(op));
    mem pos = getCurPos();
    addOp(op);
    addJumpOffs(0);
    return pos;
}


void CodeGen::resolveJump(mem jumpOffs)
{
    assert(jumpOffs <= getCurPos() - 1 - sizeof(joffs_t));
    assert(isJump(OpCode((*codeseg)[jumpOffs])));
    integer offs = integer(getCurPos()) - integer(jumpOffs + 1 + sizeof(joffs_t));
    if (offs > 32767)
        throw emessage("Jump target is too far away");
    codeseg->putJumpOffs(jumpOffs + 1, offs);
}


void CodeGen::caseLabel(Type* labelType, const variant& label)
{
    // TODO: the compiler should do type checking and typecasts for labels
    // itself, as we can't do it here (it's not on the stack).
    Type* caseType = stkTopType();
    if (labelType->isRange())
    {
        canAssign(caseType, CAST(Range*, labelType)->base, "Case label type mismatch");
        range* r = CAST(range*, label._obj());
        addOp(opCaseRange);
        addInt(r->left);
        addInt(r->right);
    }
    else
    {
        canAssign(caseType, labelType, "Case label type mismatch");
        if (labelType->isChar() && caseType->isString())
        {
            loadStr(label.to_string());
            addOp(opCaseStr);
            stkPop();
        }
        if (labelType->isOrdinal())
        {
            addOp(opCaseInt);
            addInt(label._ord());
        }
        else if (labelType->isString())
        {
            loadStr(label._str());
            addOp(opCaseStr);
            stkPop();
        }
        else if (labelType->isTypeRef())
        {
            loadTypeRef(CAST(Type*, label._obj()));
            addOp(opCaseTypeRef);
            stkPop();
        }
        else
            throw emessage("Only ordinal, string and typeref constants are allowed in case statement");
    }
    stkPush(queenBee->defBool);
}


void CodeGen::assertion()
{
    implicitCastTo(queenBee->defBool, "Boolean expression expected");
    stkPop();
    addOp(opAssert);
}


void CodeGen::linenum(integer file, integer line)
{
    if (file > 65535)
        throw emessage("Too many files... this is madness!");
    if (line > 65535)
        throw emessage("Line number too big... testing me? Huh?!");
    if (lastOp() == opLineNum)
        discardCode(lastOpOffs);
    addOp(opLineNum);
    add16(file);
    add16(line);
}


// --- BLOCK SCOPE --------------------------------------------------------- //


// In principle this belongs to typesys.cpp but defined here because it uses
// the codegen object for managing local vars.


BlockScope::BlockScope(Scope* _outer, CodeGen* _gen)
    : Scope(_outer), startId(_gen->getLocals()), gen(_gen)  { }


BlockScope::~BlockScope()  { }


void BlockScope::deinitLocals()
{
    mem i = localvars.size();
    while (i--)
        gen->deinitLocalVar(localvars[i]);
}


Variable* BlockScope::addLocalVar(Type* type, const str& name)
{
    assert(gen->getState() != NULL);
    mem id = startId + localvars.size();
    if (id >= 255)
        throw emessage("Maximum number of local variables within this scope is reached");
    objptr<Variable> v = new LocalVar(Symbol::LOCALVAR, type, name, id, gen->getState(), false);
    addUnique(v);   // may throw
    localvars.add(v);
    return v;
}

