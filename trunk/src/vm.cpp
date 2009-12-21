
#include "vm.h"


CodeSeg::CodeSeg(State* stateType) throw()
    : rtobject(stateType), stackSize(-1)  { }


CodeSeg::~CodeSeg() throw()
    { }


bool CodeSeg::empty() const
    { return code.empty(); }


void CodeSeg::close(memint s)
{
    if (stackSize == -1)
    {
        stackSize = s;
        append(opEnd);
    }
}


// --- VIRTUAL MACHINE ----------------------------------------------------- //


static void invOpcode()             { fatal(0x5002, "Invalid opcode"); }
static void doExit()                { throw eexit(); }

template<class T>
    inline T ADV(const char*& ip)
        { T t = *(T*)ip; ip += sizeof(T); return t; }

template<class T>
    inline void PUSH(variant*& stk, const T& v)
        { ::new(++stk) variant(v);  }

inline void PUSH(variant*& stk, int type, object* obj)
        { ::new(++stk) variant(variant::Type(type), obj);  }

inline void POP(variant*& stk)
        { (*stk--).~variant(); }

inline void POPPOD(variant*& stk)
        { assert(!stk->is_anyobj()); stk--; }

inline void POPTO(variant*& stk, variant* dest)     // ... to uninitialized area
        { *(podvar*)dest = *(podvar*)stk; stk--; }

inline void STORETO(variant*& stk, variant* dest)
        { dest->~variant(); POPTO(stk, dest); }

#define SETPOD(dest,v) (::new(dest) variant(v))

#define BINARY_INT(op) { (stk - 1)->_intw() op stk->_int(); POPORD(stk); }
#define UNARY_INT(op)  { stk->_intw() = op stk->_int(); }


void runRabbitRun(rtstack& stack, const char* ip, stateobj* self)
{
    // TODO: check for stack overflow
    register variant* stk = stack.bp - 1;
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
            { int t = ADV<uchar>(ip); PUSH(stk, t, ADV<object*>(ip)); } break;

        case opLoadSelfVar:     PUSH(stk, self->var(ADV<char>(ip))); break;
        case opLoadStkVar:      PUSH(stk, *(stack.bp + ADV<char>(ip))); break;

        case opStoreSelfVar:    STORETO(stk, &self->var(ADV<char>(ip))); break;
        case opStoreStkVar:     STORETO(stk, stack.bp + ADV<char>(ip)); break;

//        case opDeref:       { *stk = *(stk->_ptr()); } break;
        case opPop:         POP(stk); break;
        case opChrToStr:    { *stk = str(stk->_uchar()); } break;
        case opVarToVec:    { varvec v; v.push_back(*stk); *stk = v; } break;

        default:            invOpcode(); break;
        }
        goto loop;
exit:
        assert(stk == stack.bp - 1);
    }
    catch(exception&)
    {
        while (stk >= stack.bp)
            POP(stk);
        throw;
    }
}



void CodeGen::runConstExpr(Type* expectType, variant& result)
{
    storeRet(expectType);
    end();
    rtstack stack(codeseg.stackSize + 1);
    stack.push(variant::null);
    runRabbitRun(stack, codeseg.getCode(), NULL);
    stack.popto(result);
}
