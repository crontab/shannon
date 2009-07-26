
#include "common.h"
#include "typesys.h"
#include "vm.h"


// --- CODE SEGMENT -------------------------------------------------------- //


CodeSeg::CodeSeg(Module* _module, State* _state)
  : stksize(0), hostModule(_module), ownState(_state), closed(0),
    fileId(mem(-1)), lineNum(mem(-1))  { }

CodeSeg::~CodeSeg()  { }


void CodeSeg::clear()
{
    code.clear();
    consts.clear();
    stksize = 0;
    closed = 0;
}


static void invOpcode()        { throw emessage("Invalid opcode"); }
static void idxOverflow()      { throw emessage("Index overflow"); }


template<class T>
    inline void PUSH(variant*& stk, const T& v)
        { ::new(++stk) variant(v);  }

inline void POP(variant*& stk)
        { (*stk--).~variant(); }

inline void POPORD(variant*& stk)
#ifdef DEBUG
        { stk->_ord(); stk--; }
#else
        { stk--; }
#endif

inline void POPTO(variant*& stk, variant* dest)
        { *(podvar*)dest = *(podvar*)stk; stk--; }

inline void STORETO(variant*& stk, variant* dest)
        { dest->~variant(); POPTO(stk, dest); }
//        { *dest = *stk; POP(stk); }

template<class T>
    inline T ADV(const uchar*& ip)
        { T t = *(T*)ip; ip += sizeof(T); return t; }

#define SETPOD(dest,v) (::new(dest) variant(v))

#define BINARY_INT(op) { (stk - 1)->_int_write() op stk->_int(); POPORD(stk); }
#define UNARY_INT(op)  { stk->_int_write() = op stk->_int(); }


void CodeSeg::vecCat(const variant& src, variant* dest)
{
    vector* ts = CAST(vector*, src._object());
    if (!ts->empty())
    {
        vector* td = CAST(vector*, dest->_object());
        if (td->empty())
            *dest = src;
        else
            td->append(*ts);
    }
}


void CodeSeg::failAssertion()
{
    throw emessage("Assertion failed: " + hostModule->fileNames[fileId]
        + " line " + to_string(lineNum));
}


void CodeSeg::echo(const variant& v)
{
    // The default dump() method uses apostrophes, which we don't need here,
    // or at least at the top level (nested strings and chars in containers
    // can be with apostrophes)
    if (v.is(variant::CHAR))
        sio << uchar(v._ord());
    else if (v.is(variant::STR))
        sio << v._str_read();
    else
        sio << v;
}


// --- The Virtual Machine ------------------------------------------------- //


void CodeSeg::run(varstack& stack, langobj* self, variant* result)
{
    if (code.empty())
        return;

    assert(closed);

    register const uchar* ip = (const uchar*)code.data();
    variant* stkbase = stack.reserve(stksize);
    register variant* stk = stkbase - 1; // always points to the top element
    try
    {
        while (1)
        {
            switch(*ip++)
            {
            case opInv:     invOpcode(); break;
            case opEnd:     goto exit;
            case opNop:     break;
            case opExit:    throw eexit();

            // Const loaders
            case opLoadNull:        PUSH(stk, null); break;
            case opLoadFalse:       PUSH(stk, false); break;
            case opLoadTrue:        PUSH(stk, true); break;
            case opLoadChar:        PUSH(stk, ADV<uchar>(ip)); break;
            case opLoad0:           PUSH(stk, integer(0)); break;
            case opLoad1:           PUSH(stk, integer(1)); break;
            case opLoadInt:         PUSH(stk, ADV<integer>(ip)); break;
            case opLoadNullRange:   PUSH(stk, new range(ADV<Range*>(ip))); break;
            case opLoadNullDict:    PUSH(stk, new dict(ADV<Dict*>(ip))); break;
            case opLoadNullStr:     PUSH(stk, null_str); break;
            case opLoadNullVec:     PUSH(stk, new vector(ADV<Vec*>(ip))); break;
            case opLoadNullArray:   PUSH(stk, new vector(ADV<Array*>(ip))); break;
            case opLoadNullOrdset:  PUSH(stk, new ordset(ADV<Ordset*>(ip))); break;
            case opLoadNullSet:     PUSH(stk, new set(ADV<Set*>(ip))); break;
            case opLoadConst:       PUSH(stk, consts[ADV<uchar>(ip)]); break;
            case opLoadConst2:      PUSH(stk, consts[ADV<uint16_t>(ip)]); break;
            case opLoadTypeRef:     PUSH(stk, ADV<Type*>(ip)); break;

            case opPop:             POP(stk); break;
            case opSwap:            varswap(stk, stk - 1); break;
            case opDup:             PUSH(stk, *stk); break;

            // Safe typecasts
            case opToBool:      *stk = stk->to_bool(); break;
            case opToStr:       *stk = stk->to_string(); break;
            case opToType:      ADV<Type*>(ip)->runtimeTypecast(*stk); break;
            case opToTypeRef:   CAST(Type*, stk->_object())->runtimeTypecast(*(stk - 1)); POP(stk); break;
            case opIsType:      *stk = ADV<Type*>(ip)->isMyType(*stk); break;
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
            case opBoolXor:     SETPOD(stk - 1, bool((stk - 1)->_ord() ^ stk->_ord())); POPORD(stk); break;
            case opNeg:         UNARY_INT(-); break;
            case opBitNot:      UNARY_INT(~); break;
            case opNot:         SETPOD(stk, ! stk->_ord()); break;

            // Range operations
            case opMkRange:     *(stk - 1) = new range(ADV<Ordinal*>(ip), (stk - 1)->_ord(), stk->_ord()); POPORD(stk); break;
            case opInRange:     SETPOD(stk - 1, CAST(range*, stk->_object())->has((stk - 1)->_ord())); POP(stk); break;

            // Comparators
            case opCmpOrd:      SETPOD(stk - 1, (stk - 1)->_ord() - stk->_ord()); POPORD(stk); break;
            case opCmpStr:      *(stk - 1) = (stk - 1)->_str_read().compare(stk->_str_read()); POP(stk); break;
            case opCmpVar:      *(stk - 1) = int(*(stk - 1) == *stk) - 1; POP(stk); break;

            case opEqual:       SETPOD(stk, stk->_int() == 0); break;
            case opNotEq:       SETPOD(stk, stk->_int() != 0); break;
            case opLessThan:    SETPOD(stk, stk->_int() < 0); break;
            case opLessEq:      SETPOD(stk, stk->_int() <= 0); break;
            case opGreaterThan: SETPOD(stk, stk->_int() > 0); break;
            case opGreaterEq:   SETPOD(stk, stk->_int() >= 0); break;

            // Initializers:
            case opInitRet:     POPTO(stk, result + ADV<uchar>(ip)); break;
            case opInitLocal:   POPTO(stk, stkbase + ADV<uchar>(ip)); break;
            case opInitThis:    POPTO(stk, self->var(ADV<uchar>(ip))); break;

            // Loaders
            case opLoadRet:     PUSH(stk, result[ADV<uchar>(ip)]); break;
            case opLoadLocal:   PUSH(stk, stkbase[ADV<uchar>(ip)]); break;
            case opLoadThis:    PUSH(stk, *self->var(ADV<uchar>(ip))); break;
            case opLoadArg:     PUSH(stk, stkbase[- ADV<uchar>(ip) - 1]); break; // not tested
            case opLoadStatic:
                {
                    Module* mod = ADV<Module*>(ip);
                    PUSH(stk, *mod->instance->var(ADV<uchar>(ip)));
                }
                break;
            case opLoadMember: *stk = *CAST(langobj*, stk->_object())->var(ADV<uchar>(ip)); break;
            case opLoadOuter:   notimpl();

            // Container read operations
            case opLoadDictElem:
                {
                    dict* d = CAST(dict*, (stk - 1)->_object());
                    dict_impl::const_iterator i = d->find(*stk);
                    POP(stk);
                    if (i == d->end()) stk->clear();
                    else *stk = i->second;
                }
                break;
            case opInDictKeys: *(stk - 1) = CAST(dict*, stk->_object())->has(*(stk - 1)); POP(stk); break;
            case opLoadStrElem:
                {
                    mem idx = stk->_int();
                    POPORD(stk);
                    const str& s = stk->_str_read();
                    if (idx >= s.size()) idxOverflow();
                    *stk = s[idx];
                }
                break;
            case opLoadVecElem:
            case opLoadArrayElem:
                {
                    mem idx = stk->_ord();
                    POPORD(stk);
                    vector* v = CAST(vector*, stk->_object());
                    if (*(ip - 1) == opLoadArrayElem)
                        idx -= CAST(Vec*, v->get_rt())->arrayIndexShift();
                    if (idx >= v->size()) idxOverflow();
                    *stk = (*v)[idx];
                }
                break;
            case opInOrdset:
                {
                    SETPOD(stk - 1, CAST(ordset*, stk->_object())->has((stk - 1)->_ord()));
                    POP(stk);
                }
                break;
            case opInSet: *(stk - 1) = CAST(set*, stk->_object())->has(*(stk - 1)); POP(stk); break;

            // Storers
            case opStoreRet:    STORETO(stk, result + ADV<uchar>(ip)); break;
            case opStoreLocal:  STORETO(stk, stkbase + ADV<uchar>(ip)); break;
            case opStoreThis:   STORETO(stk, self->var(ADV<uchar>(ip))); break;
            case opStoreArg:    STORETO(stk, stkbase - ADV<uchar>(ip) - 1); break;
            case opStoreStatic:
                {
                    Module* mod = ADV<Module*>(ip);
                    STORETO(stk, mod->instance->var(ADV<uchar>(ip)));
                }
                break;

            case opStoreMember:  notimpl();
            case opStoreOuter:   notimpl();

            // Container write operations
            case opStoreDictElem:
                {
                    dict* d = CAST(dict*, (stk - 2)->_object());
                    d->tie(*(stk - 1), *stk);
                    POP(stk); POP(stk); POP(stk);
                }
                break;
            case opDelDictElem: CAST(dict*, (stk - 1)->_object())->untie(*stk); POP(stk); POP(stk); break;
            case opStoreVecElem:
            case opStoreArrayElem:
                {
                    vector* v = CAST(vector*, (stk - 2)->_object());
                    mem idx = (stk - 1)->_ord();
                    if (*(ip - 1) == opStoreArrayElem)
                        idx -= CAST(Vec*, v->get_rt())->arrayIndexShift();
                    if (idx > v->size())
                        idxOverflow();
                    if (idx == v->size())
                        v->push_back(*stk);
                    else
                        v->put(idx, *stk);
                    POP(stk); POP(stk); POP(stk);
                }
                break;
            case opAddToOrdset:
                {
                    mem idx = stk->_ord();
                    POPORD(stk);
                    ordset* s = CAST(ordset*, stk->_object());
                    idx -= CAST(Set*, s->get_rt())->ordsetIndexShift();
                    if (idx >= charset::BITS)
                        idxOverflow();
                    s->tie(idx);
                    POP(stk);
                }
                break;
            case opAddToSet: CAST(set*, (stk - 1)->_object())->tie(*stk);  POP(stk); POP(stk); break;

            // Concatenation
            case opCharToStr:   *stk = str(1, stk->_uchar()); break;
            case opCharCat:     (stk - 1)->_str_write().push_back(stk->_uchar()); POPORD(stk); break;
            case opStrCat:      (stk - 1)->_str_write().append(stk->_str_read()); POP(stk); break;
            case opVarToVec:    *stk = new vector(ADV<Vec*>(ip), 1, *stk); break;
            case opVarCat:      CAST(vector*, (stk - 1)->_object())->push_back(*stk); POP(stk); break;
            case opVecCat:      vecCat(*stk, stk - 1); POP(stk); break;

            // Misc. built-ins
            case opEmpty:       *stk = stk->empty(); break;
            case opStrLen:      *stk = stk->_str_read().size(); break;
            case opVecLen:      *stk = CAST(vector*, stk->_object())->size(); break;
            case opRangeDiff:   *stk = CAST(range*, stk->_object())->diff(); break;
            case opRangeLow:    *stk = CAST(range*, stk->_object())->left; break;
            case opRangeHigh:   *stk = CAST(range*, stk->_object())->right; break;

            // Jumps
            case opJump:        { mem o = ADV<joffs_t>(ip); ip += o; } break; // beware of a strange bug in GCC, this should be done in 2 steps
            case opJumpTrue:    { mem o = ADV<joffs_t>(ip); if (stk->_ord()) ip += o; POP(stk); } break;
            case opJumpFalse:   { mem o = ADV<joffs_t>(ip); if (!stk->_ord()) ip += o; POP(stk); } break;
            case opJumpOr:      { mem o = ADV<joffs_t>(ip); if (stk->_ord()) ip += o; else POP(stk); } break;
            case opJumpAnd:     { mem o = ADV<joffs_t>(ip); if (stk->_ord()) POP(stk); else ip += o; } break;

            // Case labels
            case opCaseInt:     PUSH(stk, stk->_ord() == ADV<integer>(ip)); break;
            case opCaseRange:
                {
                    integer l = ADV<integer>(ip);
                    PUSH(stk, stk->_ord() >= l && stk->_ord() <= ADV<integer>(ip));
                }
                break;
            case opCaseStr:     *stk = stk->_str_read() == (stk - 1)->_str_read(); break;
            case opCaseTypeRef: *stk = CAST(Type*, stk->_object())->identicalTo(CAST(Type*, (stk - 1)->_object())); break;

            // Function call
            case opCall: notimpl();

            // Helpers
            case opEcho:        echo(*stk); POP(stk); break;
            case opEchoSpace:   sio << ' '; break;
            case opEchoLn:      sio << endl; break;
            case opLineNum:     { fileId = ADV<uint16_t>(ip); lineNum = ADV<uint16_t>(ip); } break;
            case opAssert:      { if (!stk->_ord()) failAssertion(); POP(stk); } break;

            default: invOpcode(); break;
            }
        }
exit:
        if (stk != stkbase - 1)
            fatal(0x5001, "Stack unbalanced");
        stack.free(stksize);
    }
    catch(exception&)
    {
        while (stk >= stkbase)
            POP(stk);
        stack.free(stksize);
        throw;
    }
}


void ConstCode::run(variant& result)
{
    result.clear();
    varstack stack;
    CodeSeg::run(stack, NULL, &result);
    assert(stack.size() == 0);
}

