

#include "common.h"
#include "runtime.h"
#include "source.h"
#include "typesys.h"
#include "vm.h"


// --- THE VIRTUAL MACHINE ------------------------------------------------- //


CodeGen::CodeGen(CodeSeg& _codeseg)
    : codeseg(_codeseg), lastOpOffs(mem(-1)), stkMax(0), locals(0)
#ifdef DEBUG
    , stkSize(0)
#endif    
{
    assert(codeseg.empty());
}


CodeGen::~CodeGen()  { }


mem CodeGen::addOp(OpCode op)
{
    assert(!codeseg.closed);
    lastOpOffs = codeseg.size();
    add8(op);
    return lastOpOffs;
}


void CodeGen::addOpPtr(OpCode op, void* p)
    { addOp(op); addPtr(p); }

void CodeGen::add8(uchar i)
    { codeseg.push_back(i); }

void CodeGen::add16(uint16_t i)
    { codeseg.append(&i, 2); }

void CodeGen::addInt(integer i)
    { codeseg.append(&i, sizeof(i)); }

void CodeGen::addPtr(void* p)
    { codeseg.append(&p, sizeof(p)); }


void CodeGen::close()
{
    if (!codeseg.empty())
        add8(opEnd);
    codeseg.close(stkMax);
}


bool CodeGen::revertLastLoad()
{
    if (lastOpOffs == mem(-1) || !isLoadOp(OpCode(codeseg[lastOpOffs])))
    {
        discard();
        return false;
    }
    else
    {
        codeseg.resize(lastOpOffs);
        lastOpOffs = mem(-1);
        return true;
    }
}


void CodeGen::stkPush(Type* type, const variant&)
{
    genStack.push_back(stkinfo(type));
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
    // TODO: optimize this: at the end of a function deinit is not needed
    // (alhtough stack balance should be somehow checked in DEBUG build anyway)
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


void CodeGen::loadConst(Type* type, const variant& value, bool asVariant)
{
    // NONE, BOOL, CHAR, INT, ENUM, RANGE,
    //    DICT, ARRAY, VECTOR, SET, ORDSET, FIFO, VARIANT, TYPEREF, STATE
    bool addToConsts = false;
    bool empty = value.empty();

    switch (type->getTypeId())
    {
    case Type::NONE:
        addOp(opLoadNull);
        break;
    case Type::BOOL:
        addOp(empty ? opLoadFalse : opLoadTrue);
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
        if (empty) addOpPtr(opLoadNullRange, type); else addToConsts = true;
        break;
    case Type::DICT:
        if (empty) addOpPtr(opLoadNullDict, type); else addToConsts = true;
        break;
    case Type::STR:
        if (empty) addOp(opLoadNullStr); else addToConsts = true;
        break;
    case Type::VEC:
        if (empty) addOpPtr(opLoadNullVec, type); else addToConsts = true;
        break;
    case Type::ARRAY:
        if (empty) addOpPtr(opLoadNullArray, type); else addToConsts = true;
        break;
    case Type::ORDSET:
        if (empty) addOpPtr(opLoadNullOrdset, type); else addToConsts = true;
        break;
    case Type::SET:
        if (empty) addOpPtr(opLoadNullSet, type); else addToConsts = true;
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

    if (addToConsts)
    {
        mem n = codeseg.consts.size();
        codeseg.consts.push_back(value);
        if (n < 256)
        {
            addOp(opLoadConst);
            add8(uchar(n));
        }
        else if (n < 65536)
        {
            addOp(opLoadConst2);
            add16(n);
        }
        else
            throw emessage("Maximum number of constants in a block reached");
    }

    stkPush(asVariant ? queenBee->defVariant : type, value);
}


void CodeGen::loadDataseg(Module* module)
{
    assert(module->id <= 255);
    stkPush(module);
    addOp(opLoadDataseg);
    add8(module->id);
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
        { loadConst(defTypeRef, t); }


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
    if (codeseg.context == NULL || locals != genStack.size() - 1 || var->id != locals)
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
    if (codeseg.context == NULL || locals != genStack.size() || var->id != locals - 1)
        _fatal(0x6004);
    locals--;
    discard();
}


void CodeGen::initThisVar(Variable* var)
{
    assert(var->isThisVar());
    assert(var->state == codeseg.state);
    assert(var->id > 255);
    addOp(opInitThis);
    add8(var->id);
}


void CodeGen::doStaticVar(ThisVar* var, OpCode op)
{
    assert(var->state != NULL && var->state != codeseg.state);
    if (var->baseId != Base::THISVAR)
        notimpl();
    Module* module = CAST(Module*, var->state);
    assert(module->id <= 255);
    addOp(op);
    add8(module->id);
    add8(var->id);
}


void CodeGen::loadVar(Variable* var)
{
    assert(var->id <= 255);
    if (var->state != NULL && var->state != codeseg.state)
    {
        // Static from another module
        if (var->state->isModule())
            doStaticVar(var, opLoadStatic);
        else
            notimpl();
    }
    else
    {
        // Local, this, result or arg of the current State
        if (var->baseId < Base::RESULTVAR || var->baseId > Base::ARGVAR)
            notimpl();
        addOp(OpCode(opLoadRet + (var->baseId - Base::RESULTVAR)));
        add8(var->id);
    }
    stkPush(var->type);
}


void CodeGen::loadMember(ThisVar* var)
{
    assert(var->id <= 255);
    Type* objType = stkPop();
    if (!objType->isState())
        throw emessage("State type expected");
    if (var->state != objType)
        throw emessage("Member doesn't belong to this state type"); // shouldn't happen
    stkPush(var->type);
    addOp(opLoadMember);
    add8(var->id);
}


void CodeGen::storeVar(Variable* var)
{
    assert(var->id <= 255);
    implicitCastTo(var->type, "Expression type mismatch");
    stkPop();
    if (var->state != NULL && var->state != codeseg.state)
    {
        // Static from another module
        if (var->state->isModule())
            doStaticVar(var, opStoreStatic);
        else
            notimpl();
    }
    else
    {
        // Local, this, result or arg of the current State
        if (var->baseId < Base::RESULTVAR || var->baseId > Base::ARGVAR)
            notimpl();
        addOp(OpCode(opStoreRet + (var->baseId - Base::RESULTVAR)));
        add8(var->id);
    }
}


// This is square brackets op - can be string, vector, array or dictionary
void CodeGen::loadContainerElem()
{
    Type* idxType = stkPop();
    Type* contType = stkPop();
    if (contType->isVec() || contType->isStr())
    {
        if (!idxType->isInt())
            throw emessage("Vector/string index must be integer");
        idxType = queenBee->defNone;
        addOp(contType->isStr() ? opLoadStrElem : opLoadVecElem);
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
    if (contType->isStr())
        throw emessage("Operation not allowed on strings");
    else if (contType->isVec())
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
    else if (to->isStr())
        addOp(opToStr); // explicit cast to string: any object goes
    else if (
        // Variants should be typecast'ed to other types explicitly, except to boolean
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
    Type* t1 = stkPop(), * t2 = stkPop();
    if (!t1->isInt() || !t2->isInt())
        throw emessage("Operand types do not match operator");
    addOp(op);
    stkPush(t1);
}


void CodeGen::arithmUnary(OpCode op)
{
    assert(op >= opNeg && op <= opNot);
    if (!stkTopType()->isInt())
        throw emessage("Operand type doesn't match operator");
    addOp(op);
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
    if (!vecType->isVec() && !vecType->isStr())
        throw emessage("Vector/string type expected");
    implicitCastTo(CAST(Vec*, vecType)->elem, "Vector/string element type mismatch");
    elemType = stkPop();
    addOp(elemType->isChar() ? opCharCat: opVarCat);
}


void CodeGen::cat()
{
    Type* right = stkPop();
    Type* left = stkTopType();
    if ((!left->isVec() || !right->isVec()) && (!left->isStr() || !right->isStr()))
        throw emessage("Vector/string type expected");
    if (!right->canAssignTo(left))
        throw emessage("Vector/string types do not match");
    addOp(left->isStr() ? opStrCat : opVecCat);
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
    else if (left->isStr() && right->isStr())
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
    mem id = startId + localvars.size();
    if (id >= 255)
        throw emessage("Maximum number of local variables within this scope is reached");
    objptr<Variable> v = new LocalVar(Base::LOCALVAR, type, name, id, NULL, false);
    addUnique(v);   // may throw
    localvars.add(v);
    return v;
}


// --- tests --------------------------------------------------------------- //


#define check(x) assert(x)


int main()
{
    initTypeSys();
    try
    {
        Parser parser("x", new in_text(NULL, "x"));

        {
            Context ctx;
            Module* mod = ctx.addModule("test");
            
            Constant* c = mod->addConstant(queenBee->defChar, "c", char(1));

            // Arithmetic, typecasts
            variant r;
            ConstCode seg;
            {
                CodeGen gen(seg);
                gen.loadConst(c->type, c->value);
                gen.explicitCastTo(queenBee->defVariant);
                gen.explicitCastTo(queenBee->defBool);
                gen.explicitCastTo(queenBee->defInt);
                gen.loadInt(9);
                gen.arithmBinary(opAdd);
                gen.loadInt(2);
                gen.arithmUnary(opNeg);
                gen.arithmBinary(opSub);
                gen.explicitCastTo(queenBee->defStr);
                gen.endConstExpr(queenBee->defStr);
            }
            seg.run(r);
            check(r.as_str() == "12");
            
            // String operations
            c = mod->addConstant(queenBee->defStr, "s", "ef");
            seg.clear();
            {
                CodeGen gen(seg);
                gen.loadChar('a');
                gen.elemToVec();
                gen.loadChar('b');
                gen.elemCat();
                gen.loadStr("cd");
                gen.cat();
                gen.loadStr("");
                gen.cat();
                gen.loadConst(c->type, c->value);
                gen.cat();
                gen.endConstExpr(queenBee->defStr);
            }
            seg.run(r);
            check(r.as_str() == "abcdef");
            
            // Range operations
            seg.clear();
            {
                CodeGen gen(seg);
                gen.loadInt(6);
                gen.loadInt(5);
                gen.loadInt(10);
                gen.mkRange();
                gen.inRange();
                gen.loadInt(1);
                gen.loadInt(5);
                gen.loadInt(10);
                gen.mkRange();
                gen.inRange();
                gen.mkRange();
                gen.endConstExpr(queenBee->defBool->deriveRange());
            }
            seg.run(r);
            check(prange(r.as_object())->get_rt()->isRange()
                && prange(r.as_object())->equals(1, 0));

            // Vector concatenation
            vector* t = new vector(queenBee->defInt->deriveVector(), 1, 3);
            t->push_back(4);
            c = mod->addConstant(queenBee->defInt->deriveVector(), "v", t);
            seg.clear();
            {
                CodeGen gen(seg);
                gen.loadInt(1);
                gen.elemToVec();
                gen.loadInt(2);
                gen.elemCat();
                gen.loadConst(c->type, c->value);
                gen.cat();
                gen.endConstExpr(queenBee->defInt->deriveVector());
            }
            seg.run(r);
            check(r.to_string() == "[1, 2, 3, 4]");

            seg.clear();
            {
                CodeGen gen(seg);
                gen.loadBool(true);
                gen.elemToVec();

                gen.loadStr("abc");
                gen.loadStr("abc");
                gen.cmp(opEqual);
                gen.elemCat();

                gen.loadInt(1);
                gen.elemToVec();
                gen.loadTypeRef(queenBee->defInt->deriveVector());
                gen.dynamicCast();
                gen.loadTypeRef(queenBee->defInt->deriveVector());
                gen.testType();
                gen.elemCat();

    #ifdef DEBUG
                mem s = seg.size();
    #endif
                gen.loadInt(1);
                gen.testType(queenBee->defInt); // compile-time
                gen.testType(queenBee->defBool);
                check(s == seg.size() - 1);
                gen.elemCat();
                gen.loadConst(queenBee->defVariant, 2); // doesn't yield variant actually
                gen.implicitCastTo(queenBee->defVariant, "Type mismatch");
                gen.testType(queenBee->defVariant);
                gen.elemCat();
                gen.loadStr("");
                gen.explicitCastTo(queenBee->defBool);
                gen.elemCat();
                gen.loadStr("abc");
                gen.explicitCastTo(queenBee->defBool);
                gen.elemCat();
                gen.endConstExpr(queenBee->defBool->deriveVector());
            }
            seg.run(r);
            check(r.to_string() == "[true, true, true, true, true, false, true]");
        }

        {
            Context ctx;
            Module* mod = ctx.addModule("test2");
            Dict* dictType = mod->registerType(new Dict(queenBee->defStr, queenBee->defInt));
            dict* d = new dict(dictType);
            d->tie("key1", 2);
            d->tie("key2", 3);
            Constant* c = mod->addConstant(dictType, "dict", d);
            Array* arrayType = mod->registerType(new Array(queenBee->defBool, queenBee->defStr));
            Ordset* ordsetType = queenBee->defChar->deriveSet();
            Set* setType = queenBee->defInt->deriveSet();
            check(!setType->isOrdset());
            varstack stk;
            {
                CodeGen gen(*mod);
                BlockScope block(mod, &gen);

                Variable* v1 = block.addLocalVar(dictType, "v1");
                gen.loadNullContainer(dictType);
                gen.initLocalVar(v1);
                gen.loadVar(v1);
                gen.loadStr("k1");
                gen.loadInt(15);
                gen.storeContainerElem();
                gen.loadVar(v1);
                gen.loadStr("k2");
                gen.loadInt(25);
                gen.storeContainerElem();

                Variable* v2 = block.addLocalVar(arrayType, "v2");
                gen.loadNullContainer(arrayType);
                gen.initLocalVar(v2);
                gen.loadVar(v2);
                gen.loadBool(false);
                gen.loadStr("abc");
                gen.storeContainerElem();
                gen.loadVar(v2);
                gen.loadBool(true);
                gen.loadStr("def");
                gen.storeContainerElem();
                
                Variable* v3 = block.addLocalVar(ordsetType, "v3");
                gen.loadNullContainer(ordsetType);
                gen.initLocalVar(v3);
                gen.loadVar(v3);
                gen.loadChar('a');
                gen.addToSet();
                gen.loadVar(v3);
                gen.loadChar('b');
                gen.addToSet();

                Variable* v4 = block.addLocalVar(setType, "v4");
                gen.loadNullContainer(setType);
                gen.initLocalVar(v4);
                gen.loadVar(v4);
                gen.loadInt(100);
                gen.addToSet();
                gen.loadVar(v4);
                gen.loadInt(1000);
                gen.addToSet();

                gen.loadConst(queenBee->defVariant, 10);
                gen.elemToVec();
                gen.loadStr("xyz");
                gen.loadInt(1);
                gen.loadContainerElem();
                gen.elemCat();
                gen.dup();
                gen.loadInt(0);
                gen.loadContainerElem();
                gen.elemCat();
                gen.loadConst(c->type, c->value);
                gen.loadStr("key2");
                gen.loadContainerElem();
                gen.elemCat();
                gen.loadDataseg(queenBee);
                gen.loadMember(queenBee->siovar);
                gen.elemCat();
                gen.loadVar(v1);
                gen.elemCat();
                gen.loadVar(v2);
                gen.elemCat();
                gen.dup();
                gen.loadInt(7);
                gen.loadInt(21);
                gen.storeContainerElem();
                gen.dup();
                gen.loadInt(7);
                gen.loadInt(22);
                gen.storeContainerElem();
                gen.loadVar(v3);
                gen.elemCat();
                gen.loadVar(v4);
                gen.elemCat();

                gen.loadChar('a');
                gen.loadVar(v3);
                gen.inSet();
                gen.elemCat();

                gen.loadChar('c');
                gen.loadVar(v3);
                gen.inSet();
                gen.elemCat();

                gen.loadInt(1000);
                gen.loadVar(v4);
                gen.inSet();
                gen.elemCat();

                gen.loadInt(1001);
                gen.loadVar(v4);
                gen.inSet();
                gen.elemCat();
                
                gen.loadStr("k3");
                gen.loadVar(v1);
                gen.inDictKeys();
                gen.elemCat();

                gen.loadStr("k1");
                gen.loadVar(v1);
                gen.inDictKeys();
                gen.elemCat();
                
                gen.loadVar(v1);
                gen.loadStr("k2");
                gen.delDictElem();

                gen.storeVar(queenBee->sresultvar);
                block.deinitLocals();
                gen.end();
            }
            variant result = ctx.run(stk);
            str s = result.to_string();
            check(s ==
                "[10, 'y', 10, 3, [<char-fifo>], ['k1': 15], ['abc', 'def'], 22, "
                "[97, 98], [100, 1000], true, false, true, false, false, true]");
        }

        {
            Context ctx;
            Module* mod = ctx.addModule("test2");
            varstack stk;
            {
                CodeGen gen(*mod);
                BlockScope block(mod, &gen);
                Variable* s1 = block.addLocalVar(queenBee->defStr, "s1");
                Variable* s2 = block.addLocalVar(queenBee->defStr, "s2");
                gen.loadStr("abc");
                gen.loadStr("def");
                gen.cat();
                gen.initLocalVar(s1);
                gen.loadStr("123");
                gen.initLocalVar(s2);
                gen.loadVar(s2);
                gen.exit();
                block.deinitLocals();   // not reached
                gen.end();
            }
            variant result = ctx.run(stk);
            check(result.as_str() == "123");
        }

    }
    catch (std::exception& e)
    {
        sio << "Exception: " << e.what() << endl;
        return 101;
    }
#ifdef DEBUG
    sio << "Total objects: " << object::alloc << endl;
#endif
/*
    while (!sio.empty())
    {
        str s = sio.line();
        sio << s << endl;
    }
*/
    doneTypeSys();
#ifdef DEBUG
    assert(object::alloc == 0);
#endif
}

