
#include "common.h"
#include "typesys.h"
#include "vm.h"


// --- CODE SEGMENT -------------------------------------------------------- //


CodeSeg::CodeSeg(State* _state, Context* _context)
  : stksize(0), state(_state), context(_context)
#ifdef DEBUG
    , closed(false)
#endif
    { }

CodeSeg::~CodeSeg()  { }

void CodeSeg::clear()
{
    code.clear();
    consts.clear();
    stksize = 0;
#ifdef DEBUG
    closed = false;
#endif
}

void CodeSeg::add8(uint8_t i)
    { code.push_back(i); }

void CodeSeg::add16(uint16_t i)
    { code.append((char*)&i, 2); }

void CodeSeg::addInt(integer i)
    { code.append((char*)&i, sizeof(i)); }

void CodeSeg::addPtr(void* p)
    { code.append((char*)&p, sizeof(p)); }


void CodeSeg::close(mem _stksize)
{
    assert(!closed);
    if (!code.empty())
        add8(opEnd);
    stksize = _stksize;
#ifdef DEBUG
    closed = true;
#endif
}


static void invOpcode()        { throw emessage("Invalid opcode"); }


template<class T>
    inline void PUSH(variant*& stk, const T& v)
        { ::new(++stk) variant(v);  }

inline void POP(variant*& stk)
        { (*stk--).~variant(); }

inline void POPTO(variant*& stk, variant* dest)
        { assert(dest->is_null()); *(podvar*)dest = *(podvar*)stk; stk--; }

template<class T>
    inline T IPADV(const uchar*& ip)
        { T t = *(T*)ip; ip += sizeof(T); return t; }

#define SETPOD(stk,v) (::new(stk) variant(v))

#define BINARY_INT(op) { (stk - 1)->_int_write() op stk->_int(); POP(stk); }
#define UNARY_INT(op)  { stk->_int_write() = op stk->_int(); }


void CodeSeg::varToVec(Vector* type, const variant& elem, variant* result)
{
    *result = new vector(type, 1, elem);
}


void CodeSeg::varCat(const variant& elem, variant* vec)
{
    assert(vec->as_object()->get_rt()->isVector());
    vector* t = (vector*)vec->_object();
    Vector* type = CAST(Vector*, t->get_rt());
    if (t == NULL)
        varToVec(type, elem, vec);
    else
        t->push_back(elem);
}


void CodeSeg::vecCat(const variant& src, variant* dest)
{
    vector* ts = (vector*)src._object();
    if (ts == NULL)
        return;
    vector* td = (vector*)dest->_object();
    if (td == NULL)
        *dest = src;
    else
    {
        assert(td->get_rt()->isVector());
        assert(ts->get_rt()->isVector());
        td->append(*ts);
    }
}


void CodeSeg::run(langobj* self, varstack& stack, variant* result) const
{
    if (code.empty())
        return;

    assert(closed);
    assert(self == NULL || self->get_rt()->canCastImplTo(state));

    register const uchar* ip = (const uchar*)code.data();
    register variant* stk = stack.reserve(stksize) - 1; // always points to the top element
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
            case opLoadChar:        PUSH(stk, IPADV<uint8_t>(ip)); break;
            case opLoad0:           PUSH(stk, integer(0)); break;
            case opLoad1:           PUSH(stk, integer(1)); break;
            case opLoadInt:         PUSH(stk, IPADV<integer>(ip)); break;
            case opLoadNullStr:     PUSH(stk, null_str); break;
            case opLoadNullRange:   PUSH(stk, (object*)NULL); break;
            case opLoadNullVec:     PUSH(stk, (object*)NULL); break;
            case opLoadNullDict:    PUSH(stk, (object*)NULL); break;
            case opLoadNullOrdset:  PUSH(stk, (object*)NULL); break;
            case opLoadNullSet:     PUSH(stk, (object*)NULL); break;
            case opLoadConst:       PUSH(stk, consts[IPADV<uint8_t>(ip)]); break;
            case opLoadConst2:      PUSH(stk, consts[IPADV<uint16_t>(ip)]); break;
            case opLoadTypeRef:     PUSH(stk, IPADV<Type*>(ip)); break;
            
            case opPop:             POP(stk); break;
            case opSwap:            varswap(stk, stk - 1); break;

            // Safe typecasts
            case opToBool:      *stk = stk->to_bool(); break;
            case opToStr:       *stk = stk->to_string(); break;
            case opToType:      IPADV<Type*>(ip)->runtimeTypecast(*stk); break;
            case opToTypeRef:   CAST(Type*, stk->_object())->runtimeTypecast(*(stk - 1)); POP(stk); break;
            case opIsType:      *stk = IPADV<Type*>(ip)->isMyType(*stk); break;
            case opIsTypeRef:   *(stk - 1) = CAST(Type*, stk->_object())->isMyType(*(stk - 1)); POP(stk); break;

            // Arithmetic
            // TODO: range checking in debug mode
            case opAdd:         BINARY_INT(+=); break;
            case opSub:         BINARY_INT(-=); break;
            case opMul:         BINARY_INT(*=); break;
            case opDiv:         BINARY_INT(/=); break;
            case opMod:         BINARY_INT(%=); break;
            case opBitAnd:      BINARY_INT(&=); break;
            case opBitOr:       BINARY_INT(|=); break;
            case opBitXor:      BINARY_INT(^=); break;
            case opBitShl:      BINARY_INT(<<=); break;
            case opBitShr:      BINARY_INT(>>=); break;
            case opNeg:         UNARY_INT(-); break;
            case opBitNot:      UNARY_INT(~); break;
            case opNot:         UNARY_INT(-); break;

            // Vector/string concatenation
            case opCharToStr:   *stk = str(1, stk->_uchar()); break;
            case opCharCat:     (stk - 1)->_str_write().push_back(stk->_uchar()); POP(stk); break;
            case opStrCat:      (stk - 1)->_str_write().append(stk->_str_read()); POP(stk); break;
            case opVarToVec:    varToVec(IPADV<Vector*>(ip), *stk, stk); break;
            case opVarCat:      varCat(*stk, stk - 1); POP(stk); break;
            case opVecCat:      vecCat(*stk, stk - 1); POP(stk); break;

            // Range operations
            case opMkRange:     *(stk - 1) = new range(IPADV<Ordinal*>(ip), (stk - 1)->_ord(), stk->_ord()); POP(stk); break;
            case opInRange:     *(stk - 1) = ((range*)stk->_object())->has((stk - 1)->_ord()); POP(stk); break;

            // Comparators
            case opCmpOrd:      *(stk - 1) = (stk - 1)->_ord() - stk->_ord(); POP(stk); break;
            case opCmpStr:      *(stk - 1) = (stk - 1)->_str_read().compare(stk->_str_read()); POP(stk); break;
            case opCmpVar:      *(stk - 1) = int(*(stk - 1) == *stk) - 1; POP(stk); break;

            case opEqual:       SETPOD(stk, stk->_int() == 0); break; // TODO: optimize (just assign BOOL type)
            case opNotEq:       SETPOD(stk, stk->_int() != 0); break;
            case opLessThan:    SETPOD(stk, stk->_int() < 0); break;
            case opLessEq:      SETPOD(stk, stk->_int() <= 0); break;
            case opGreaterThan: SETPOD(stk, stk->_int() > 0); break;
            case opGreaterEq:   SETPOD(stk, stk->_int() >= 0); break;

            // Initializers:
            case opInitRet:     if (result != NULL) POPTO(stk, result); break;
            case opInitLocal:   POPTO(stk, stkbase + IPADV<uchar>(ip)); break;
            case opInitThis:    POPTO(stk, (*self)[IPADV<uchar>(ip)]); break;

            default: invOpcode(); break;
            }
        }
exit:
        if (stk != stkbase)
            fatal(0x5001, "Stack unbalanced");
        stack.free(stksize);
    }
    catch(exception&)
    {
        while (stk > stkbase)
            POP(stk);
        // TODO: stack is not free()'d here
        stack.free(stksize);
        throw;
    }
}


void ConstCode::run(variant& result) const
{
    result.clear();
    varstack stack;
    CodeSeg::run(NULL, stack, &result);
    assert(stack.size() == 0);
}


Context::Context()
{
    registerModule("system", queenBee);
}


Context::~Context()  { }


Module* Context::registerModule(const str& name, Module* module)
{
    assert(module->id == defs.size());
    if (defs.size() == 255)
        throw emessage("Maximum number of modules reached");
    objptr<ModuleAlias> alias = new ModuleAlias(name, module);
    addUnique(alias);   // may throw
    defs.add(alias);
    modules.add(module);
    module->setName(name);
    return module;
}


Module* Context::addModule(const str& name)
{
    objptr<Module> module = new Module(this, defs.size());
    return registerModule(name, module);
}


void Context::run(varstack& stack)
{
    // TODO: call a virtual init method for all modules
    assert(datasegs.empty());
    assert(modules.size() == defs.size());

    mem count = defs.size();
    for (mem i = 0; i < count; i++)
        datasegs.add(modules[i]->newObject());

    assert(modules[0] == queenBee);
    queenBee->initialize(datasegs[0]);

    mem level = 0;
    try
    {
        while (level < count)
        {
            modules[level]->run(datasegs[level], stack, NULL);
            level++;
        }
    }
    catch (exception&)
    {
        while (level--)
            modules[level]->finalize(datasegs[level], stack);
        throw;
    }

    while (level--)
        modules[level]->finalize(datasegs[level], stack);
    datasegs.clear();
}



