

#include "common.h"
#include "runtime.h"
#include "source.h"
#include "symbols.h"
#include "typesys.h"
#include "vm.h"

#include <stack>


// --- THE VIRTUAL MACHINE ------------------------------------------------- //


static void invOpcode()        { throw emessage("Invalid opcode"); }


template<class T>
    inline void PUSH(variant*& stk, const T& v) { ::new(++stk) variant(v);  }
inline void POP(variant*& stk)                  { (*stk--).~variant(); }


#define BINARY_INT(op) { (stk - 1)->_int_write() op stk->_int(); POP(stk); }
#define UNARY_INT(op)  { stk->_int_write() = op stk->_int(); }


void CodeSeg::doRun(variant* stk, const uchar* ip)
{
    variant* stkbase = stk;
    try
    {
        while (1)
        {
            switch(*ip++)
            {
            case opInv:     invOpcode(); break;
            case opEnd:     goto exit;
            case opNop:     break;

            // Const loaders
            case opLoadNull:        PUSH(stk, null); break;
            case opLoadFalse:       PUSH(stk, false); break;
            case opLoadTrue:        PUSH(stk, true); break;
            case opLoadChar:        PUSH(stk, *ip++); break;
            case opLoad0:           PUSH(stk, integer(0)); break;
            case opLoad1:           PUSH(stk, integer(1)); break;
            case opLoadInt:         PUSH(stk, *(integer*)ip); ip += sizeof(integer); break;
            case opLoadNullStr:     PUSH(stk, null_str); break;
            case opLoadNullRange:   PUSH(stk, new_range()); break;
            case opLoadNullVec:     PUSH(stk, new_tuple()); break;
            case opLoadNullDict:    PUSH(stk, new_dict()); break;
            case opLoadNullOrdset:  PUSH(stk, new_ordset()); break;
            case opLoadNullSet:     PUSH(stk, new_set()); break;
            case opLoadConst:       PUSH(stk, consts[*ip++]); break;
            case opLoadConst2:      PUSH(stk, consts[*(uint16_t*)ip]); ip += 2; break;
            case opLoadTypeRef:     PUSH(stk, *(object**)ip); ip += sizeof(object*); break;

            // Safe typecasts
            case opToBool:  *stk = stk->to_bool(); break;
            case opToStr:   *stk = stk->to_string(); break;
            case opToType:  { Type* t = *(Type**)ip; ip += sizeof(Type*); t->runtimeTypecast(*stk); } break;
            case opDynCast: notimpl(); break; // TODO:

            // Arithmetic
            // TODO: range checking in debug mode
            case opAdd:     BINARY_INT(+=); break;
            case opSub:     BINARY_INT(-=); break;
            case opMul:     BINARY_INT(*=); break;
            case opDiv:     BINARY_INT(/=); break;
            case opMod:     BINARY_INT(%=); break;
            case opBitAnd:  BINARY_INT(&=); break;
            case opBitOr:   BINARY_INT(|=); break;
            case opBitXor:  BINARY_INT(^=); break;
            case opBitShl:  BINARY_INT(<<=); break;
            case opBitShr:  BINARY_INT(>>=); break;
            case opNeg:     UNARY_INT(-); break;
            case opBitNot:  UNARY_INT(~); break;
            case opNot:     UNARY_INT(-); break;

            // Vector/string concatenation
            case opCharToStr:   *stk = str(1, stk->_uchar()); break;
            case opCharCat:     (stk - 1)->_str_write().push_back(stk->_uchar()); POP(stk); break;
            case opStrCat:      (stk - 1)->_str_write().append(stk->_str_read()); POP(stk); break;
            case opVarToVec:    *stk = new tuple(1, *stk); break;
            case opVarCat:      (stk - 1)->_tuple_write().push_back(*stk); POP(stk); break;
            case opVecCat:      (stk - 1)->_tuple_write().append(stk->_tuple_read()); POP(stk); break;

            // Range operations (work for all ordinals)
            case opMkRange:     (stk - 1)->assign((stk - 1)->_ord(), stk->_ord()); POP(stk); break;
            case opInRange:     *(stk - 1) = stk->_range_read().has((stk - 1)->_ord()); POP(stk); break;

            // Comparators
            case opCmpOrd:      *(stk - 1) = (stk - 1)->_ord() - stk->_ord(); POP(stk); break;
            case opCmpStr:      *(stk - 1) = (stk - 1)->_str_read().compare(stk->_str_read()); POP(stk); break;
            case opCmpVar:      *(stk - 1) = int(*(stk - 1) == *stk) - 1; POP(stk); break;

            case opEqual:       stk->_int_write() = stk->_int() == 0; break;
            case opNotEq:       stk->_int_write() = stk->_int() != 0; break;
            case opLessThan:    stk->_int_write() = stk->_int() < 0; break;
            case opLessEq:      stk->_int_write() = stk->_int() <= 0; break;
            case opGreaterThan: stk->_int_write() = stk->_int() > 0; break;
            case opGreaterEq:   stk->_int_write() = stk->_int() >= 0; break;

            default: invOpcode(); break;
            }
        }
exit:
        if (stk != stkbase + returns)
            fatal(0x5001, "Stack unbalanced");
    }
    catch(exception& e)
    {
        while (stk > stkbase)
            POP(stk);
        // TODO: stack is not free()'d here
        throw e;
    }
}


void CodeSeg::run(varstack& stack)
{
    assert(closed);
    if (code.empty())
        return;
    doRun(stack.reserve(stksize) - 1, (const uchar*)code.data());
    stack.free(stksize - returns);
}


void ConstCode::run(variant& result)
{
    varstack stack;
    CodeSeg::run(stack);
    result = stack.top();
    stack.pop();
    assert(stack.size() == 0);
}



// --- CODE GENERATOR ------------------------------------------------------ //


class CodeGen: noncopyable
{
protected:

    struct stkinfo
    {
        Type* type;
        stkinfo(Type* t): type(t) { }
    };

    CodeSeg& codeseg;

    std::stack<stkinfo> genStack;
    mem stkMax;
#ifdef DEBUG
    mem stkSize;
#endif

    void stkPush(Type* t, const variant& v);
    void stkPush(Type* t)
            { stkPush(t, null); }
    void stkPush(Constant* c)
            { stkPush(c->type, c->value); }
    const stkinfo& stkTop() const;
    Type* stkTopType() const
            { return stkTop().type; }
    Type* stkPop();

    bool tryCast(Type*, Type*);
    
public:
    CodeGen(CodeSeg&);
    ~CodeGen();

    void endConstExpr(Type* expectType);
    void loadNone();
    void loadBool(bool b)
            { loadConst(queenBee->defBool, b); }
    void loadChar(uchar c)
            { loadConst(queenBee->defChar, c); }
    void loadInt(integer i)
            { loadConst(queenBee->defInt, i); }
    void loadConst(Type*, const variant&);
    void explicitCastTo(Type*);
    void implicitCastTo(Type*);
    void arithmBinary(OpCode);
    void arithmUnary(OpCode);
    void elemToVec();
    void elemCat();
    void cat();
    void mkRange();
    void inRange();
    void cmp(Token tok);
};


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
    else if (to->isString())
        codeseg.addOp(opToStr);     // ... as well as to string
    else if (from->canCastImplTo(to))
        ;   // means variant copying is considered safe, including State casts (not implemented yet)
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
    else if (from->isVariant() || (from->isOrdinal() && to->isOrdinal()))
    {
        // Ordinals must be casted at runtime so that the variant type of the
        // value on the stack is correct for subsequent operations.
        codeseg.addOp(opToType);    // calls to->runtimeTypecast(v)
        codeseg.addPtr(to);
    }
    else if (from->isState() && to->isState())
    {
        // Implicit type cast wasn't possible, so try run-time typecast
        codeseg.addOp(opDynCast);
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
    codeseg.addOp(elemType->isChar() ? opCharToStr : opVarToVec);
    stkPush(elemType->deriveVector());
}


void CodeGen::elemCat()
{
    Type* elemType = stkPop();
    Type* vecType = stkTopType();
    if (!vecType->isVector())
        throw emessage("Vector type expected");
    if (!elemType->canCastImplTo(PVector(vecType)->elem))
        throw emessage("Vector element type mismatch");
    codeseg.addOp(elemType->isChar() ? opCharCat : opVarCat);
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
    stkPush(POrdinal(left)->deriveRange());
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


void CodeGen::cmp(Token tok)
{
    assert(tok >= tokEqual && tok <= tokGreaterEq);
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
        if (tok != tokEqual && tok != tokNotEq)
            throw emessage("Only equality can be tested for this type");
        codeseg.addOp(opCmpVar);
    }
    codeseg.addOp(opEqual + OpCode(tok - tokEqual));
    stkPush(queenBee->defBool);
}


// --- tests --------------------------------------------------------------- //


#define check(x) assert(x)


int main()
{
    initTypeSys();
    try
    {
        Parser parser("x", new in_text("x"));

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
        check(r.left() == 1 && r.right() == 0);

        // Vector concatenation
        tuple* t = new tuple(1, 3);
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

    }
    catch (std::exception& e)
    {
        ferr << "Exception: " << e.what() << endl;
        return 101;
    }
    doneTypeSys();
#ifdef DEBUG
    assert(object::alloc == 0);
#endif
}

