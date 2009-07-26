

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
    if (lastOpOffs != mem(-1) && isLoadOp(OpCode((*codeseg)[lastOpOffs])))
        discardCode(lastOpOffs);
    else
        // discard();
        notimpl();
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


const CodeGen::stkinfo& CodeGen::stkTop() const
    { return genStack.back(); }


const CodeGen::stkinfo& CodeGen::stkTop(mem i) const
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
    genStack.back().type = type;
}


void CodeGen::end()
{
    close();
    assert(genStack.size() - locals == 0);
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


void CodeGen::loadNullContainer(Container* contType)
{
    loadConst(contType, (object*)NULL);
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


mem CodeGen::loadCompoundConst(const variant& value)
{
    mem id = codeseg->consts.size();
    codeseg->consts.push_back(value);
    loadConstById(id);
    return id;
}


void CodeGen::loadConst(Type* type, const variant& value, bool asVariant)
{
    // NONE, BOOL, CHAR, INT, ENUM, RANGE,
    //    DICT, ARRAY, VECTOR, SET, ORDSET, FIFO, VARIANT, TYPEREF, STATE
    bool isEmpty = value.empty();

    switch (type->getTypeId())
    {
    case Type::NONE:
        addOp(opLoadNull);
        break;
    case Type::BOOL:
        addOp(isEmpty ? opLoadFalse : opLoadTrue);
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
    case Type::RANGE:
        if (isEmpty) addOpPtr(opLoadNullRange, type);
        else loadCompoundConst(value);
        break;
    case Type::DICT:
        if (isEmpty) addOpPtr(opLoadNullDict, type);
        else loadCompoundConst(value);
        break;
    case Type::STR:
        if (isEmpty)
            addOp(opLoadNullStr);
        else
        {
            // Detect duplicate string consts. The reason this is done here and
            // not, say, in the host module is because of const expressions that
            // don't have an execution context.
            StringMap::iterator i = stringMap.find(value._str_read());
            if (i == stringMap.end())
            {
                mem id = loadCompoundConst(value);
                stringMap.insert(i, std::pair<str, mem>(value._str_read(), id));
            }
            else
                loadConstById(i->second);
        }
        break;
    case Type::VEC:
        if (isEmpty) addOpPtr(opLoadNullVec, type);
        else loadCompoundConst(value);
        break;
    case Type::ARRAY:
        if (isEmpty) addOpPtr(opLoadNullArray, type);
        else loadCompoundConst(value);
        break;
    case Type::ORDSET:
        if (isEmpty) addOpPtr(opLoadNullOrdset, type);
        else loadCompoundConst(value);
        break;
    case Type::SET:
        if (isEmpty) addOpPtr(opLoadNullSet, type);
        else loadCompoundConst(value);
        break;
    case Type::VARFIFO:
    case Type::CHARFIFO:
        _fatal(0x6002);
        break;
    case Type::VARIANT:
        loadConst(queenBee->typeFromValue(value), value, true);
        return;
    case Type::TYPEREF:
    case Type::STATE:
        addOpPtr(opLoadTypeRef, value.as_object());
        break;
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
    implicitCastTo(var->type, "Expression type mismatch");
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
    Type* type = stkTopType();
    if (type->isTypeRef())
    {
        type = CAST(Type*, stkTopValue()._object());
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
    // TODO: do dictionary element selection?
    else
        throw emessage("Invalid member selection");
}


// This is square brackets op - can be string, vector, array or dictionary
void CodeGen::loadContainerElem()
{
    Type* idxType = stkPop();
    Type* contType = stkPop();
    if (contType->isVector())
    {
        if (!idxType->isInt())
            throw emessage("Vector/string index must be integer");
        idxType = queenBee->defNone;
        addOp(contType->isString() ? opLoadStrElem : opLoadVecElem);
    }
    else if (contType->isDict())
        addOp(opLoadDictElem);
    else if (contType->isArray())
        // TODO: check the index at compile time if possible
        addOp(opLoadArrayElem);
    else
        throw emessage("Container/string type expected");
    typeCast(idxType, CAST(Container*, contType)->index, "Container index type mismatch");
    stkPush(CAST(Container*, contType)->elem);
}


void CodeGen::storeContainerElem()
{
    Type* elemType = stkPop();
    Type* idxType = stkPop();
    Type* contType = stkPop();
    if (contType->isString())
        throw emessage("Operation not allowed on strings");
    else if (contType->isVector())
    {
        idxType = queenBee->defNone;
        addOp(opStoreVecElem);
    }
    else if (contType->isArray())
        addOp(opStoreArrayElem);
    else if (contType->isDict())
        addOp(opStoreDictElem);
    else
        throw emessage("Container type expected");
    typeCast(idxType, CAST(Container*, contType)->index, "Container key type mismatch");
    typeCast(elemType, CAST(Container*, contType)->elem, "Container element type mismatch");
}


void CodeGen::delDictElem()
{
    Type* idxType = stkPop();
    Type* dictType = stkPop();
    if (!dictType->isDict())
        throw emessage("Dictionary type expected");
    typeCast(idxType, CAST(Dict*, dictType)->index, "Dictionary key type mismatch");
    addOp(opDelDictElem);
}


void CodeGen::addToSet()
{
    Type* idxType = stkPop();
    Type* setType = stkPop();
    if (setType->isOrdset())
        addOp(opAddToOrdset);
    else if (setType->isSet())
        addOp(opAddToSet);
    else
        throw emessage("Set type expected");
    typeCast(idxType, CAST(Set*, setType)->index, "Set element type mismatch");
}


void CodeGen::inSet()
{
    // Operator `in`: elem in set
    Type* setType = stkPop();
    Type* idxType = stkPop();
    if (setType->isOrdset())
        addOp(opInOrdset);
    else if (setType->isSet())
        addOp(opInSet);
    else
        throw emessage("Set type expected");
    typeCast(idxType, CAST(Set*, setType)->index, "Set element type mismatch");
    stkPush(queenBee->defBool);
}


void CodeGen::inDictKeys()
{
    // Operator `in`: key in dict
    Type* dictType = stkPop();
    Type* idxType = stkPop();
    if (!dictType->isDict())
        throw emessage("Dictionary type expected");
    typeCast(idxType, CAST(Dict*, dictType)->index, "Dictionary key type mismatch");
    addOp(opInDictKeys);
    stkPush(queenBee->defBool);
}


void CodeGen::typeCast(Type* from, Type* to, const char* errmsg)
{
    if (!to->isVariant() && !from->canAssignTo(to))
        throw emessage(errmsg == NULL ? "Type mismatch" : errmsg);
}


void CodeGen::implicitCastTo(Type* to, const char* errmsg)
{
    typeCast(stkTopType(), to, errmsg);
    stkReplace(to);
}


void CodeGen::explicitCastTo(Type* to)
{
    Type* from = stkTopType();

    // Try implicit cast first
    if (to->isVariant() || from->canAssignTo(to))
    {
        stkReplace(to);
        return;
    }
    stkPop();
    if (to->isBool())
        addOp(opToBool);    // everything can be cast to bool
    else if (to->isString())
        addOp(opToStr); // explicit cast to string: any object goes
    else if (
        // Variants should be typecast'ed to other types explicitly
        from->isVariant()
        // Ordinals must be casted at runtime so that the variant type of the
        // value on the stack is correct for subsequent operations.
        || (from->isOrdinal() && to->isOrdinal())
        // States: implicit type cast wasn't possible, so try run-time typecast
        || (from->isState() && to->isState()))
            addOpPtr(opToType, to);    // calls to->runtimeTypecast(v)
    else
        throw emessage("Invalid typecast");
    stkPush(to);
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


void CodeGen::elemToVec()
{
    Type* elemType = stkPop();
    Type* vecType = elemType->deriveVector();
    if (elemType->isChar())
        addOp(opCharToStr);
    else
        addOpPtr(opVarToVec, vecType);
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
    Type* right = stkPop();
    Type* left = stkTopType();
    if (!left->isVector() || !right->isVector())
        throw emessage("Vector/string type expected");
    if (!right->canAssignTo(left))
        throw emessage("Vector/string types do not match");
    addOp(left->isString() ? opStrCat : opVecCat);
}


void CodeGen::mkRange()
{
    Type* right = stkPop();
    Type* left = stkPop();
    if (!left->isOrdinal() || !right->isOrdinal())
        throw emessage("Range elements must be ordinal");
    if (!right->canAssignTo(left))
        throw emessage("Range element types do not match");
    Range* rangeType = POrdinal(left)->deriveRange();
    addOpPtr(opMkRange, rangeType);
    stkPush(rangeType);
}


void CodeGen::inRange()
{
    Type* rng = stkPop();
    Type* elem = stkPop();
    if (!rng->isRange())
        throw emessage("Range type expected");
    if (!elem->canAssignTo(PRange(rng)->base))
        throw emessage("Range element type mismatch");
    addOp(opInRange);
    stkPush(queenBee->defBool);
}


void CodeGen::cmp(OpCode op)
{
    assert(isCmpOp(op));
    Type* right = stkPop();
    Type* left = stkPop();
    if (!right->canAssignTo(left))
        throw emessage("Types mismatch in comparison");
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
    stkPush(queenBee->defBool);
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
    Type* caseType = stkTopType();
    if (labelType->isRange())
    {
        typeCast(caseType, CAST(Range*, labelType)->base, "Case label type mismatch");
        addOp(opCaseRange);
        range* r = CAST(range*, label._object());
        addInt(r->left);
        addInt(r->right);
    }
    else
    {
        typeCast(caseType, labelType, "Case label type mismatch");
        if (labelType->isOrdinal())
        {
            addOp(opCaseInt);
            addInt(label._ord());
        }
        else if (labelType->isString())
        {
            loadStr(label._str_read());
            addOp(opCaseStr);
            stkPop();
        }
        else if (labelType->isTypeRef())
        {
            loadTypeRef(CAST(Type*, label._object()));
            addOp(opCaseTypeRef);
            stkPop();
        }
        else
            throw emessage("Only ordinals, strings and typerefs are allowed in case statement");
    }
    stkPush(queenBee->defBool);
}


void CodeGen::assertion(integer file, integer line)
{
    linenum(file, line);
    implicitCastTo(queenBee->defBool, "Boolean expression expected");
    stkPop();
    addOp(opAssert);
}


void CodeGen::linenum(integer file, integer line)
{
    if (file > 65535)
        throw emessage("Too many files... testing me? Huh?");
    if (line > 65535)
        throw emessage("Line number too big");
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

