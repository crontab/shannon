
#include "vm.h"


// TODO: less loaders: for args and locals one instruction can be used
// TODO: assert: use constant repository for file names
// TODO: multiple return values probably not needed
// TODO: store the current file name in a named const, say __FILE__


CodeGen::CodeGen(CodeSeg& c)
    : codeOwner(c.getType()), codeseg(c), locals(0), maxStack(0), lastOpOffs(-1)  { }

CodeGen::~CodeGen()
    { }

void CodeGen::error(const char* msg)
    { throw ecmessage(msg); }


memint CodeGen::addOp(OpCode op)
{
    lastOpOffs = codeseg.size();
    add<uchar>(op);
    return lastOpOffs;
}


void CodeGen::discardCode(memint from)
{
    codeseg.resize(from);
    if (lastOpOffs >= from)
        lastOpOffs = memint(-1);
}


void CodeGen::revertLastLoad()
{
    if (isUndoableLoadOp(lastOp()))
        discardCode(lastOpOffs);
    else
        // discard();
        notimpl();
}


OpCode CodeGen::lastOp()
{
    if (lastOpOffs == memint(-1))
        return opInv;
    return OpCode(codeseg.at<uchar>(lastOpOffs));
}


void CodeGen::stkPush(Type* type)
{
    simStack.push_back(type);
    if (simStack.size() > maxStack)
        maxStack = simStack.size();
}


Type* CodeGen::stkPop()
{
    Type* result = simStack.back();
    simStack.pop_back();
    return result;
}


void CodeGen::stkReplaceTop(Type* t)
    { simStack.replace_back(t); }


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
        addOp(PContainer(to)->hasSmallElem() ? opChrToStr : opVarToVec);
        stkReplaceTop(to);
        return true;
    }

    if (from->isNullCont() && to->isAnyCont())
    {
        stkPop();
        revertLastLoad();
        loadEmptyCont(PContainer(to));
        return true;
    }

    if (from->isReference())
    {
        // TODO: replace the original loader with its deref version (in a separate function)
        Type* actual = PReference(from)->to;
        addOp(opDeref);
        stkReplaceTop(actual);
        return tryImplicitCast(actual);
    }

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
    case Type::TYPEREF:     addOp(opLoadTypeRef, value._rtobj()); break;
    case Type::NONE:        addOp(opLoadNull); break;
    case Type::VARIANT:     error("Variant constants are not supported"); break;
    case Type::REF:         error("Reference constants are not supported"); break;
    case Type::BOOL:
    case Type::CHAR:
    case Type::INT:
    case Type::ENUM:
        {
            integer i = value._ord();
            if (i == 0)
                addOp(opLoad0);
            else if (i == 1)
                addOp(opLoad1);
            else if (uinteger(i) <= 255)
                { addOp(opLoadOrd8); add<uchar>(i); }
            else
                { addOp(opLoadOrd); add<integer>(i); }
        }
        break;
    case Type::NULLCONT: addOp(opLoadNull); break;

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
        addOp(opLoadConstObj);
        add<uchar>(value.getType());
        add<object*>(value.as_anyobj());
        break;
    }
    stkPush(type);
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

    // If var is a reference, load the value, otherwise load the var's address
    bool deref = var->type->isReference();
    stkPush(deref ? var->type : var->type->getRefType());
    if (var->isSelfVar() && var->state == codeOwner->selfPtr)
        addOp<char>(deref ? opLoadSelfVar : opLoadSelfVarA, var->id);
    else
        notimpl();
}


void CodeGen::store()
{
    Type* l = stkTop(2);
    if (!l->isReference())
        error("Invalid L-value");
    l = PReference(l)->to;
    implicitCast(l, "Type mismatch in assignment");
    addOp(opStore);
    stkPop();
    stkPop();
}


void CodeGen::end()
{
    codeseg.close(maxStack);
    assert(simStack.size() - locals == 0);
}


void CodeGen::runConstExpr(Type* expectType, variant& result)
{
    if (expectType != NULL)
        implicitCast(expectType, "Constant expression type mismatch");
    initRet();
    end();
    rtstack stack;
    result.clear();
    codeseg.run(stack, NULL, &result);
    assert(stack.size() == 0);
}


