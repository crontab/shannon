
#include "vm.h"


// TODO: store the current file name in a named const, say __FILE__


CodeGen::CodeGen(CodeSeg& c, State* treg, bool compileTime)
    : codeOwner(c.getStateType()), typeReg(treg), codeseg(c), locals(0),
      lastOp(opInv), prevLoaderOffs(-1)
{
    assert(treg != NULL);
    if (compileTime != (codeOwner == NULL))
        fatal(0x6003, "CodeGen: invalid codeOwner");
}


CodeGen::~CodeGen()
    { }


void CodeGen::error(const char* msg)
    { throw emessage(msg); }


void CodeGen::error(const str& msg)
    { throw emessage(msg); }


void CodeGen::addOp(Type* type, OpCode op)
{
    simStack.push_back(SimStackItem(type, getCurrentOffs()));
    if (simStack.size() > codeseg.stackSize)
        codeseg.stackSize = simStack.size();
    addOp(op);
}


void CodeGen::undoDesignator(memint from)
{
    codeseg.erase(from);
    stkPop();
    prevLoaderOffs = -1;
}


void CodeGen::undoLoader()
{
    memint offs = stkTopItem().offs;
    if (!isUndoableLoadOp(codeseg[offs]))
        error("Invalid type cast");
    undoDesignator(offs);
}


Type* CodeGen::stkPop()
{
    const SimStackItem& s = simStack.back();
    prevLoaderOffs = s.offs;
    Type* result = s.type;
    simStack.pop_back();
    return result;
}


void CodeGen::stkReplaceTop(Type* t)
{
    memint offs = stkTopItem().offs;
    simStack.pop_back();
    simStack.push_back(SimStackItem(t, offs));
}


bool CodeGen::tryImplicitCast(Type* to)
{
    Type* from = stkTop();

    if (from == to)
        return true;

    if (to->isVariant() || from->canAssignTo(to))
    {
        // canAssignTo() should take care of polymorphic typecasts
        stkReplaceTop(to);
        return true;
    }

    if (!to->isReference() && from->isReference())
    {
        deref();
        return tryImplicitCast(to);
    }

    // Vector elements are automatically converted to vectors when necessary,
    // e.g. char -> str
    if (to->isAnyVec() && from->identicalTo(PContainer(to)->elem))
    {
        elemToVec();
        return true;
    }

    if (from->isNullCont() && to->isAnyCont())
    {
        undoLoader();
        loadEmptyCont(PContainer(to));
        return true;
    }

    return false;
}


void CodeGen::implicitCast(Type* to, const char* errmsg)
{
    // TODO: better error message, something like <type> expected; use Type::dump()
    if (!tryImplicitCast(to))
        error(errmsg == NULL ? "Type mismatch" : errmsg);
}


void CodeGen::explicitCast(Type* to)
{
    if (tryImplicitCast(to))
        return;

    Type* from = stkTop();

    if (from->isAnyOrd() && to->isAnyOrd())
        stkReplaceTop(to);

    else if (from->isVariant())
    {
        stkPop();
        addOp<Type*>(to, opCast, to);
    }

    // TODO: better error message with type defs
    else
        error("Invalid explicit typecast");
}


void CodeGen::isType(Type* to, bool isnot, memint undoOffs)
{
    Type* from = stkTop();
    if (from->canAssignTo(to))
    {
        undoDesignator(undoOffs);
        loadConst(queenBee->defBool, int(!isnot));
    }
    else if (from->isAnyState() || from->isVariant())
    {
        stkPop();
        addOp<Type*>(queenBee->defBool, opIsType, to);
        if (isnot)
            _not();
    }
    else
    {
        undoDesignator(undoOffs);
        loadConst(queenBee->defBool, int(isnot));
    }
}


void CodeGen::createSubrangeType()
{
    Type* left = stkTop(2);
    if (!left->isAnyOrd())
        error("Non-ordinal range bounds");
    implicitCast(left, "Incompatible subrange bounds");
    stkPop();
    stkPop();
    addOp<Ordinal*>(defTypeRef, opMkSubrange, POrdinal(left));
}


void CodeGen::deinitLocalVar(Variable* var)
{
    // This is called from BlockScope.
    // TODO: don't generate POPs if at the end of a function in RELEASE mode
    assert(var->isLocalVar());
    assert(locals == simStack.size());
    if (var->id != locals - 1)
        fatal(0x6002, "Invalid local var id");
    locals--;
    popValue();
}


void CodeGen::popValue()
{
    stkPop();
    addOp(opPop);
}


Type* CodeGen::tryUndoTypeRef()
{
    memint offs = stkTopItem().offs;
    if (codeseg[offs] == opLoadTypeRef)
    {
        Type* type = codeseg.at<Type*>(offs + 1);
        stkPop();
        codeseg.erase(offs);
        return type;
    }
    else
        return NULL;
}


bool CodeGen::deref()
{
    Type* type = stkTop();
    if (type->isReference())
    {
        type = type->getValueType();
        if (type->isDerefable())
        {
            stkPop();
            addOp(type, opDeref);
        }
        else
            notimpl();
        return true;
    }
    return false;
}


void CodeGen::mkref()
{
    Type* type = stkTop();
    if (!type->isReference())
    {
        if (type->isDerefable())
        {
            stkPop();
            addOp(type->getRefType(), opMkRef);
        }
        else
            error("Can't convert to reference");
    }
}


void CodeGen::nonEmpty()
{
    Type* type = stkTop();
    if (!type->isBool())
    {
        stkPop();
        addOp(queenBee->defBool, opNonEmpty);
    }
}


void CodeGen::loadTypeRef(Type* type)
{
    addOp<Type*>(defTypeRef, opLoadTypeRef, type);
}


void CodeGen::loadConst(Type* type, const variant& value)
{
    // NOTE: compound consts should be held by a smart pointer somewhere else
    switch(value.getType())
    {
    case variant::VOID:
        addOp(type, opLoadNull);
        return;
    case variant::ORD:
        {
            assert(type->isAnyOrd());
            integer i = value._int();
            if (i == 0)
                addOp(type, opLoad0);
            else if (i == 1)
                addOp(type, opLoad1);
            else if (uinteger(i) <= 255)
                addOp<uchar>(type, opLoadByte, i);
            else
                addOp<integer>(type, opLoadOrd, i);
        }
        return;
    case variant::REAL:
        notimpl();
        break;
    case variant::VARPTR:
        break;    
    case variant::STR:
        assert(type->isByteVec());
        addOp<object*>(type, opLoadStr, value._str().obj);
        return;
    case variant::VEC:
    case variant::SET:
    case variant::ORDSET:
    case variant::DICT:
    case variant::REF:
        break;
    case variant::RTOBJ:
        if (value._rtobj()->getType()->isTypeRef())
        {
            loadTypeRef(cast<Type*>(value._rtobj()));
            return;
        }
        break;
    }
    fatal(0x6001, "Unknown constant literal");
}


void CodeGen::loadDefinition(Definition* def)
{
    Type* type = def->type;
    if (type->isTypeRef() || type->isVoid() || def->type->isAnyOrd() || def->type->isByteVec())
        loadConst(def->type, def->value);
    else
        addOp<Definition*>(def->type, opLoadConst, def);
}


static variant::Type typeToVarType(Container* t)
{
    // Currently only works for containers
    switch (t->typeId)
    {
    case Type::NULLCONT:
        return variant::VOID;
    case Type::VEC:
        return t->isByteVec() ? variant::STR : variant::VEC;
    case Type::SET:
        return t->isByteSet() ? variant::ORDSET : variant::SET;
    case Type::DICT:
        return t->isByteDict() ? variant::VEC : variant::DICT;
    default:
        notimpl();
        return variant::VOID;
    }
}


void CodeGen::loadEmptyCont(Container* contType)
    { addOp<char>(contType, opLoadEmptyVar, typeToVarType(contType)); }


void CodeGen::loadSymbol(Symbol* sym)
{
    if (sym->isDefinition())
        loadDefinition(PDefinition(sym));
    else if (sym->isAnyVar())
        loadVariable(PVariable(sym));
    else
        notimpl();
}


void CodeGen::loadVariable(Variable* var)
{
    // TODO: check outer states too!
    assert(var->host != NULL);
    if (isCompileTime())
        error("Variables not allowed in constant expressions");
    if (var->isSelfVar() && var->host == codeOwner->selfPtr)
    {
        assert(var->id >= 0 && var->id <= 255);
        addOp<uchar>(var->type, opLoadSelfVar, var->id);
    }
    else if (var->isLocalVar() && var->host == codeOwner)
    {
        assert(var->id >= -128 && var->id <= 127);
        addOp<char>(var->type, opLoadStkVar, var->id);
    }
    else
        notimpl();
}


void CodeGen::loadMember(const str& ident, memint undoOffs)
{
    Type* stateType = stkTop();
    if (!stateType->isAnyState())
        error("Invalid member selection");
    loadMember(PState(stateType)->findShallow(ident), undoOffs);
}


void CodeGen::loadMember(Symbol* sym, memint undoOffs)
{
    Type* stateType = stkTop();
    if (!stateType->isAnyState())
        error("Invalid member selection");
    if (sym->isAnyVar())
        loadMember(PVariable(sym));
    else if (sym->isDefinition())
    {
        undoDesignator(undoOffs);
        loadDefinition(PDefinition(sym));
    }
    else
        notimpl();
}


void CodeGen::loadMember(Variable* var)
{
    if (isCompileTime())
        error("Variables not allowed in constant expressions");
    Type* stateType = stkPop();
    // TODO: check parent states too
    if (!stateType->isAnyState() || var->host != stateType
            || !var->isSelfVar())
        error("Invalid member selection");
    assert(var->id >= 0 && var->id <= 255);
    addOp<uchar>(var->type, opLoadMember, var->id);
}


void CodeGen::storeRet(Type* type)
{
    implicitCast(type);
    stkPop();
    addOp<char>(opInitStkVar, isCompileTime() ? -1 : codeOwner->prototype->retVarId());
}


void CodeGen::initLocalVar(LocalVar* var)
{
    // Local var simply remains on the stack, so just check the types.
    assert(var->id >= 0 && var->id <= 127);
    if (locals != simStack.size() - 1 || var->id != locals)
        fatal(0x6004, "initLocalVar(): invalid var id");
    locals++;
    implicitCast(var->type, "Variable type mismatch");
}


void CodeGen::initSelfVar(SelfVar* var)
{
    if (var->host != codeOwner)
        fatal(0x6005, "initSelfVar(): not my var");
    implicitCast(var->type, "Variable type mismatch");
    stkPop();
    assert(var->id >= 0 && var->id <= 255);
    addOp<uchar>(opInitSelfVar, var->id);
}


void CodeGen::loadContainerElem()
{
    // This is square brackets op - can be string, vector, array or dictionary.
    OpCode op = opInv;
    Type* contType = stkTop(2);
    if (contType->isAnyVec())
    {
        implicitCast(queenBee->defInt, "Vector index must be integer");
        op = contType->isByteVec() ? opStrElem : opVecElem;
    }
    else if (contType->isAnyDict())
    {
        implicitCast(PContainer(contType)->index, "Dictionary key type mismatch");
        op = contType->isByteDict() ? opByteDictElem : opDictElem;
    }
    else
        error("Vector/dictionary type expected");
    stkPop();
    stkPop();
    addOp(PContainer(contType)->elem, op);
}


void CodeGen::length()
{
    Type* type = stkTop();
    if (type->isNullCont())
    {
        undoLoader();
        loadConst(queenBee->defInt, 0);
    }
    else if (type->isAnyVec())
    {
        stkPop();
        addOp(queenBee->defInt, type->isByteVec() ? opStrLen : opVecLen);
    }
    else
        error("'#' expects vector or string");
}


Container* CodeGen::elemToVec()
{
    Type* elemType = stkTop();
    Container* vecType = elemType->deriveVec(typeReg);
    stkPop();
    addOp(vecType, vecType->isByteVec() ? opChrToStr : opVarToVec);
    return vecType;
}


void CodeGen::elemCat()
{
    Type* vecType = stkTop(2);
    if (!vecType->isAnyVec())
        error("Vector/string type expected");
    implicitCast(PContainer(vecType)->elem, "Vector/string element type mismatch");
    stkPop();
    addOp(vecType->isByteVec() ? opChrCat: opVarCat);
}


void CodeGen::cat()
{
    Type* vecType = stkTop(2);
    if (!vecType->isAnyVec())
        error("Left operand is not a vector");
    implicitCast(vecType, "Vector/string types do not match");
    stkPop();
    addOp(vecType->isByteVec() ? opStrCat : opVecCat);
}


Container* CodeGen::elemToSet()
{
    Type* elemType = stkTop();
    Container* setType = elemType->deriveSet(typeReg);
    stkPop();
    addOp(setType, setType->isByteSet() ? opElemToByteSet : opElemToSet);
    return setType;
}


Container* CodeGen::rangeToSet()
{
    Type* left = stkTop(2);
    if (!left->isAnyOrd())
        error("Non-ordinal range bounds");
    if (!left->canAssignTo(stkTop()))
        error("Incompatible range bounds");
    Container* setType = left->deriveSet(typeReg);
    if (!setType->isByteSet())
        error("Invalid element type for ordinal set");
    stkPop();
    stkPop();
    addOp(setType, opRngToByteSet);
    return setType;
}


void CodeGen::setAddElem()
{
    Type* setType = stkTop(2);
    if (!setType->isAnySet())
        error("Set type expected");
    implicitCast(PContainer(setType)->index, "Set element type mismatch");
    stkPop();
    addOp(setType->isByteSet() ? opByteSetAddElem : opSetAddElem);
}


void CodeGen::checkRangeLeft()
{
    Type* setType = stkTop(2);
    if (!setType->isByteSet())
        error("Byte set type expected");
    implicitCast(PContainer(setType)->index, "Set element type mismatch");
}


void CodeGen::setAddRange()
{
    Type* setType = stkTop(3);
    if (!setType->isByteSet())
        error("Byte set type expected");
    implicitCast(PContainer(setType)->index, "Set element type mismatch");
    stkPop();
    stkPop();
    addOp(opByteSetAddRng);
}


Container* CodeGen::pairToDict()
{
    Type* val = stkTop();
    Type* key = stkTop(2);
    Container* dictType = val->deriveContainer(typeReg, key);
    stkPop();
    stkPop();
    addOp(dictType, dictType->isByteDict() ? opPairToByteDict : opPairToDict);
    return dictType;
}


void CodeGen::checkDictKey()
{
    Type* dictType = stkTop(2);
    if (!dictType->isAnyDict())
        error("Dictionary type expected");
    implicitCast(PContainer(dictType)->index, "Dictionary key type mismatch");
}


void CodeGen::dictAddPair()
{
    Type* dictType = stkTop(3);
    if (!dictType->isAnyDict())
        error("Dictionary type expected");
    implicitCast(PContainer(dictType)->elem, "Dictionary element type mismatch");
    stkPop();
    stkPop();
    addOp(dictType->isByteDict() ? opByteDictAddPair : opDictAddPair);
}


void CodeGen::arithmBinary(OpCode op)
{
    assert(op >= opAdd && op <= opBitShr);
    Type* right = stkPop();
    Type* left = stkPop();
    if (!right->isInt() || !left->isInt())
        error("Operand types do not match binary operator");
    addOp(left->identicalTo(right) ? left : queenBee->defInt, op);
}


void CodeGen::arithmUnary(OpCode op)
{
    assert(op >= opNeg && op <= opNot);
    Type* type = stkTop();
    if (!type->isInt())
        error("Operand type doesn't match unary operator");
    addOp(op);
}


void CodeGen::cmp(OpCode op)
{
    assert(isCmpOp(op));
    Type* left = stkTop(2);
    implicitCast(left, "Type mismatch in comparison");
    Type* right = stkTop();
    if (left->isAnyOrd() && right->isAnyOrd())
        addOp(opCmpOrd);
    else if (left->isByteVec() && right->isByteVec())
        addOp(opCmpStr);
    else
    {
        if (op != opEqual && op != opNotEq)
            error("Only equality can be tested for this type");
        addOp(opCmpVar);
    }
    stkPop();
    stkPop();
    addOp(queenBee->defBool, op);
}


void CodeGen::_not()
{
    Type* type = stkTop();
    if (type->isInt())
        addOp(opBitNot);
    else
    {
        implicitCast(queenBee->defBool, "Boolean or integer operand expected");
        addOp(opNot);
    }
}


memint CodeGen::boolJumpForward(OpCode op)
{
    assert(isBoolJump(op));
    implicitCast(queenBee->defBool, "Boolean expression expected");
    stkPop();
    return jumpForward(op);
}


memint CodeGen::jumpForward(OpCode op)
{
    assert(isJump(op));
    memint pos = getCurrentOffs();
    addOp(op);
    addJumpOffs(0);
    return pos;
}


void CodeGen::resolveJump(memint jumpOffs)
{
    assert(jumpOffs <= getCurrentOffs() - 1 - memint(sizeof(jumpoffs)));
    assert(isJump(codeseg[jumpOffs]));
    integer offs = integer(getCurrentOffs()) - integer(jumpOffs + 1 + sizeof(jumpoffs));
    if (offs > 32767)
        error("Jump target is too far away");
    codeseg.atw<jumpoffs>(jumpOffs + 1) = offs;
}


void CodeGen::linenum(integer n)
{
    if (lastOp != opLineNum)
        addOp<integer>(opLineNum, n);
}


void CodeGen::assertion(const str& cond)
{
    implicitCast(queenBee->defBool, "Boolean expression expected for 'assert'");
    stkPop();
    addOp(opAssert, cond.obj);
}


void CodeGen::dumpVar(const str& expr)
{
    Type* type = stkPop();
    addOp(opDump, expr.obj);
    add(type);
}


// --- ASSIGNMENT


static void errorLValue()
    { throw emessage("Not an l-value"); }


static OpCode loaderToStorer(OpCode op)
{
    switch (op)
    {
        case opLoadSelfVar: return opStoreSelfVar;
        case opLoadStkVar:  return opStoreStkVar;
        case opLoadMember:  return opStoreMember;
        case opDeref:       return opStoreRef;
        // end grounded storers
        case opStrElem:     return opStoreStrElem;
        case opVecElem:     return opStoreVecElem;
        case opDictElem:    return opStoreDictElem;
        default:
            errorLValue();
            return opInv;
    }
}


static OpCode loaderToLea(OpCode op)
{
    switch (op)
    {
        case opLoadSelfVar: return opLeaSelfVar;
        case opLoadStkVar:  return opLeaStkVar;
        case opLoadMember:  return opLeaMember;
        case opDeref:       return opLeaRef;
        default:
            errorLValue();
            return opInv;
    }
}


str CodeGen::lvalue()
{
    memint offs = stkTopItem().offs;
    OpCode storer = loaderToStorer(codeseg[offs]);
    if (isGroundedStorer(storer))
    {
        // Plain assignment to a "grounded" variant: remove the loader and
        // return the corresponding storer to be appended later at the end
        // of the assignment statement.
    }
    else
    {
        // A more complex assignment case: look at the previous loader - it 
        // should be a grounded one, transform it to its LEA equivalent, then
        // transform/move the last loader like in the previous case.
        codeseg.replaceOp(prevLoaderOffs, loaderToLea(codeseg[prevLoaderOffs]));
    }
    codeseg.replaceOp(offs, storer);
    return codeseg.cutOp(offs);
}


void CodeGen::assignment(const str& storerCode)
{
    assert(!storerCode.empty());
    Type* dest = stkTop(2);
    // if (!dest->isReference())
    //     deref();
    implicitCast(dest, "Type mismatch in assignment");
    codeseg.append(storerCode);
    stkPop();
    stkPop();
}


void CodeGen::end()
{
    codeseg.close();
    assert(simStack.size() - locals == 0);
}

