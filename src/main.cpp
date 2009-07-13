

#include "runtime.h"
#include "source.h"
#include "symbols.h"
#include "typesys.h"
#include "vm.h"


// --- THE VIRTUAL MACHINE ------------------------------------------------- //


void invOpcode()        { throw EInvOpcode(); }


template<class T>
    inline void PUSH(variant*& stk, const T& v) { ::new(++stk) variant(v);  }
inline void POP(variant*& stk)                  { (*stk--).~variant(); }


#define BINARY_INT(op) { (stk - 1)->_int_write() op stk->_int(); POP(stk); }
#define UNARY_INT(op)  { stk->_int_write() = op stk->_int(); }


void CodeSeg::doRun(variant*& stk, const uchar* ip)
{
    while (1)
    {
        switch(*ip++)
        {
        case opInv: invOpcode(); break;
        case opEnd: return;
        case opNop: break;

        // Arithmetic
        case opAdd:    BINARY_INT(+=); break;
        case opSub:    BINARY_INT(-=); break;
        case opMul:    BINARY_INT(/=); break;
        case opMod:    BINARY_INT(%=); break;
        case opBitAnd: BINARY_INT(&=); break;
        case opBitOr:  BINARY_INT(|=); break;
        case opBitXor: BINARY_INT(^=); break;
        case opBitShl: BINARY_INT(<<=); break;
        case opBitShr: BINARY_INT(>>=); break;
        case opNeg:    UNARY_INT(-); break;
        case opBitNot: UNARY_INT(~); break;
        case opNot:    UNARY_INT(-); break;

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
        case opLoadNullTuple:   PUSH(stk, new_tuple()); break;
        case opLoadNullDict:    PUSH(stk, new_dict()); break;
        case opLoadNullOrdset:  PUSH(stk, new_ordset()); break;
        case opLoadNullSet:     PUSH(stk, new_set()); break;
        case opLoadNullFifo:    PUSH(stk, new_fifo()); break;
        case opLoadConst:       PUSH(stk, consts[*ip++]); break;
        case opLoadTypeRef:     PUSH(stk, *(object**)ip); ip += sizeof(object*); break;

        default: invOpcode(); break;
        }
    }
}


void CodeSeg::run(varstack& stack, int returns)
{
    assert(closed);
    if (code.empty())
        return;
    variant* stkbase = stack.reserve(stksize);
    variant* stk = stkbase - 1; // point to the top element
    try
    {
        doRun(stk, (const uchar*)code.data());
    }
    catch(exception& e)
    {
        while (stk <= stkbase)
            POP(stk);
        stack.free(stksize);
        throw e;
    }
    assert(stk == stkbase + returns - 1);
    stack.free(stksize - returns);
}


void ConstCode::run(variant& result)
{
    varstack stack;
    CodeSeg::run(stack, 1);
    result = stack.top();
    stack.pop();
    assert(stack.size() == 0);
}



// --- CODE GENERATOR ------------------------------------------------------ //


DEF_EXCEPTION(ETypeMismatch, "Type mismatch")
DEF_EXCEPTION(EExprTypeMismatch, "Expression type mismatch")


class CodeGen: noncopyable
{
protected:

    struct stkinfo
    {
        Type* type;
        stkinfo(Type* t): type(t) { }
    };

    CodeSeg& codeseg;

    std::vector<stkinfo> genStack;
    varstack valStack;
    mem stkMax;

    void stkPush(Type* t, const variant& v);
    void stkPush(Type* t)
            { stkPush(t, null); }
    void stkPush(Constant* c)
            { stkPush(c->type, c->value); }
    const stkinfo& stkTop() const;
    Type* topType() const
            { return stkTop().type; }
    const variant& topValue() const
            { return valStack.top(); }
    Type* stkPop();

public:
    CodeGen(CodeSeg&);
    ~CodeGen();
    
    void endConstExpr(Type* expectType);
    void loadInt(integer);
    void arithmBinary(OpCode);
    void arithmUnary(OpCode);
};


CodeGen::CodeGen(CodeSeg& _codeseg)
    : codeseg(_codeseg), stkMax(0)
{
    assert(codeseg.empty());
}


CodeGen::~CodeGen()  { codeseg.close(stkMax); }


void CodeGen::endConstExpr(Type* expectType)
{
    Type* resultType = stkPop();
    if (expectType != NULL && !resultType->canCastImplTo(expectType))
        throw EExprTypeMismatch();
}


void CodeGen::loadInt(integer v)
{
    if (v == 0)
        codeseg.addOp(opLoad0);
    else if (v == 1)
        codeseg.addOp(opLoad1);
    else
    {
        codeseg.addOp(opLoadInt);
        codeseg.addInt(v);
    }
    stkPush(queenBee->defaultInt, v);
}


void CodeGen::arithmBinary(OpCode op)
{
    assert(op >= opAdd && op <= opBitShr);
    Type* t1 = stkPop(), * t2 = stkPop();
    if (!t1->isInt() || !t2->isInt() || !t1->canCastImplTo(t2))
        throw ETypeMismatch();
    codeseg.addOp(op);
    stkPush(t1);
}


void CodeGen::arithmUnary(OpCode op)
{
    assert(op >= opNeg && op <= opNot);
    if (!topType()->isInt())
        throw ETypeMismatch();
    codeseg.addOp(op);
}


void CodeGen::stkPush(Type* type, const variant& value)
{
    genStack.push_back(stkinfo(type));
    valStack.push(value);
    if (genStack.size() > stkMax)
        stkMax = genStack.size();
}


const CodeGen::stkinfo& CodeGen::stkTop() const
    { return genStack.back(); }


Type* CodeGen::stkPop()
{
    assert(genStack.size() == valStack.size() && genStack.size() > 0);
    Type* result = genStack.back().type;
    genStack.pop_back();
    valStack.pop();
    return result;
}


// --- varlist ------------------------------------------------------------- //


// --- tests --------------------------------------------------------------- //


#define check(x) assert(x)


int main()
{
    initTypeSys();
    try
    {
        Parser parser("x", new in_text("x"));

        variant r;
        ConstCode seg;

        {
            CodeGen gen(seg);
            gen.loadInt(1);
            gen.loadInt(9);
            gen.arithmBinary(opAdd);
            gen.loadInt(2);
            gen.arithmUnary(opNeg);
            gen.arithmBinary(opSub);
            gen.endConstExpr(queenBee->defaultInt);
        }
        seg.run(r);
        check(r.as_int() == 12);
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

