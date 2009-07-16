

#include "common.h"
#include "runtime.h"
#include "source.h"
#include "symbols.h"
#include "typesys.h"
#include "vm.h"


// --- THE VIRTUAL MACHINE ------------------------------------------------- //


CodeGen::CodeGen(CodeSeg& _codeseg)
    : codeseg(_codeseg), stkMax(0)
#ifdef DEBUG
    , stkSize(0)
#endif    
{
    assert(codeseg.empty());
}


CodeGen::~CodeGen()  { }


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
        codeseg.addOp(opLoadNull);
        break;
    case Type::BOOL:
        codeseg.addOp(empty ? opLoadFalse : opLoadTrue);
        break;
    case Type::CHAR:
        codeseg.addOp(opLoadChar);
        codeseg.add8(value.as_char());
        break;
    case Type::INT:
    case Type::ENUM:
        {
            integer v = value.as_int();
            if (v == 0)
                codeseg.addOp(opLoad0);
            else if (v == 1)
                codeseg.addOp(opLoad1);
            else
            {
                codeseg.addOp(opLoadInt);
                codeseg.addInt(v);
            }
        }
        break;
    case Type::RANGE:
        if (empty) codeseg.addOp(opLoadNullRange); else addToConsts = true;
        break;
    case Type::DICT:
        if (empty) codeseg.addOp(opLoadNullDict); else addToConsts = true;
        break;
    case Type::ARRAY:
        if (empty) _fatal(0x6001);
        addToConsts = true;
        break;
    case Type::VECTOR:
        if (empty)
            codeseg.addOp(type->isString() ? opLoadNullStr : opLoadNullVec);
        else
            addToConsts = true;
        break;
    case Type::SET:
        if (empty) codeseg.addOp(opLoadNullSet); else addToConsts = true;
        break;
    case Type::ORDSET:
        if (empty) codeseg.addOp(opLoadNullSet); else addToConsts = true;
        break;
    case Type::FIFO:
        if (empty) _fatal(0x6002);
        addToConsts = true;
        break;
    case Type::VARIANT:
        _fatal(0x6003);
        break;
    case Type::TYPEREF:
    case Type::STATE:
        codeseg.addOp(opLoadTypeRef);
        codeseg.addPtr(value.as_object());
        break;
    }

    if (addToConsts)
    {
        mem n = codeseg.consts.size();
        codeseg.consts.push_back(value);
        if (n < 256)
        {
            codeseg.addOp(opLoadConst);
            codeseg.add8(uchar(n));
        }
        else if (n < 65536)
        {
            codeseg.addOp(opLoadConst2);
            codeseg.add16(n);
        }
        else
            throw emessage("Maximum number of constants in a block reached");
    }

    stkPush(type, value);
}


// Try implicit cast and return true if succeeded. This code is shared
// between the explicit and implicit typecast routines.
bool CodeGen::tryCast(Type* from, Type* to)
{
    if (to->isVariant())
        ;   // everything in the VM is a variant anyway
    else if (to->isBool())
        codeseg.addOp(opToBool);    // everything can be cast to bool
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
        codeseg.addOp(opToStr); // explicit cast to string: any object goes
    else if (
        // Variants should be typecast'ed to other types explicitly, except to boolean
        from->isVariant()
        // Ordinals must be casted at runtime so that the variant type of the
        // value on the stack is correct for subsequent operations.
        || (from->isOrdinal() && to->isOrdinal())
        // States: implicit type cast wasn't possible, so try run-time typecast
        || (from->isState() && to->isState()))
    {
        codeseg.addOp(opToType);    // calls to->runtimeTypecast(v)
        codeseg.addPtr(to);
    }
    else
        throw emessage("Invalid typecast");
    stkPush(to);
}


void CodeGen::arithmBinary(OpCode op)
{
    assert(op >= opAdd && op <= opBitShr);
    Type* t1 = stkPop(), * t2 = stkPop();
    if (!t1->isInt() || !t2->isInt())
        throw emessage("Operand types do not match operator");
    codeseg.addOp(op);
    stkPush(t1);
}


void CodeGen::arithmUnary(OpCode op)
{
    assert(op >= opNeg && op <= opNot);
    if (!stkTopType()->isInt())
        throw emessage("Operand type doesn't match operator");
    codeseg.addOp(op);
}


void CodeGen::elemToVec()
{
    Type* elemType = stkPop();
    Type* vecType = elemType->deriveVector();
    if (elemType->isChar())
        codeseg.addOp(opCharToStr);
    else
    {
        codeseg.addOp(opVarToVec);
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
        codeseg.addOp(opCharCat);
    else
    {
        codeseg.addOp(opVarCat);
        codeseg.addPtr(vecType);
    }
}


void CodeGen::cat()
{
    Type* right = stkPop();
    Type* left = stkTopType();
    if (!left->isVector() || !right->isVector())
        throw emessage("Vector type expected");
    if (!right->canCastImplTo(left))
        throw emessage("Vector types do not match");
    codeseg.addOp(PVector(left)->elem->isChar() ? opStrCat : opVecCat);
}


void CodeGen::mkRange()
{
    Type* right = stkPop();
    Type* left = stkPop();
    if (!left->isOrdinal() || !right->isOrdinal())
        throw emessage("Range elements must be ordinal");
    if (!right->canCastImplTo(left))
        throw emessage("Range element types do not match");
    codeseg.addOp(opMkRange);
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
    codeseg.addOp(opInRange);
    stkPush(queenBee->defBool);
}


void CodeGen::cmp(OpCode op)
{
    assert(op >= opEqual && op <= opGreaterEq);
    Type* right = stkPop();
    Type* left = stkPop();
    if (!right->canCastImplTo(left))
        throw emessage("Types mismatch in comparison");
    if (left->isOrdinal() && right->isOrdinal())
        codeseg.addOp(opCmpOrd);
    else if (left->isString() && right->isString())
        codeseg.addOp(opCmpStr);
    else
    {
        // Only == and != are allowed for all other types
        if (op != opEqual && op != opNotEq)
            throw emessage("Only equality can be tested for this type");
        codeseg.addOp(opCmpVar);
    }
    codeseg.addOp(op);
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

        Module m("test", 0, NULL);
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
        
        {
            varstack stk;
            Context ctx;
            ctx.registerModule(queenBee);
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

