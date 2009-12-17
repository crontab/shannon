
#include "vm.h"


CodeSeg::CodeSeg(State* stateType)
    : rtobject(stateType), stackSize(0)  { }

CodeSeg::~CodeSeg()
    { }

bool CodeSeg::empty() const
    { return code.empty(); }


// --- VIRTUAL MACHINE ----------------------------------------------------- //


struct podvar { char data[sizeof(variant)]; };

static void invOpcode()             { fatal(0x5002, "Invalid opcode"); }
static void doExit()                { throw eexit(); }

template<class T>
    inline void PUSH(variant*& stk, const T& v)
        { ::new(++stk) variant(v);  }

inline void PUSH(variant*& stk, int type, object* obj)
        { ::new(++stk) variant(variant::Type(type), obj);  }

inline void POP(variant*& stk)
        { (*stk--).~variant(); }

inline void POPPOD(variant*& stk)
        { assert(!stk->is_refcnt()); stk--; }

inline void POPTO(variant*& stk, variant* dest)     // ... to uninitialized area
        { *(podvar*)dest = *(podvar*)stk; stk--; }

inline void STORETO(variant*& stk, variant* dest)   // pop and copy properly
        { dest->~variant(); POPTO(stk, dest); }

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
            { int t = ADV<uchar>(ip); PUSH(stk, t, ADV<object*>(ip)); } break;

        case opInitRet:     POPTO(stk, result); break;
        case opDeref:       { *stk = cast<reference*>(stk->as_rtobj())->var; } break;
        case opPop:         POP(stk); break;
        case opChrToStr:    { *stk = str(stk->_uchar()); } break;
        case opVarToVec:    { varvec v; v.push_back(*stk); *stk = v; } break;

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

