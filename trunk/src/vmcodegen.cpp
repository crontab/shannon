
#include "vm.h"


// TODO: less loaders: for args and locals one instruction can be used
// TODO: assert: use constant repository for file names
// TODO: multiple return values probably not needed
// TODO: store the current file name in a named const, say __FILE__


CodeGen::CodeGen(CodeSeg& c)
    : codeOwner(c.getType()), codeseg(c), locals(0), maxStack(0)  { }

CodeGen::~CodeGen()
    { }

void CodeGen::error(const char* msg)
    { throw ecmessage(msg); }


void CodeGen::addOp(Type* type, OpCode op)
{
    memint offs = codeseg.size();
    simStack.push_back(SimStackItem(type, offs));
    if (simStack.size() > maxStack)
        maxStack = simStack.size();
    add<uchar>(op);
}


void CodeGen::undoLastLoad()
{
    memint offs = stkTopOffs();
    assert(offs >= 0 && offs < codeseg.size());
    if (isUndoableLoadOp(codeseg[offs]))
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
    simStack.replace_back(SimStackItem(t, offs));
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

    if (to->isVec() && from->canAssignTo(PContainer(to)->elem))
    {
        stkPop();
        addOp(to, PContainer(to)->hasSmallElem() ? opChrToStr : opVarToVec);
        return true;
    }

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


void CodeGen::loadConst(Type* type, const variant& value)
{
    // variant: NONE, ORD, REAL, STR, VEC, SET, ORDSET, DICT, RTOBJ
    // Type: TYPEREF, NONE, VARIANT, REF, BOOL, CHAR, INT, ENUM,
    //       NULLCONT, VEC, SET, DICT, FIFO, FUNC, PROC, OBJECT, MODULE
    switch (type->typeId)
    {
    case Type::TYPEREF:     addOp(type, opLoadTypeRef, value._rtobj()); break;
    case Type::NONE:        addOp(type, opLoadNull); break;
    case Type::VARIANT:     error("Variant constants are not supported"); break;
    case Type::REF:         error("Reference constants are not supported"); break;
    case Type::BOOL:
    case Type::CHAR:
    case Type::INT:
    case Type::ENUM:
        {
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
    case Type::NULLCONT: addOp(type, opLoadNull); break;

    // All dynamic objects in the system are derived from "object" so copying
    // involves only incrementing the ref counter.
    case Type::VEC:
    case Type::SET:
    case Type::DICT:
    case Type::FIFO:
    case Type::FUNC:
    case Type::PROC:
    case Type::CLASS:
    case Type::MODULE:
        addOp<uchar>(type, opLoadConstObj, value.getType());
        add<object*>(value.as_anyobj());
        break;
    }
}


void CodeGen::loadEmptyCont(Container* contType)
{
    variant v;
    switch (contType->typeId)
    {
    case Type::NULLCONT:
        error("Container type undefined");
    case Type::VEC:
        if (contType->hasSmallElem()) v = str(); else v = varvec(); break;
    case Type::SET: break;
        if (contType->hasSmallIndex()) v = ordset(); else v = varset(); break;
    case Type::DICT: break;
        if (contType->hasSmallIndex()) v = varvec(); else v = vardict(); break;
    default:
        notimpl();
    }
    loadConst(contType, v);
}


void CodeGen::loadVariable(Variable* var)
{
    assert(var->state != NULL);
    assert(var->id >= 0 && var->id <= 127);
    if (codeOwner == NULL)
        error("Variables not allowed in constant expressions");

    if (var->isSelfVar() && var->state == codeOwner->selfPtr)
        addOp<char>(var->type, opLoadSelfVar, var->id);
    else if (var->isLocalVar() && var->state == codeOwner)
        addOp<char>(var->type, opLoadStkVar, var->id);
    else
        notimpl();
}


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


void CodeGen::storeRet(Type* type)
{
    implicitCast(type);
    addOp<char>(opStoreStkVar, codeOwner ? codeOwner->retVarId() : -1);
    stkPop();
}


void CodeGen::end()
{
    codeseg.close(maxStack);
    assert(simStack.size() - locals == 0);
}


