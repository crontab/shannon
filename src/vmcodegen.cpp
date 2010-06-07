
#include "vm.h"


// TODO: assert: use constant repository for file names
// TODO: multiple return values probably not needed
// TODO: store the current file name in a named const, say __FILE__


CodeGen::CodeGen(CodeSeg& c)
    : codeOwner(c.getType()), codeseg(c), locals(0), isConstCode(true)  { }


CodeGen::~CodeGen()
    { }


void CodeGen::error(const char* msg)
    { throw ecmessage(msg); }


void CodeGen::addOp(Type* type, OpCode op)
{
    memint offs = codeseg.size();
    simStack.push_back(SimStackItem(type, offs));
    if (simStack.size() > codeseg.stackSize)
        codeseg.stackSize = simStack.size();
    addOp(op);
}


void CodeGen::undoLastLoad()
{
    memint offs = stkTopOffs();
    assert(offs >= 0 && offs < codeseg.size());
    if (isUndoableLoadOp(OpCode(codeseg[offs])))
    {
        stkPop();
        codeseg.resize(offs);
    }
    else
        // discard();
        notimpl();
}


Type* CodeGen::stkPop()
{
    Type* result = simStack.back().type;
    simStack.pop_back();
    return result;
}


void CodeGen::stkReplaceTop(Type* t)
{
    memint offs = simStack.back().offs;
    simStack.pop_back();
    simStack.push_back(SimStackItem(t, offs));
}


void CodeGen::canAssign(Type* from, Type* to, const char* errmsg)
{
    if (!from->canAssignTo(to))
        error(errmsg == NULL ? "Type mismatch" : errmsg);
}


bool CodeGen::tryImplicitCast(Type* to)
{
    Type* from = stkTop();

    if (from == to || from->identicalTo(to))
        return true;

    if (from->canAssignTo(to) || to->isVariant())
    {
        stkReplaceTop(to);
        return true;
    }

    // Vector elements are automatically converted to vectors when necessary,
    // e.g. char -> str
    if (to->isVec() && from->identicalTo(PContainer(to)->elem))
    {
        elemToVec();
        return true;
    }

    // Null container is automatically converted to a more concrete type
    if (from->isNullCont() && to->isAnyCont())
    {
        undoLastLoad();
        loadEmptyCont(PContainer(to));
        return true;
    }

    // TODO: automatic dereference
    return false;
}


void CodeGen::implicitCast(Type* to, const char* errmsg)
{
    if (!tryImplicitCast(to))
        error(errmsg == NULL ? "Type mismatch" : errmsg);
}


void CodeGen::deinitLocalVar(Variable* var)
{
    // This is called from BlockScope.
    // TODO: don't generate POPs if at the end of a function
    assert(var->isLocalVar());
    assert(locals == simStack.size());
    assert(var->id == locals - 1);
    locals--;
    discard();
}


void CodeGen::discard()
{
    stkPop();
    addOp(opPop);
}


Type* CodeGen::tryUndoTypeRef()
{
    memint offs = stkTopOffs();
    if (codeseg[offs] == opLoadTypeRef)
    {
        Type* type = codeseg.at<Type*>(offs + 1);
        stkPop();
        codeseg.resize(offs);
        return type;
    }
    else
        return NULL;
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
    case variant::NONE:
        addOp(type, opLoadNull);
        break;
    case variant::ORD:
        {
            assert(type->isAnyOrd());
            integer i = value._ord();
            if (i == 0)
                addOp(type, opLoad0);
            else if (i == 1)
                addOp(type, opLoad1);
            else if (uinteger(i) <= 255)
                addOp<uchar>(type, opLoadOrd8, i);
            else
                addOp<integer>(type, opLoadOrd, i);
        }
        break;
    case variant::REAL: notimpl(); break;
    case variant::STR:
        assert(type->isOrdVec());
        addOp<object*>(type, opLoadStr, value._anyobj());
        break;
    case variant::VEC:
    case variant::ORDSET:
    case variant::DICT:
        fatal(0x6001, "Internal: unknown constant literal");
        break;
    case variant::RTOBJ:
        if (value._rtobj()->getType()->isTypeRef())
            loadTypeRef(cast<Type*>(value._rtobj()));
        else
            fatal(0x6001, "Internal: unknown constant literal");
        break;
    }
}


void CodeGen::loadDefinition(Definition* def)
{
    Type* type = def->type;
    if (type->isTypeRef() || type->isNone() || def->type->isAnyOrd() || def->type->isOrdVec())
        loadConst(def->type, def->value);
    else
        addOp<Definition*>(def->type, opLoadConst, def);
}


void CodeGen::loadEmptyCont(Container* contType)
{
    variant::Type vartype = variant::NONE;
    switch (contType->typeId)
    {
    case Type::NULLCONT:
        error("Container type undefined");
        break;
    case Type::VEC:
        vartype = contType->hasSmallElem() ? variant::STR : variant::VEC;
        break;
    case Type::SET:
        vartype = contType->hasSmallIndex() ? variant::ORDSET : variant::VEC;
        break;
    case Type::DICT:
        vartype = contType->hasSmallIndex() ? variant::VEC : variant::DICT;
        break;
    default:
        notimpl();
    }
    addOp<char>(contType, opLoadEmptyVar, vartype);
}


void CodeGen::loadVariable(Variable* var)
{
    assert(var->state != NULL);
    assert(var->id >= 0 && var->id <= 127);
    if (codeOwner == NULL)
        error("Variables not allowed in constant expressions");
    else
        isConstCode = false;
    // TODO: check parent states too
    if (var->isSelfVar() && var->state == codeOwner->selfPtr)
        addOp<char>(var->type, opLoadSelfVar, var->id);
    else if (var->isLocalVar() && var->state == codeOwner)
        addOp<char>(var->type, opLoadStkVar, var->id);
    else
        notimpl();
}


void CodeGen::loadMember(Variable* var)
{
    if (codeOwner == NULL)
        error("Variables not allowed in constant expressions");
    else
        isConstCode = false;
    Type* stateType = stkPop();
    // TODO: check parent states too
    if (!stateType->isAnyState() || var->state != stateType
            || !var->isSelfVar())
        error("Invalid member selection");
    addOp<char>(var->type, opLoadMember, var->id);
}


void CodeGen::loadMember(const str& ident)
{
    Type* stateType = stkTop();
    if (!stateType->isAnyState())
        error("Invalid member selection");
    Symbol* sym = PState(stateType)->findShallow(ident);
    if (sym->isVariable())
        loadMember(PVariable(sym));
    else if (sym->isDefinition())
    {
        undoLastLoad();
        loadDefinition(PDefinition(sym));
    }
    else
        notimpl();
}


void CodeGen::loadSymbol(Variable* moduleVar, Symbol* sym)
{
    if (sym->isDefinition())
        loadDefinition(PDefinition(sym));
    else if (sym->isVariable())
    {
        if (moduleVar != NULL)
        {
            loadVariable(moduleVar);
            loadMember(PVariable(sym));
        }
        else
            loadVariable(PVariable(sym));
    }
    else
        notimpl();
}


void CodeGen::storeRet(Type* type)
{
    implicitCast(type);
    addOp<char>(opInitStkVar, codeOwner ? codeOwner->prototype->retVarId() : -1);
    stkPop();
}


/*
Type* CodeGen::undoDesignatorLoad(str& loader)
{
    // Returns a code chunk to be used with store() below.
    memint offs = stkTopOffs();
    if (!isDesignatorLoadOp(codeseg[offs]))
        error("Not a designator (L-value)");
    loader = codeseg.cutTail(offs);
    return stkPop();
}


void CodeGen::storeDesignator(str loaderCode, Type* type)
{
    OpCode op = OpCode(loaderCode[0]);
    assert(isDesignatorLoadOp(op));
    implicitCast(type, "Assignment type mismatch");
    loaderCode.replace(0, char(designatorLoadToStore(op)));
    codeseg.append(loaderCode);
    stkPop();
}
*/

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


Container* CodeGen::elemToVec()
{
    Type* elemType = stkPop();
    Container* contType = elemType->deriveVec();
    addOp(contType, elemType->isSmallOrd() ? opChrToStr : opVarToVec);
    return contType;
}


void CodeGen::elemCat()
{
    Type* elemType = stkTop();
    Type* vecType = stkTop(2);
    if (!vecType->isVec())
        error("Vector/string type expected");
    implicitCast(PContainer(vecType)->elem, "Vector/string element type mismatch");
    elemType = stkPop();
    addOp(elemType->isSmallOrd() ? opChrCat: opVarCat);
}


void CodeGen::cat()
{
    Type* vecType = stkTop(2);
    if (!vecType->isVec())
        error("Left operand is not a vector");
    implicitCast(vecType, "Vector/string types do not match");
    stkPop();
    addOp(vecType->isOrdVec() ? opStrCat : opVecCat);
}


void CodeGen::cmp(OpCode op)
{
    assert(isCmpOp(op));
    Type* left = stkTop(2);
    implicitCast(left, "Type mismatch in comparison");
    Type* right = stkTop();
    if (left->isAnyOrd() && right->isAnyOrd())
        addOp(opCmpOrd);
    else if (left->isOrdVec() && right->isOrdVec())
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


void CodeGen::boolXor()
{
    if (!stkPop()->isBool() || !stkPop()->isBool())
        error("Operand types do not match binary operator");
    addOp(queenBee->defBool, opBoolXor);
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
    memint pos = codeseg.size();
    addOp(op);
    addJumpOffs(0);
    return pos;
}


void CodeGen::resolveJump(memint jumpOffs)
{
    assert(jumpOffs <= codeseg.size() - 1 - memint(sizeof(jumpoffs)));
    assert(isJump(OpCode(codeseg.at<uchar>(jumpOffs))));
    integer offs = integer(codeseg.size()) - integer(jumpOffs + 1 + sizeof(jumpoffs));
    if (offs > 32767)
        error("Jump target is too far away");
    codeseg.atw<jumpoffs>(jumpOffs + 1) = offs;
}


void CodeGen::end()
{
    codeseg.close();
    assert(simStack.size() - locals == 0);
}


