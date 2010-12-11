
#include "vm.h"


inline bool hasTypeArg(OpCode op)
{
    OpArgType arg = opTable[op].arg;
    return arg >= argType && arg <= argFifo;
}


inline bool hasStateArg(OpCode op)
{
    OpArgType arg = opTable[op].arg;
    return arg == argState || arg == argFarState;
}


// --- Code Segment -------------------------------------------------------- //


CodeSeg::CodeSeg(State* s) throw()
    : object(), state(s)
#ifdef DEBUG
    , closed(false)
#endif
    { }


CodeSeg::~CodeSeg() throw()
    { }


memint CodeSeg::opLenAt(memint offs) const
    { return opLen(opAt(offs)); }


void CodeSeg::eraseOp(memint offs)
    { code.erase(offs, opLenAt(offs)); }


str CodeSeg::cutOp(memint offs)
{
    memint len = opLenAt(offs);
    str s = code.substr(offs, len);
    code.erase(offs, len);
    return s;
}


void CodeSeg::replaceOpAt(memint i, OpCode op)
{
    assert(opArgType(opAt(i)) == opArgType(op));
    *code.atw<uchar>(i) = op;
}


Type* CodeSeg::typeArgAt(memint i) const
{
    assert(hasTypeArg(opAt(i)));
    return at<Type*>(i + 1);
}


void CodeSeg::close()
{
#ifdef DEBUG
    assert(!closed);
    closed = true;
#endif
    append(opEnd);
}


// --- Code Generator ------------------------------------------------------ //


evoidfunc::evoidfunc() throw() { }
evoidfunc::~evoidfunc() throw() { }
const char* evoidfunc::what() throw() { return "Void function called"; }


CodeGen::CodeGen(CodeSeg& c, Module* m, State* treg, bool compileTime) throw()
    : module(m), codeOwner(c.getStateType()), typeReg(treg), codeseg(c), locals(0),
      prevLoaderOffs(-1), primaryLoaders()
{
    assert(treg != NULL);
    if (compileTime != (codeOwner == NULL))
        fatal(0x6003, "CodeGen: invalid codeOwner");
}


CodeGen::~CodeGen() throw()
    { }


void CodeGen::error(const char* msg)
    { throw emessage(msg); }


void CodeGen::error(const str& msg)
    { throw emessage(msg); }


void CodeGen::stkPush(Type* type, memint offs)
{
    simStack.push_back(SimStackItem(type, offs));
    OpCode op = codeseg.opAt(offs);
    if (isPrimaryLoader(op))
        primaryLoaders.push_back(offs);
}


void CodeGen::addOp(Type* type, OpCode op)
{
    memint offs = getCurrentOffs();
    addOp(op);
    stkPush(type, offs);
}


void CodeGen::addOp(Type* type, const str& code)
{
    memint offs = getCurrentOffs();
    codeseg.append(code);
    stkPush(type, offs);
}


Type* CodeGen::stkPop()
{
    const SimStackItem& s = simStack.back();
    prevLoaderOffs = s.loaderOffs;
    if (!primaryLoaders.empty() && s.loaderOffs < primaryLoaders.back())
        primaryLoaders.pop_back();
    Type* result = s.type;
    simStack.pop_back();
    return result;
}


void CodeGen::undoSubexpr()
{
    // This works based on the assumption that at any stack level there is a 
    // corresponding primary loader starting from which all code can be safely
    // discarded. I think this should work provided that any instruction
    // pushes not more than one value onto the stack (regardless of how many
    // it pops off the stack). See also stkPop().
    memint from;
    primaryLoaders.pop_back(from); // get & pop
    codeseg.erase(from);
    simStack.pop_back();
    prevLoaderOffs = -1;
}


bool CodeGen::canDiscardValue()
    { return isDiscardable(codeseg.opAt(stkLoaderOffs())); }

void CodeGen::stkReplaceType(Type* t)
    { simStack.backw().type = t; }


bool CodeGen::tryImplicitCast(Type* to)
{
    Type* from = stkType();

    if (from == to)
        return true;

    if (to->isVariant() || from->canAssignTo(to))
    {
        // canAssignTo() should take care of polymorphic typecasts
        stkReplaceType(to);
        return true;
    }

    // Vector elements are automatically converted to vectors when necessary,
    // e.g. char -> str
    if (to->isVectorOf(from))
    {
        elemToVec(PContainer(to));
        return true;
    }

    if (from->isNullCont())
    {
        undoSubexpr();
        if (to->isAnyFifo())
            loadFifo(PFifo(to));
        else
            loadEmptyConst(to);
        return true;
    }

    if (from->isFuncPtr() && to->isTypeRef())
    {
        memint offs = stkLoaderOffs();
        if (hasStateArg(codeseg.opAt(offs)))
        {
            State* stateType = codeseg.stateArgAt(offs);
            undoSubexpr();
            loadTypeRefConst(stateType);
            return true;
        }
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

    Type* from = stkType();

    if (from->isAnyOrd() && to->isAnyOrd())
        stkReplaceType(to);

    else if (from->isVariant())
    {
        stkPop();
        addOp<Type*>(to, opCast, to);
    }

    // TODO: better error message with type defs
    else
        error("Invalid explicit typecast");
}


void CodeGen::isType(Type* to)
{
    Type* from = stkType();
    if (from->canAssignTo(to))
    {
        undoSubexpr();
        loadConst(queenBee->defBool, 1);
    }
    else if (from->isAnyState() || from->isVariant())
    {
        stkPop();
        addOp<Type*>(queenBee->defBool, opIsType, to);
    }
    else
    {
        undoSubexpr();
        loadConst(queenBee->defBool, 0);
    }
}


void CodeGen::mkRange()
{
    Type* left = stkType(2);
    if (!left->isAnyOrd())
        error("Non-ordinal range bounds");
    implicitCast(left, "Incompatible range bounds");
    stkPop();
    stkPop();
    addOp(POrdinal(left)->getRangeType(), opMkRange);
}


void CodeGen::toStr()
    { addOp<Type*>(queenBee->defStr, opToStr, stkPop()); }


void CodeGen::deinitLocalVar(Variable* var)
{
    // TODO: don't generate POPs if at the end of a function in RELEASE mode
    assert(var->isStkVar());
    assert(locals == getStackLevel());
    if (var->id != locals - 1)
        fatal(0x6002, "Invalid local var id");
    popValue();
    locals--;
}


void CodeGen::deinitFrame(memint baseLevel)
{
    memint topLevel = getStackLevel();
    for (memint i = topLevel; i > baseLevel; i--)
    {
        bool isPod = stkType(topLevel - i + 1)->isPod();
        addOp(isPod ? opPopPod : opPop);
    }
}


void CodeGen::popValue()
{
    bool isPod = stkPop()->isPod();
    addOp(isPod ? opPopPod : opPop);
}


Type* CodeGen::undoTypeRef()
{
    memint offs = stkLoaderOffs();
    if (codeseg.opAt(offs) != opLoadTypeRef)
        error("Const type reference expected");
    Type* type = codeseg.typeArgAt(offs);
    undoSubexpr();
    return type;
}


State* CodeGen::undoStateRef()
{
    Type* type = undoTypeRef();
    if (!type->isAnyState())
        error("State/function type expected");
    return PState(type);
}


Ordinal* CodeGen::undoOrdTypeRef()
{
    Type* type = undoTypeRef();
    if (!type->isAnyOrd())
        error("Ordinal type reference expected");
    return POrdinal(type);
}


bool CodeGen::deref()
{
    Type* type = stkType();
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
    Type* type = stkType();
    if (!type->isReference())
    {
        if (codeseg.opAt(stkLoaderOffs()) == opDeref)
            error("Superfluous automatic dereference");
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
    Type* type = stkType();
    if (!type->isBool())
    {
        stkPop();
        addOp(queenBee->defBool, opNonEmpty);
    }
}


void CodeGen::loadTypeRefConst(Type* type)
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
    case variant::RANGE:
    case variant::VEC:
    case variant::SET:
    case variant::ORDSET:
    case variant::DICT:
    case variant::REF:
        break;
    case variant::RTOBJ:
        if (value._rtobj()->getType()->isTypeRef())
        {
            loadTypeRefConst(cast<Type*>(value._rtobj()));
            return;
        }
        break;
    }
    fatal(0x6001, "Unknown constant literal");
}


void CodeGen::loadTypeRef(Type* type)
{
    if (type->isAnyState())
    {
        // A state definition by default is tranformed into a function pointer
        // to preserve the object subexpression that may have preceeded it (see
        // loadMember(Symbol*). Later though, one of the following may occur:
        // (1) it's a function call, then most likely opLoad*FuncPtr will be
        // replaced with a op*Call (see call()); (2) typecast is requested to 
        // TypeRef, in which case the preceeding subexpression is discarded;
        // (3) member constant selection or scope override is requested: same 
        // as (2); or (4) otherwise the function pointer is left "as is".
        State* stateType = PState(type);
        if (stateType->isStatic())
        {
            addOp<State*>(stateType->prototype, opLoadStaticFuncPtr, stateType);
        }
        else if (isCompileTime())
        {
            addOp<State*>(stateType->prototype, opLoadFuncPtrErr, stateType);
        }
        else if (stateType->parent == codeOwner->parent)
        {
            codeOwner->useOutsideObject();
            addOp<State*>(stateType->prototype, opLoadOuterFuncPtr, stateType);
        }
        else if (stateType->parent == codeOwner)
        {
            codeOwner->useOutsideObject(); // uses dataseg
            codeOwner->useInnerObj();
            addOp<State*>(stateType->prototype, opLoadInnerFuncPtr, stateType);
        }
        else if (stateType->parent == codeOwner->parentModule) // near top-level func
        {
            loadDataSeg();
            stkPop();
            addOp<State*>(stateType->prototype, opMkFuncPtr, stateType);
        }
        // TODO: far call, see loadMember(State*, Symbol*)
        else
            error("Invalid context for a function pointer");
    }
    else
        loadTypeRefConst(type);
}


void CodeGen::loadDefinition(Definition* def)
{
    Type* aliasedType = def->getAliasedType();
    if (aliasedType)
        loadTypeRef(aliasedType);
    else if (def->type->isVoid() || def->type->isAnyOrd() || def->type->isByteVec())
        loadConst(def->type, def->value);
    else
        addOp<Definition*>(def->type, opLoadConst, def);
}


static variant::Type typeToVarType(Type* t)
{
    // TYPEREF, VOID, VARIANT, REF,
    //    BOOL, CHAR, INT, ENUM,
    //    NULLCONT, VEC, SET, DICT,
    //    FIFO, PROTOTYPE, SELFSTUB, STATE
    // VOID, ORD, REAL, VARPTR,
    //      STR, VEC, SET, ORDSET, DICT, REF, RTOBJ
    switch (t->typeId)
    {
    case Type::TYPEREF:
        return variant::RTOBJ;
    case Type::VOID:
    case Type::NULLCONT:
    case Type::VARIANT:
        return variant::VOID;
    case Type::REF:
        return variant::REF;
    case Type::RANGE:
        return variant::RANGE;
    case Type::BOOL:
    case Type::CHAR:
    case Type::INT:
    case Type::ENUM:
        return variant::ORD;
    case Type::VEC:
        return t->isByteVec() ? variant::STR : variant::VEC;
    case Type::SET:
        return t->isByteSet() ? variant::ORDSET : variant::SET;
    case Type::DICT:
        return t->isByteDict() ? variant::VEC : variant::DICT;
    case Type::FUNCPTR:
    case Type::FIFO:
    case Type::STATE:
    case Type::MODULE:
        return variant::RTOBJ;
    case Type::SELFSTUB:
        throw emessage("'self' incomplete");
    }
    return variant::VOID;
}


void CodeGen::loadEmptyConst(Type* type)
    { addOp<uchar>(type, opLoadEmptyVar, typeToVarType(type)); }


void CodeGen::loadSymbol(Symbol* sym)
{
    if (sym->isDef())
        loadDefinition(PDefinition(sym));
    else if (sym->isAnyVar())
        loadVariable(PVariable(sym));
    else
        notimpl();
}


void CodeGen::_loadVar(Variable* var, OpCode op)
{
    assert(var->id >= 0 && var->id < 255);
    if (isCompileTime())
        // Load an error message generator in case it gets executed; however
        // this may be useful in expressions like typeof, where the value
        // is not needed:
        addOp(var->type, opLoadVarErr);
    else
        addOp<uchar>(var->type, op, var->id);
}


void CodeGen::loadInnerVar(InnerVar* var)
{
    // In ordinary (non-ctor) functions innerobj may not be available because
    // of optimizations, so we use stack reference whenever possible
    if (codeOwner && codeOwner->isCtor)
    {
        // codeOwner->useInnerObj(); -- done in State::State()
        _loadVar(var, opLoadInnerVar);
    }
    else
        _loadVar(var, opLoadStkVar);
}


void CodeGen::loadResultVar(ResultVar* var)
{
    assert(var->id == 0);
    addOp(var->type, isCompileTime() ? opLoadVarErr : opLoadResultVar);
}


static void varNotAccessible(const str& name)
    { throw emessage("'" + name  + "' is not accessible within this context"); }


void CodeGen::loadVariable(Variable* var)
{
    assert(var->host != NULL);
    if (isCompileTime())
        addOp(var->type, opLoadVarErr);
    else if (var->host == codeOwner)
    {
        if (var->isStkVar())
            loadStkVar(PStkVar(var));
        else if (var->isArgVar())
            loadArgVar(PArgVar(var));
        else if (var->isPtrVar())
            loadPtrVar(PPtrVar(var));
        else if (var->isResultVar())
            loadResultVar(PResultVar(var));
        else if (var->isInnerVar())
            loadInnerVar(PInnerVar(var));
        else
            varNotAccessible(var->name);
    }
    else if (var->isInnerVar() && var->host == codeOwner->parent)
    {
        codeOwner->useOutsideObject();
        _loadVar(var, opLoadOuterVar);
    }
    else if (var->isInnerVar() && var->host == module)
    {
        loadDataSeg();
        loadMember(module, var);
    }
    else
        varNotAccessible(var->name);
}


void CodeGen::loadMember(State* hostStateType, Symbol* sym)
{
    assert(hostStateType == stkType());
    if (sym->host != hostStateType)  // shouldn't happen
        fatal(0x600c, "Invalid member selection");
    if (sym->isAnyVar())
        loadMember(hostStateType, PVariable(sym));
    else if (sym->isDef())
    {
        Definition* def = PDefinition(sym);
        Type* type = def->getAliasedType();
        if (type && type->isAnyState())
            loadMember(hostStateType, PState(type));
        else
        {
            undoSubexpr();
            loadDefinition(def);
        }
    }
    else
        notimpl();
}


void CodeGen::loadMember(State* hostStateType, State* stateType)
{
    assert(hostStateType == stkType());
    if (stateType->parent != hostStateType)  // shouldn't happen
        fatal(0x600d, "Invalid member state selection");
    if (stateType->isStatic())
    {
        undoSubexpr();
        addOp(stateType->prototype, opLoadStaticFuncPtr, stateType);
    }
    else if (isCompileTime())
    {
        undoSubexpr();
        addOp(stateType->prototype, opLoadFuncPtrErr, stateType);
    }
    else
    {
        stkPop();  // host
        codeOwner->useOutsideObject();
        Module* targetModule = stateType->parentModule;
        if (targetModule == codeOwner->parentModule) // near call
            addOp<State*>(stateType->prototype, opMkFuncPtr, stateType);
        else
        {
            // For far calls/funcptrs a data segment object should be provided
            // as well: we do this by providing the ID of a corresponding
            // module instance variable:
            InnerVar* moduleVar = codeOwner->parentModule->findUsedModuleVar(targetModule);
            if (moduleVar == NULL)
                error("Function call impossible within this context");
            addOp<State*>(stateType->prototype, opMkFarFuncPtr, stateType);
            add<uchar>(moduleVar->id);
        }
    }
}


void CodeGen::loadMember(State* stateType, Variable* var)
{
    // This variant of loadMember() is called when (1) loading a global/static
    // variable which is not accessible other than through the dataseg object,
    // or (2) from loadMember(Symbol*)
    assert(stateType == stkType());
    if (var->host != stateType || !var->isInnerVar())
        varNotAccessible(var->name);
    if (isCompileTime())
    {
        undoSubexpr();
        addOp(var->type, opLoadVarErr);
    }
    else
    {
        assert(var->id >= 0 && var->id < 255);
        stkPop();
        addOp<uchar>(var->type, opLoadMember, var->id);
    }
}


void CodeGen::loadThis()
{
    if (isCompileTime())
        error("'this' is not available in const expressions");
    else if (codeOwner->parent && codeOwner->parent->isCtor)
    {
        codeOwner->useOutsideObject();
        addOp(codeOwner->parent, opLoadOuterObj);
    }
    else
        error("'this' is not available within this context");
}


void CodeGen::loadDataSeg()
{
    if (isCompileTime())
        error("Static data can not be accessed in const expressions");
    codeOwner->useOutsideObject();
    addOp(module, opLoadDataSeg);
}


void CodeGen::initStkVar(StkVar* var)
{
    if (var->host != codeOwner)
        fatal(0x6005, "initLocalVar(): not my var");
    // Local var simply remains on the stack, so just check the types.
    assert(var->id >= 0 && var->id < 255);
    assert(locals == getStackLevel() - 1 && var->id == locals);
    locals++;
    implicitCast(var->type, "Variable type mismatch");
}


void CodeGen::initInnerVar(InnerVar* var)
{
    assert(var->id >= 0 && var->id < 255);
    assert(codeOwner);
    if (var->host != codeOwner)
        fatal(0x6005, "initInnerVar(): not my var");
    implicitCast(var->type, "Variable type mismatch");
    if (codeOwner->isCtor)
    {
        // codeOwner->useInnerObj(); -- done in State::State()
        stkPop();
        addOp<uchar>(opInitInnerVar, var->id);
    }
    else
    {
        assert(getStackLevel() - 1 == var->id);
        locals++;
        // addOp<uchar>(opInitStkVar, var->id);
    }
}


void CodeGen::incStkVar(StkVar* var)
{
    assert(var->id >= 0 && var->id < 255);
    addOp<uchar>(opIncStkVar, var->id);
}


void CodeGen::loadContainerElem()
{
    // This is square brackets op - can be string, vector, array or dictionary.
    OpCode op = opInv;
    Type* contType = stkType(2);
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
    else if (contType->isAnySet())
    {
        // Selecting a set element thorugh [] returns void, because that's the
        // element type for sets. However, [] selection is used with operator del,
        // that's why we need the opcode opSetElem, which actually does nothing.
        // (see CodeGen::deleteContainerElem())
        implicitCast(PContainer(contType)->index, "Set element type mismatch");
        op = contType->isByteSet() ? opByteSetElem : opSetElem;
    }
    else
        error("Vector/dictionary/set expected");
    stkPop();
    stkPop();
    addOp(PContainer(contType)->elem, op);
}


void CodeGen::loadKeyByIndex()
{
    // For non-byte dicts and sets, used internally by the for loop parser
    Type* contType = stkType(2);
    if (!stkType()->isAnyOrd())
        fatal(0x6008, "loadContainerElemByIndex(): invalid index");
    stkPop();
    stkPop();
    if (contType->isAnyDict() && !contType->isByteDict())
        addOp(PContainer(contType)->index, opDictKeyByIdx);
    else if (contType->isAnySet() && !contType->isByteSet())
        addOp(PContainer(contType)->index, opSetKey);
    else
        fatal(0x6009, "loadContainerElemByIndex(): invalid container type");
}


void CodeGen::loadDictElemByIndex()
{
    // Used internally by the for loop parser
    Type* contType = stkType(2);
    if (!stkType()->isAnyOrd())
        fatal(0x6008, "loadContainerElemByIndex(): invalid index");
    stkPop();
    stkPop();
    if (contType->isAnyDict() && !contType->isByteDict())
        addOp(PContainer(contType)->elem, opDictElemByIdx);
    else
        fatal(0x6009, "loadDictKeyByIndex(): invalid container type");
}


void CodeGen::loadSubvec()
{
    Type* contType = stkType(3);
    Type* left = stkType(2);
    Type* right = stkType();
    bool tail = right->isVoid();
    if (!tail)
        implicitCast(left);
    if (contType->isAnyVec())
    {
        if (!left->isInt())
            error("Vector index type mismatch");
        stkPop();
        stkPop();
        stkPop();
        addOp(contType, contType->isByteVec() ? opSubstr : opSubvec);
    }
    else
        error("Vector/string type expected");
}


void CodeGen::length()
{
    // NOTE: len() for sets and dicts is not a language feature, it's needed for 'for' loops
    Type* type = stkType();
    if (type->isNullCont())
    {
        undoSubexpr();
        loadConst(queenBee->defInt, 0);
    }
    else if (type->isByteSet())
    {
        undoSubexpr();
        loadConst(queenBee->defInt, POrdinal(PContainer(type)->index)->getRange());
    }
    else
    {
        OpCode op = opInv;
        if (type->isAnySet())
            op = opSetLen;
        else if (type->isAnyVec() || type->isByteDict())
            op = type->isByteVec() ? opStrLen : opVecLen;
        else if (type->isAnyDict())
            op = opDictLen;
        else
            error("len() expects vector or string");
        stkPop();
        addOp(queenBee->defInt, op);
    }
}


void CodeGen::lo()
{
    Type* type = stkType();
    if (type->isTypeRef())
        loadConst(queenBee->defInt, undoOrdTypeRef()->left);
    else if (type->isNullCont() || type->isAnyVec())
    {
        undoSubexpr();
        loadConst(queenBee->defInt, 0);
    }
    else if (type->isRange())
    {
        stkPop();
        addOp(queenBee->defInt, opRangeLo);
    }
    else
        error("lo() expects vector, string or ordinal type reference");
}


void CodeGen::hi()
{
    Type* type = stkType();
    if (type->isTypeRef())
        loadConst(queenBee->defInt, undoOrdTypeRef()->right);
    else if (type->isNullCont())
    {
        undoSubexpr();
        loadConst(queenBee->defInt, -1);
    }
    else if (type->isAnyVec())
    {
        stkPop();
        addOp(queenBee->defInt, type->isByteVec() ? opStrHi : opVecHi);
    }
    else if (type->isRange())
    {
        stkPop();
        addOp(queenBee->defInt, opRangeHi);
    }
    else
        error("hi() expects vector, string or ordinal type reference");
}


Container* CodeGen::elemToVec(Container* vecType)
{
    Type* elemType = stkType();
    if (vecType)
    {
        if (!vecType->isAnyVec())
            error("Vector type expected");
        implicitCast(vecType->elem, "Vector/string element type mismatch");
    }
    else
        vecType = elemType->deriveVec(typeReg);
    stkPop();
    addOp(vecType, vecType->isByteVec() ? opChrToStr : opVarToVec);
    return vecType;
}


void CodeGen::elemCat()
{
    Type* vecType = stkType(2);
    if (!vecType->isAnyVec())
        error("Vector/string type expected");
    implicitCast(PContainer(vecType)->elem, "Vector/string element type mismatch");
    stkPop();
    addOp(vecType->isByteVec() ? opChrCat: opVarCat);
}


void CodeGen::cat()
{
    Type* vecType = stkType(2);
    if (!vecType->isAnyVec())
        error("Left operand is not a vector");
    implicitCast(vecType, "Vector/string types do not match");
    stkPop();
    addOp(vecType->isByteVec() ? opStrCat : opVecCat);
}


Container* CodeGen::elemToSet()
{
    Type* elemType = stkType();
    Container* setType = elemType->deriveSet(typeReg);
    stkPop();
    addOp(setType, setType->isByteSet() ? opElemToByteSet : opElemToSet);
    return setType;
}


Container* CodeGen::rangeToSet()
{
    Type* left = stkType(2);
    if (!left->isAnyOrd())
        error("Non-ordinal range bounds");
    if (!left->canAssignTo(stkType()))
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
    Type* setType = stkType(2);
    if (!setType->isAnySet())
        error("Set type expected");
    implicitCast(PContainer(setType)->index, "Set element type mismatch");
    stkPop();
    addOp(setType->isByteSet() ? opByteSetAddElem : opSetAddElem);
}


void CodeGen::checkRangeLeft()
{
    Type* setType = stkType(2);
    if (!setType->isByteSet())
        error("Byte set type expected");
    implicitCast(PContainer(setType)->index, "Set element type mismatch");
}


void CodeGen::setAddRange()
{
    Type* setType = stkType(3);
    if (!setType->isByteSet())
        error("Byte set type expected");
    implicitCast(PContainer(setType)->index, "Set element type mismatch");
    stkPop();
    stkPop();
    addOp(opByteSetAddRng);
}


Container* CodeGen::pairToDict()
{
    Type* val = stkType();
    Type* key = stkType(2);
    Container* dictType = val->deriveContainer(typeReg, key);
    stkPop();
    stkPop();
    addOp(dictType, dictType->isByteDict() ? opPairToByteDict : opPairToDict);
    return dictType;
}


void CodeGen::checkDictKey()
{
    Type* dictType = stkType(2);
    if (!dictType->isAnyDict())
        error("Dictionary type expected");
    implicitCast(PContainer(dictType)->index, "Dictionary key type mismatch");
}


void CodeGen::dictAddPair()
{
    Type* dictType = stkType(3);
    if (!dictType->isAnyDict())
        error("Dictionary type expected");
    implicitCast(PContainer(dictType)->elem, "Dictionary element type mismatch");
    stkPop();
    stkPop();
    addOp(dictType->isByteDict() ? opByteDictAddPair : opDictAddPair);
}


void CodeGen::inCont()
{
    Type* contType = stkPop();
    Type* elemType = stkPop();
    OpCode op = opInv;
    if (contType->isAnySet())
        op = contType->isByteSet() ? opInByteSet : opInSet;
    else if (contType->isAnyDict())
        op = contType->isByteDict() ? opInByteDict : opInDict;
    else
        error("Set/dict type expected");
    if (!elemType->canAssignTo(PContainer(contType)->index))
        error("Key type mismatch");
    addOp(queenBee->defBool, op);
}


void CodeGen::inBounds()
{
    Type* type = undoOrdTypeRef();
    Type* elemType = stkPop();
    if (!elemType->isAnyOrd())
        error("Ordinal type expected");
    addOp<Ordinal*>(queenBee->defBool, opInBounds, POrdinal(type));
}


void CodeGen::inRange()
{
    Type* right = stkPop();
    Type* left = stkPop();
    if (!right->isRange())
        error("Range type expected");
    if (!left->canAssignTo(PRange(right)->elem))
        error("Range element type mismatch");
    addOp(queenBee->defBool, opInRange);
}


void CodeGen::inRange2(bool isCaseLabel)
{
    Type* right = stkPop();
    Type* left = stkPop();
    Type* elem = isCaseLabel ? stkType() : stkPop();
    if (!left->canAssignTo(right))
        error("Incompatible range bounds");
    if (!elem->canAssignTo(left))
        error("Element type mismatch");
    if (!elem->isAnyOrd() || !left->isAnyOrd() || !right->isAnyOrd())
        error("Ordinal type expected");
    addOp(queenBee->defBool, isCaseLabel ? opCaseRange : opInRange2);
}


void CodeGen::loadFifo(Fifo* type)
{
    addOp<Fifo*>(type, type->isByteFifo() ? opLoadCharFifo : opLoadVarFifo, type);
}


Fifo* CodeGen::elemToFifo()
{
    Type* elem = stkPop();
    Fifo* fifoType = elem->deriveFifo(codeOwner);
    addOp<Fifo*>(fifoType, opElemToFifo, fifoType);
    return fifoType;
}


void CodeGen::fifoEnq()
{
    Type* fifoType = stkType(2);
    if (!fifoType->isAnyFifo())
        error("Fifo type expected");
    implicitCast(PFifo(fifoType)->elem, "Fifo element type mismatch");
    stkPop();
    stkPop();
    addOp(fifoType, fifoType->isByteFifo() ? opFifoEnqChar : opFifoEnqVar);
}


void CodeGen::fifoPush()
{
    Type* fifoType = stkType(2);
    if (!fifoType->isAnyFifo())
        error("'<<' expects FIFO type");
    Type* right = stkType();
    // TODO: what about conversions like in C++? probably Nah.
    if (right->isVectorOf(PFifo(fifoType)->elem))
    {
        stkPop();
        stkPop();
        addOp(fifoType, fifoType->isByteFifo() ? opFifoEnqChars : opFifoEnqVars);
    }
    else if (tryImplicitCast(PFifo(fifoType)->elem))
        fifoEnq();
    else
        error("FIFO element type mismatch");
}


void CodeGen::fifoDeq()
{
    Type* fifoType = stkType();
    if (!fifoType->isAnyFifo())
        error("Fifo type expected");
    stkPop();
    addOp(PFifo(fifoType)->elem,
        fifoType->isByteFifo() ? opFifoDeqChar : opFifoDeqVar);
}


void CodeGen::fifoToken()
{
    Type* setType = stkType();
    if (!setType->isByteSet())
        error("Small ordinal set expected");
    Type* fifoType = stkType(2);
    if (!fifoType->isByteFifo())
        error("Small ordinal FIFO expected");
    if (!PContainer(setType)->index->canAssignTo(PFifo(fifoType)->elem))
        error("Set and FIFO element type mismatch");
    stkPop();
    stkPop();
    addOp(PFifo(fifoType)->elem->deriveVec(typeReg), opFifoCharToken);
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
    Type* type = stkType();
    if (!type->isInt())
        error("Operand type doesn't match unary operator");
    addOp(op);
}


void CodeGen::cmp(OpCode op)
{
    assert(isCmpOp(op));
    Type* left = stkType(2);
    implicitCast(left, "Type mismatch in comparison");
    Type* right = stkType();
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


void CodeGen::caseCmp()
{
    Type* left = stkType(2);
    implicitCast(left, "Type mismatch in comparison");
    Type* right = stkPop();
    if (left->isAnyOrd() && right->isAnyOrd())
        addOp(queenBee->defBool, opCaseOrd);
    else if (left->isByteVec() && right->isByteVec())
        addOp(queenBee->defBool, opCaseStr);
    else
        addOp(queenBee->defBool, opCaseVar);
}


void CodeGen::_not()
{
    Type* type = stkType();
    if (type->isInt())
        addOp(opBitNot);
    else
    {
        implicitCast(queenBee->defBool, "Boolean or integer operand expected");
        addOp(opNot);
    }
}


void CodeGen::stkVarCmp(StkVar* var, OpCode op)
{
    // implicitCast(var->type, "Type mismatch in comparison");
    if (!stkType()->isAnyOrd() || !var->type->isAnyOrd())
        fatal(0x6007, "localVarCmp(): unsupported type");
    stkPop();
    if (op == opGreaterThan)
        op = opStkVarGt;
    else if (op == opGreaterEq)
        op = opStkVarGe;
    else
        fatal(0x6007, "localVarCmp(): unsupported opcode");
    assert(var->id >= 0 && var->id < 255);
    addOp<uchar>(queenBee->defBool, op, var->id);
}


void CodeGen::stkVarCmpLength(StkVar* var, StkVar* contVar)
{
    // TODO: optimize (single instruction?)
    loadStkVar(contVar);
    length();
    stkVarCmp(var, opGreaterEq);
}


void CodeGen::boolJump(memint target, OpCode op)
{
    assert(isBoolJump(op));
    implicitCast(queenBee->defBool, "Boolean expression expected");
    stkPop();
    _jump(target, op);
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
    addOp<jumpoffs>(op, 0);
    return pos;
}


void CodeGen::resolveJump(memint target)
{
    assert(target <= getCurrentOffs() - 1 - memint(sizeof(jumpoffs)));
    memint offs = getCurrentOffs() - (target + codeseg.opLenAt(target));
    if (offs > 32767)
        error("Jump target is too far away");
    codeseg.jumpOffsAt(target) = offs;
}


void CodeGen::_jump(memint target, OpCode op)
{
    assert(target <= getCurrentOffs() - 1 - memint(sizeof(jumpoffs)));
    memint offs = target - (getCurrentOffs() + codeseg.opLen(op));
    if (offs < -32768)
        error("Jump target is too far away");
    addOp<jumpoffs>(op, jumpoffs(offs));
}


void CodeGen::linenum(integer n)
{
    addOp<integer>(opLineNum, n);
}


void CodeGen::assertion(integer ln, const str& cond)
{
    implicitCast(queenBee->defBool, "Boolean expression expected for 'assert'");
    stkPop();
    addOp(opAssert);
    add(ln);
    add(cond);
}


void CodeGen::dumpVar(const str& expr)
{
    Type* type = stkPop();
    addOp(opDump, expr.obj);
    add(type);
}


void CodeGen::programExit()
{
    stkPop();
    addOp(opExit);
}


// --- ASSIGNMENTS --------------------------------------------------------- //


static void errorLValue()
    { throw emessage("Not an l-value"); }

static void errorNotAddressableElem()
    { throw emessage("Not an addressable container element"); }

static void errorNotInsertableElem()
    { throw emessage("Not an insertable location"); }


static OpCode loaderToStorer(OpCode op)
{
    switch (op)
    {
        case opLoadInnerVar:    return opStoreInnerVar;
        case opLoadOuterVar:    return opStoreOuterVar;
        case opLoadStkVar:      return opStoreStkVar;
        case opLoadArgVar:      return opStoreArgVar;
        case opLoadPtrVar:      return opStorePtrVar;
        case opLoadResultVar:   return opStoreResultVar;
        case opLoadMember:      return opStoreMember;
        case opDeref:           return opStoreRef;
        // end grounded loaders
        case opStrElem:         return opStoreStrElem;
        case opVecElem:         return opStoreVecElem;
        case opDictElem:        return opStoreDictElem;
        case opByteDictElem:    return opStoreByteDictElem;
        default:
            errorLValue();
            return opInv;
    }
}


static OpCode loaderToLea(OpCode op)
{
    switch (op)
    {
        case opLoadInnerVar:    return opLeaInnerVar;
        case opLoadOuterVar:    return opLeaOuterVar;
        case opLoadStkVar:      return opLeaStkVar;
        case opLoadArgVar:      return opLeaArgVar;
        case opLoadPtrVar:      return opLeaPtrVar;
        case opLoadResultVar:   return opLeaResultVar;
        case opLoadMember:      return opLeaMember;
        case opDeref:           return opLeaRef;
        default:
            errorLValue();
            return opInv;
    }
}


static OpCode loaderToInserter(OpCode op)
{
    switch (op)
    {
        case opStrElem:   return opStrIns;
        case opVecElem:   return opVecIns;
        case opSubstr:    return opSubstrReplace;
        case opSubvec:    return opSubvecReplace;
        default:
            errorNotInsertableElem();
            return opInv;
    }
}


static OpCode loaderToDeleter(OpCode op)
{
    switch (op)
    {
        case opStrElem:       return opDelStrElem;
        case opVecElem:       return opDelVecElem;
        case opSubstr:        return opDelSubstr;
        case opSubvec:        return opDelSubvec;
        case opDictElem:      return opDelDictElem;
        case opByteDictElem:  return opDelByteDictElem;
        case opSetElem:       return opDelSetElem;
        case opByteSetElem:   return opDelByteSetElem;
        default:
            errorNotAddressableElem();
            return opInv;
    }
}


void CodeGen::toLea()
{
    // Note that the sim stack doesn't change even though the value is an
    // effective address (pointer) now
    memint offs = stkLoaderOffs();
    codeseg.replaceOpAt(offs, loaderToLea(codeseg.opAt(offs)));
}


void CodeGen::prevToLea()
{
    memint offs = stkPrevLoaderOffs();
    codeseg.replaceOpAt(offs, loaderToLea(codeseg.opAt(offs)));
}


str CodeGen::lvalue()
{
    memint offs = stkLoaderOffs();
    OpCode loader = codeseg.opAt(offs);
    if (isGroundedLoader(loader))
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
        prevToLea();
    }
    OpCode storer = loaderToStorer(loader);
    codeseg.replaceOpAt(offs, storer);
    return codeseg.cutOp(offs);
}


void CodeGen::assign(const str& storerCode)
{
    assert(!storerCode.empty());
    Type* dest = stkType(2);
    if (dest->isVoid())  // Don't remember why it's here. Possibly because of set elem selection
        error("Destination is void type");
    implicitCast(dest, "Type mismatch in assignment");
    codeseg.append(storerCode);
    stkPop();
    stkPop();
}


str CodeGen::arithmLvalue(Token tok)
{
    // Like with lvalue(), returns the storer code to be processed by assign()
    assert(tok >= tokAddAssign && tok <= tokModAssign);
    toLea();
    OpCode op = OpCode(opAddAssign + (tok - tokAddAssign));
    memint offs = getCurrentOffs();
    codeseg.append(op);
    return codeseg.cutOp(offs);
}


void CodeGen::catLvalue()
{
    if (!stkType()->isAnyVec())
        error("'|=' expects vector/string type");
    toLea();
}


void CodeGen::catAssign()
{
    Type* left = stkType(2);
    if (!left->isAnyVec())
        error("'|=' expects vector/string type");
    Type* right = stkType();
    if (right->canAssignTo(PContainer(left)->elem))
        addOp(left->isByteVec() ? opChrCatAssign : opVarCatAssign);
    else
    {
        implicitCast(left, "Type mismatch in in-place concatenation");
        addOp(left->isByteVec() ? opStrCatAssign : opVecCatAssign);
    }
    stkPop();
    stkPop();
}


str CodeGen::insLvalue()
{
    prevToLea();
    memint offs = stkLoaderOffs();
    OpCode inserter = loaderToInserter(codeseg.opAt(offs));
    codeseg.replaceOpAt(offs, inserter);
    return codeseg.cutOp(offs);
}


void CodeGen::insAssign(const str& storerCode)
{
    assert(!storerCode.empty());
    Type* left = stkType(2);
    Type* right = stkType();
    // This one is a little bit messy. If the lvalue is an element selection
    // then 'left' is the element type; otherwise if it's a subvec/substr 
    // selection (s[i..j]), then 'left' is vector/string type. At the same time,
    // we need to support both vector and element cases on the right. The below
    // code somehow works correctly but I don't like all this.
    if (!right->isVectorOf(left))
        implicitCast(left, "Type mismatch in 'ins'");
    codeseg.append(storerCode);
    stkPop();
    stkPop();
}


void CodeGen::deleteContainerElem()
{
    prevToLea();
    memint offs = stkLoaderOffs();
    OpCode deleter = loaderToDeleter(codeseg.opAt(offs));
    codeseg.replaceOpAt(offs, deleter);
    stkPop();
}


// --- FUNCTIONS, CALLS ---------------------------------------------------- //


void CodeGen::_popArgs(FuncPtr* proto)
{
    // Pop arguments off the simulation stack
    for (memint i = proto->formalArgs.size(); i--; )
    {
#ifdef DEBUG
        Type* argType = proto->formalArgs[i]->type;
        if (argType && !stkType()->canAssignTo(argType))
            error("Argument type mismatch");  // shouldn't happen, checked by the compiler earlier
#endif
        stkPop();
    }
}


void CodeGen::call(FuncPtr* proto)
{
    _popArgs(proto);

    // Remove the opMk*FuncPtr and append a corresponding caller. Note that
    // opcode arguments for funcptr loaders and their respective callers
    // should match.
    assert(stkType()->isFuncPtr());
    OpCode op = opInv;
    memint offs = stkLoaderOffs();
    switch (codeseg.opAt(offs))
    {
        case opLoadOuterFuncPtr: op = opSiblingCall; break;
        case opLoadInnerFuncPtr: op = opChildCall; break;
        case opLoadStaticFuncPtr: op = opStaticCall; break;
        case opMkFuncPtr: op = opMethodCall; break;
        case opMkFarFuncPtr: op = opFarMethodCall; break;
        default: ; // leave op = opInv
    }

    stkPop(); // funcptr; arguments are gone already
    if (op != opInv)
    {
        codeseg.replaceOpAt(offs, op); // replace funcptr loader with a call op
        str callCode = codeseg.cutOp(offs); // and move it to the end (after the actual args)
        if (proto->returns)
            addOp(proto->returnType, callCode);
        else
        {
            codeseg.append(callCode);
            throw evoidfunc();
        }
    }

    else  // indirect call
    {
        if (proto->returns)
            addOp<uchar>(proto->returnType, opCall, proto->popArgCount);
        else
        {
            addOp<uchar>(opCall, proto->popArgCount);
            throw evoidfunc();
        }
    }
}


void CodeGen::staticCall(State* callee)
{
    _popArgs(callee->prototype);
    if (callee->prototype->returns)
        addOp<State*>(callee->prototype->returnType, opStaticCall, callee);
    else
    {
        addOp<State*>(opStaticCall, callee);
        throw evoidfunc();
    }
}


void CodeGen::end()
{
    codeseg.close();
    assert(getStackLevel() == locals);
}

