

#include "common.h"
#include "runtime.h"
#include "parser.h"
#include "typesys.h"
#include "vm.h"


// TODO: less loaders: for args and locals one instruction can be used
// TODO: assert: use constant repository for file names
// TODO: multiple return values probably not needed
// TODO: store the current file name in a named const, say __FILE__


class CodeGen: noncopyable
{
protected:
    State* codeOwner;
    CodeSeg* codeseg;

    objvec<Type> simStack;      // exec simulation stack
    memint locals;              // number of local vars currently on the stack
    memint maxStack;            // total stack slots needed without function calls
    memint lastOpOffs;

    template <class T>
        void add(const T& t)    { codeseg->append<T>(t); }
    memint addOp(OpCode op);
    memint addOp(OpCode op, object* o);
    void stkPush(Type*);
    Type* stkPop();
    void stkReplaceTop(Type* t);
    Type* stkTop()
        { return simStack.back(); }
    static void error(const char*);

    void canAssign(Type* from, Type* to, const char* errmsg = NULL);
    bool tryImplicitCast(Type*);
    void implicitCast(Type*, const char* errmsg = NULL);

public:
    CodeGen(State*);
    CodeGen(CodeSeg*);  // for const expressions
    ~CodeGen();
    
     // NOTE: compound consts shoudl be held by a smart pointer somewhere else
    void loadConst(Type*, const variant&);
    void initRet()
        { addOp(opInitRet); stkPop(); }
    void end();
    void runConstExpr(Type* expectType, variant& result);
};


// ------------------------------------------------------------------------- //


void CodeGen::error(const char* msg)
    { throw ecmessage(msg); }


CodeGen::CodeGen(State* o)
    : codeOwner(o), codeseg(o->code), locals(0), maxStack(0), lastOpOffs(-1)  { }

CodeGen::CodeGen(CodeSeg* c)
    : codeOwner(NULL), codeseg(c), locals(0), maxStack(0), lastOpOffs(-1)  { }

CodeGen::~CodeGen()
    { }


memint CodeGen::addOp(OpCode op)
{
    lastOpOffs = codeseg->size();
    add<uchar>(op);
    return lastOpOffs;
}


memint CodeGen::addOp(OpCode op, object* o)
{
    memint r = addOp(op);
    add(o);
    return r;
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

    if (to->isVec() && from->canAssignTo(PContainer(to)->elem))
    {
        addOp(from->isSmallOrd() ? opChrToStr : opElmToVec);
        stkReplaceTop(to);
        return true;
    }
    
    return false;
}


void CodeGen::implicitCast(Type* to, const char* errmsg)
{
    if (!tryImplicitCast(to))
        error(errmsg == NULL ? "Type mismatch" : errmsg);
}


void CodeGen::loadConst(Type* type, const variant& value)
{
    // variant: NONE, ORD, REAL, STR, VEC, SET, ORDSET, DICT, RTOBJ
    // Type: TYPEREF, NONE, VARIANT, REF, BOOL, CHAR, INT, ENUM,
    //       NULLCONT, VEC, SET, DICT, FIFO, FUNC, PROC, OBJECT, MODULE
    switch (type->typeId)
    {
    case Type::TYPEREF: addOp(opLoadTypeRef, value._rtobj()); break;
    case Type::NONE: addOp(opLoadNull); break;
    case Type::VARIANT: error("Variant constants are not supported"); break;
    case Type::REF: error("Reference constants are not supported"); break;
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
    default: notimpl();
    }
    stkPush(type);
}


void CodeGen::end()
{
    codeseg->close(maxStack);
    assert(simStack.size() - locals == 0);
}


void CodeGen::runConstExpr(Type* expectType, variant& result)
{
    if (expectType != NULL)
        implicitCast(expectType, "Constant expression type mismatch");
    initRet();
    end();
    varpool stack;
    result.clear();
    codeseg->run(stack, NULL, &result);
    assert(stack.size() == 0);
}


// --- VIRTUAL MACHINE ----------------------------------------------------- //


struct podvar { char data[sizeof(variant)]; };

static void invOpcode()             { fatal(0x5002, "Invalid opcode"); }
static void doExit()                { throw eexit(); }

template<class T>
    inline void PUSH(variant*& stk, const T& v)
        { ::new(++stk) variant(v);  }

inline void PUSH(variant*& stk, variant::Type type, object* obj)
        { ::new(++stk) variant(type, obj);  }

inline void POP(variant*& stk)
        { (*stk--).~variant(); }

inline void POPPOD(variant*& stk)
        { assert(!stk->is_refcnt()); stk--; }

inline void POPTO(variant*& stk, variant* dest)     // ... to uninitialized area
        { *(podvar*)dest = *(podvar*)stk; stk--; }

inline void STORETO(variant*& stk, variant* dest)   // pop and copy properly
        { dest->~variant(); POPTO(stk, dest); }
//        { *dest = *stk; POP(stk); }

#define SETPOD(dest,v) (::new(dest) variant(v))

#define BINARY_INT(op) { (stk - 1)->_intw() op stk->_int(); POPORD(stk); }
#define UNARY_INT(op)  { stk->_intw() = op stk->_int(); }


void CodeSeg::run(varpool& stack, rtobject* self, variant* result)
{
    // Make sure there's NULL char (opEnd) at the end
    register const uchar* ip = (const uchar*)code.c_str();

    // stk always points at an exisitng top element
    variant* stkbase = stack.reserve(stackSize);
    register variant* stk = stkbase - 1;

    try
    {
loop:
        switch(*ip++)
        {
        case opEnd:         goto exit;
        case opNop:         break;
        case opExit:        doExit(); break;

        case opLoadTypeRef: PUSH(stk, ADV<Type*>(ip)); break;
        case opLoadNull:    PUSH(stk, variant::null); break;
        case opLoad0:       PUSH(stk, integer(0)); break;
        case opLoad1:       PUSH(stk, integer(1)); break;
        case opLoadOrd8:    PUSH(stk, integer(ADV<uchar>(ip))); break;
        case opLoadOrd:     PUSH(stk, ADV<integer>(ip)); break;
        case opLoadConstObj:
            { uchar t = ADV<uchar>(ip); PUSH(stk, variant::Type(t), ADV<object*>(ip)); } break;

        case opInitRet:     POPTO(stk, result); break;
        case opChrToStr:    notimpl();
        case opElmToVec:    notimpl();

        default:            invOpcode(); break;
        }
        goto loop;
exit:
        if (stk != stkbase - 1)
            fatal(0x5001, "Internal: stack unbalanced (corrupt code?)");
        stack.free(stackSize);
    }
    catch(exception&)
    {
        while (stk >= stkbase)
            POP(stk);
        stack.free(stackSize);
        throw;
    }
}


// --- HIS MAJESTY THE COMPILER -------------------------------------------- //


// --- tests --------------------------------------------------------------- //


#include "typesys.h"


void ut_fail(unsigned line, const char* e)
{
    fprintf(stderr, "%s:%u: test failed `%s'\n", __FILE__, line, e);
    exit(200);
}

#define fail(e)     ut_fail(__LINE__, e)
#define check(e)    { if (!(e)) fail(#e); }

#define check_throw(a) \
    { bool chk_throw = false; try { a; } catch(exception&) { chk_throw = true; } check(chk_throw); }


void test_vm1()
{
    {
        CodeSeg code;
        CodeGen gen(&code);
        gen.loadConst(queenBee->defInt, 21);
        variant result;
        gen.runConstExpr(queenBee->defInt, result);
        check(result == 21);
    }
}


int main()
{
    printf("%lu %lu\n", sizeof(object), sizeof(container));

    initTypeSys();
    
    test_vm1();
    
    doneTypeSys();

    if (object::allocated != 0)
    {
        fprintf(stderr, "object::allocated: %d\n", object::allocated);
        _fatal(0xff01);
    }

    return 0;
}
