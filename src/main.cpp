

#include "common.h"
#include "runtime.h"
#include "source.h"
#include "typesys.h"
#include "vm.h"


// --- THE VIRTUAL MACHINE ------------------------------------------------- //


CodeGen::CodeGen(CodeSeg& _codeseg)
    : codeseg(_codeseg), lastLoadOp(mem(-1)), stkMax(0)
#ifdef DEBUG
    , stkSize(0)
#endif    
{
    assert(codeseg.empty());
}


CodeGen::~CodeGen()  { }


mem CodeGen::addOp(OpCode op)
{
    mem s = codeseg.size();
    if (isLoadOp(op))
        lastLoadOp = s;
    else
        lastLoadOp = mem(-1);
    codeseg.add8(op);
    return s;
}


void CodeGen::revertLastLoad()
{
    if (lastLoadOp == mem(-1))
        discard();
    else
    {
        codeseg.resize(lastLoadOp);
        lastLoadOp = mem(-1);
    }
}


void CodeGen::stkPush(Type* type, const variant&)
{
    genStack.push(stkinfo(type));
    if (genStack.size() > stkMax)
        stkMax = genStack.size();
#ifdef DEBUG
    stkSize++;
#endif
}


const CodeGen::stkinfo& CodeGen::stkTop() const
    { return genStack.top(); }


Type* CodeGen::stkPop()
{
    Type* result = genStack.top().type;
    genStack.pop();
#ifdef DEBUG
    stkSize--;
#endif
    return result;
}


void CodeGen::end()
{
    codeseg.close(stkMax, 0);
    assert(genStack.size() == 0);
}


void CodeGen::endConstExpr(Type* expectType)
{
    codeseg.close(stkMax, 1);
    Type* resultType = stkPop();
    if (expectType != NULL && !resultType->canCastImplTo(expectType))
        throw emessage("Const expression type mismatch");
    assert(genStack.size() == 0);
}


void CodeGen::loadConst(Type* type, const variant& value)
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
        codeseg.add8(value.as_char());
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
                codeseg.addInt(v);
            }
        }
        break;
    case Type::RANGE:
        if (empty) addOp(opLoadNullRange); else addToConsts = true;
        break;
    case Type::DICT:
        if (empty) addOp(opLoadNullDict); else addToConsts = true;
        break;
    case Type::ARRAY:
        if (empty) _fatal(0x6001);
        addToConsts = true;
        break;
    case Type::VECTOR:
        if (empty)
            addOp(type->isString() ? opLoadNullStr : opLoadNullVec);
        else
            addToConsts = true;
        break;
    case Type::SET:
        if (empty) addOp(opLoadNullSet); else addToConsts = true;
        break;
    case Type::ORDSET:
        if (empty) addOp(opLoadNullSet); else addToConsts = true;
        break;
    case Type::FIFO:
        if (empty) _fatal(0x6002);
        addToConsts = true;
        break;
    case Type::VARIANT:
        loadConst(queenBee->typeFromValue(value), value);
        return;
    case Type::TYPEREF:
    case Type::STATE:
        addOp(opLoadTypeRef);
        codeseg.addPtr(value.as_object());
        break;
    }

    if (addToConsts)
    {
        mem n = codeseg.consts.size();
        codeseg.consts.push_back(value);
        if (n < 256)
        {
            addOp(opLoadConst);
            codeseg.add8(uchar(n));
        }
        else if (n < 65536)
        {
            addOp(opLoadConst2);
            codeseg.add16(n);
        }
        else
            throw emessage("Maximum number of constants in a block reached");
    }

    stkPush(type, value);
}


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


// Try implicit cast and return true if succeeded. This code is shared
// between the explicit and implicit typecast routines.
bool CodeGen::tryCast(Type* from, Type* to)
{
    if (to->isVariant())
        ;   // everything in the VM is a variant anyway
    else if (to->isBool())
        addOp(opToBool);    // everything can be cast to bool
    else if (from->canCastImplTo(to))
        ;   // means variant copying is considered safe, including State casts
    else
        return false;
    return true;
}


void CodeGen::implicitCastTo(Type* to)
{
    if (stkTopType()->identicalTo(to))
        return;
    Type* from = stkPop();
    if (!tryCast(from, to))
        throw emessage("Type mismatch");
    stkPush(to);
}


void CodeGen::explicitCastTo(Type* to)
{
    if (stkTopType()->identicalTo(to))
        return;
    Type* from = stkPop();
    if (tryCast(from, to))
        ;   // implicit typecast does the job
    else if (to->isString())
        addOp(opToStr); // explicit cast to string: any object goes
    else if (
        // Variants should be typecast'ed to other types explicitly, except to boolean
        from->isVariant()
        // Ordinals must be casted at runtime so that the variant type of the
        // value on the stack is correct for subsequent operations.
        || (from->isOrdinal() && to->isOrdinal())
        // States: implicit type cast wasn't possible, so try run-time typecast
        || (from->isState() && to->isState()))
    {
        addOp(opToType);    // calls to->runtimeTypecast(v)
        codeseg.addPtr(to);
    }
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
    {
        addOp(opIsType);
        codeseg.addPtr(type);
    }
    else
    {
        // Can be determined at compile time
        revertLastLoad();
        addOp(varType->canCastImplTo(type) ? opLoadTrue : opLoadFalse);
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
    {
        addOp(opVarToVec);
        codeseg.addPtr(vecType);
    }
    stkPush(vecType);
}


void CodeGen::elemCat()
{
    Type* elemType = stkPop();
    Type* vecType = stkTopType();
    if (!vecType->isVector())
        throw emessage("Vector type expected");
    if (!elemType->canCastImplTo(PVector(vecType)->elem))
        throw emessage("Vector element type mismatch");
    if (elemType->isChar())
        addOp(opCharCat);
    else
        addOp(opVarCat);
}


void CodeGen::cat()
{
    Type* right = stkPop();
    Type* left = stkTopType();
    if (!left->isVector() || !right->isVector())
        throw emessage("Vector type expected");
    if (!right->canCastImplTo(left))
        throw emessage("Vector types do not match");
    addOp(PVector(left)->elem->isChar() ? opStrCat : opVecCat);
}


void CodeGen::mkRange()
{
    Type* right = stkPop();
    Type* left = stkPop();
    if (!left->isOrdinal() || !right->isOrdinal())
        throw emessage("Range elements must be ordinal");
    if (!right->canCastImplTo(left))
        throw emessage("Range element types do not match");
    addOp(opMkRange);
    Type* rangeType = POrdinal(left)->deriveRange();
    codeseg.addPtr(rangeType);
    stkPush(rangeType);
}


void CodeGen::inRange()
{
    Type* rng = stkPop();
    Type* elem = stkPop();
    if (!rng->isRange())
        throw emessage("Range type expected");
    if (!elem->canCastImplTo(PRange(rng)->base))
        throw emessage("Range element type mismatch");
    addOp(opInRange);
    stkPush(queenBee->defBool);
}


void CodeGen::cmp(OpCode op)
{
    assert(isCmpOp(op));
    Type* right = stkPop();
    Type* left = stkPop();
    if (!right->canCastImplTo(left))
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


// --- tests --------------------------------------------------------------- //


#define check(x) assert(x)


int main()
{
    initTypeSys();
    try
    {
        Parser parser("x", new in_text(NULL, "x"));

        Module m(1);
        Constant* c = m.addConstant("c", queenBee->defChar, char(1));

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
        c = m.addConstant("s", queenBee->defStr, "ef");
        seg.clear();
        {
            CodeGen gen(seg);
            gen.loadChar('a');
            gen.elemToVec();
            gen.loadChar('b');
            gen.elemCat();
            gen.loadConst(queenBee->defStr, "cd");
            gen.cat();
            gen.loadConst(queenBee->defStr, "");
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
        c = m.addConstant("v", queenBee->defInt->deriveVector(), t);
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

            gen.loadConst(queenBee->defStr, "abc");
            gen.loadConst(queenBee->defStr, "abc");
            gen.cmp(opEqual);
            gen.elemCat();

            gen.loadInt(1);
            gen.elemToVec();
            gen.loadConst(defTypeRef, queenBee->defInt->deriveVector());
            gen.dynamicCast();
            gen.loadConst(defTypeRef, queenBee->defInt->deriveVector());
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
            gen.implicitCastTo(queenBee->defVariant);
            gen.testType(queenBee->defVariant);
            gen.elemCat();

            gen.endConstExpr(queenBee->defBool->deriveVector());
        }
        seg.run(r);
        check(r.to_string() == "[true, true, true, true, true]");

        {
            varstack stk;
            Context ctx;
            ModuleAlias* m = ctx.addModule("test");
            StateBody* b = m->getBody();
            {
                CodeGen gen(*b);
            }
            ctx.run(stk);
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

