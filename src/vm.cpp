
#include <stdlib.h>
#include <stdio.h>

#include "vm.h"
#include "langobj.h"


FILE* echostm = stdout;


#ifdef SINGLE_THREADED

const char* fileName = "";
int lineNum = 0;

VmStack stk;

#endif


static void runtimeError(int code, const char* msg)
{
    if (lineNum > 0)
        fprintf(stderr, "\nRuntime error [%d] %s(%d): %s\n", code, fileName, lineNum, msg);
    else
        fprintf(stderr, "\nRuntime error [%d]: %s\n", code, msg);
    throw code;
}

// TODO: probably only <= is enough (i.e. return only 0 or < 0), the rest can 
// be optimized by the code generator.
static int compareInt(int a, int b)
    { if (a < b) return -1; else if (a == b) return 0; return 1; }

static int compareLarge(large a, large b)
    { if (a < b) return -1; else if (a == b) return 0; return 1; }

static int compareStrChr(ptr a, int b)
    { return PTR_TO_STRING(a).compare(string(char(b))); }

static int compareStr(ptr a, ptr b)
    { return PTR_TO_STRING(a).compare(PTR_TO_STRING(b)); }


static ptr catVecElem(ShType* type)
{
    StorageModel sto = type->storageModel();
    ptr* l = NULL;
    switch (sto)
    {
        case stoByte:
            {
                int elem = stk.popInt();
                l = stk.topPtrRef();
                PTR_TO_STRING(*l).append(char(elem));
            }
            break;
        case stoInt:
            {
                int elem = stk.popInt();
                l = stk.topPtrRef();
                *pint(PTR_TO_STRING(*l).appendn(4)) = elem;
            }
            break;
        case stoLarge:
            {
                large elem = stk.popLarge();
                l = stk.topPtrRef();
                *plarge(PTR_TO_STRING(*l).appendn(8)) = elem;
            }
            break;
        case stoPtr:
            {
                ptr elem = stk.popPtr();
                l = stk.topPtrRef();
                *pptr(PTR_TO_STRING(*l).appendn(sizeof(ptr))) = elem;
            }
            break;
        case stoVec:
            {
                ptr elem = string::_initialize(stk.popPtr());
                l = stk.topPtrRef();
                if (PTR_TO_STRING(*l).refcount() > 1)
                    runtimeError(2, "(Internal) Refcount > 1");
                *pptr(PTR_TO_STRING(*l).appendn(sizeof(ptr))) = elem;
            }
            break;
        default: runtimeError(2, "(Internal) Unknown type");
    }
    return *l;
}


static void popByType(ShType* type, ShValue& result)
{
    switch (type->storageModel())
    {
        case stoByte:
        case stoInt: result.assignInt(type, stk.popInt()); break;
        case stoLarge: result.assignLarge(type, stk.popLarge()); break;
        case stoPtr: result.assignPtr(type, stk.popPtr()); break;
        case stoVec: 
            {
                ptr p = stk.popPtr();
                result.assignVec(type, PTR_TO_STRING(p));
            }
            break;
        case stoVoid: result.assignVoid(type); break;
        default: internal(58);
    }
}

static void doEcho(ShType* type)
{
    ShValue value;
    popByType(type, value);
    string s;
    if (type->isString())
        s = PTR_TO_STRING(value.value.ptr_);
    else if (type->isChar())
        s = char(value.value.int_);
    else
        s = type->displayValue(value);
    fwrite(s.c_bytes(), s.size(), 1, echostm);
}

static void doAssert(const char* fn, int linenum)
{
    if (!stk.popInt())
    {
        string s = string("Assertion failed: ") + fn + '(' + itostring(linenum) + ')';
        runtimeError(1, s.c_str());
    }
}

static void doLinenum(const char* fn, int ln)
{
    fileName = fn;
    lineNum = ln;
    // printf("VM: %s(%d)\n", fn, ln);
}

static ptr itostr10(large v)
{
    return itostring(v)._initialize();
}


#define GEN_LOADERS(KIND,PTR) \
    case opLoad##KIND##Byte: stk.pushInt(*puchar(PTR)); break; \
    case opLoad##KIND##Int: stk.pushInt(*pint(PTR)); break; \
    case opLoad##KIND##Large: stk.pushLarge(*plarge(PTR)); break; \
    case opLoad##KIND##Ptr: \
    case opLoad##KIND##Vec: stk.pushPtr(*pptr(PTR)); break; \
    case opLoad##KIND##Void: break;

#define GEN_STORERS(KIND,PTR) \
    case opStore##KIND##Byte: { int t = stk.popInt(); *puchar(PTR) = t; } break; \
    case opStore##KIND##Int: { int t = stk.popInt(); *pint(PTR) = t; } break; \
    case opStore##KIND##Large: { large t = stk.popLarge(); *plarge(PTR) = t; } break; \
    case opStore##KIND##Ptr: { ptr t  = stk.popPtr(); *pptr(PTR) = t; } break; \
    case opStore##KIND##Vec: { ptr t = stk.popPtr(); pptr s = pptr(PTR); \
            string::_finalize(*s); *s = string::_initialize(t); } break; \
    case opStore##KIND##Void: break; \
    case opInit##KIND##Vec: { ptr t = stk.popPtr(); *pptr(PTR) = string::_initialize(t); } break; \
    case opFin##KIND##PodVec: { string::_finalize(*pptr(PTR)); } break; \
    case opFin##KIND##Vec: { ptr t = (p++)->ptr_; PVector(t)->rtFinalize(PTR); } break;


// TODO: try to pass the stack pointer as an arg to this func and see the difference in asm.

void VmCodeSegment::run(VmQuant* p, pchar dataseg, pchar stkbase, ptr retval)
{
    while (1)
    {
        switch ((p++)->op_)
        {
        case opEnd: return;
        case opNop: break;

        // --- FUNCTION RETURN VALUE---------------------------------------- //

        case opRetByte:
        case opRetInt: *pint(retval) = stk.popInt(); break;
        case opRetLarge: *plarge(retval) = stk.popLarge(); break;
        case opRetPtr: *pptr(retval) = stk.popPtr(); break;
        case opRetVec: *pptr(retval) = string::_initialize(stk.popPtr());
        case opRetVoid: break;

        // --- LOAD/STORE -------------------------------------------------- //

        case opLoadZero: stk.pushInt(0); break;
        case opLoadLargeZero: stk.pushLarge(0); break;
        case opLoadOne: stk.pushInt(1); break;
        case opLoadLargeOne: stk.pushLarge(1); break;
        case opLoadIntConst: stk.pushInt((p++)->int_); break;
#ifdef PTR64
        case opLoadLargeConst: stk.pushLarge((p++)->large_); break;
#else
        case opLoadLargeConst:
            { int lo = (p++)->int_; stk.pushLarge(lo, (p++)->int_); } break;
#endif
        case opLoadFalse: stk.pushInt(0); break;
        case opLoadTrue: stk.pushInt(1); break;
        case opLoadNull: stk.pushPtr(NULL); break;
        case opLoadNullVec: stk.pushPtr(emptystr); break;
        case opLoadVecConst: stk.pushPtr((p++)->ptr_); break;
        case opLoadTypeRef: stk.pushPtr((p++)->ptr_); break;

        // --- VAR LOAD/STORE ---------------------------------------------- //

        GEN_LOADERS(This, dataseg + (p++)->offs_)
        GEN_STORERS(This, dataseg + (p++)->offs_)

        GEN_LOADERS(Loc, stkbase + (p++)->offs_)
        GEN_STORERS(Loc, stkbase + (p++)->offs_)
        
        // --- SOME VECTOR MAGIC ------------------------------------------- //

        case opCopyToTmpVec:
            *pptr(stkbase + (p++)->offs_) = string::_initialize(stk.topPtr());
            break;
        case opElemToVec:
            {
                ShType* type = PType((p++)->ptr_);
if (type->isVector()) runtimeError(2, "ELEM2VEC: unknown type");
                int size = type->staticSize();
                char* vec = pchar(string::_initializen(size));
                if (size > 4)
                    *plarge(vec) = stk.popLarge();
                else if (size > 1)
                    *pint(vec) = stk.popInt();
                else
                    *puchar(vec) = uchar(stk.popInt());
                stk.pushPtr(*pptr(stkbase + (p++)->offs_) = vec);
            }
            break;
        case opVecCat:
            {
                ShType* type = PType((p++)->ptr_);
if (type->isVector()) runtimeError(2, "VECCAT: unknown type");
                ptr r = stk.popPtr();
                ptr* l = stk.topPtrRef();
                PTR_TO_STRING(*l).append(PTR_TO_STRING(r));
                *pptr(stkbase + (p++)->offs_) = *l;
            }
            break;
        case opVecElemCat:
            { PType type = PType((p++)->ptr_); *pptr(stkbase + (p++)->offs_) = catVecElem(type); } break;


        // --- COMPARISONS ------------------------------------------------- //

        case opCmpInt: { int r = stk.popInt(); int* t = stk.topIntRef(); *t = compareInt(*t, r); } break;
        case opCmpLarge: { large r = stk.popLarge(); stk.pushInt(compareLarge(stk.popLarge(), r)); } break;
        case opCmpStr: { ptr r = stk.popPtr(); ptr l = stk.popPtr(); stk.pushInt(compareStr(l, r)); } break;
        case opCmpStrChr: { int r = stk.popInt(); ptr l = stk.popPtr(); stk.pushInt(compareStrChr(l, r)); } break;
        case opCmpChrStr: { ptr r = stk.popPtr(); int* t = stk.topIntRef(); *t = -compareStrChr(r, *t); } break;
        case opCmpPodVec:
            { ptr r = stk.popPtr(); ptr l = stk.popPtr(); stk.pushInt(!PTR_TO_STRING(l).equal(PTR_TO_STRING(r))); } break;
        case opCmpPtr: stk.pushInt(!(stk.popPtr() == stk.popPtr())); break;

        case opEQ: { int* t = stk.topIntRef(); *t = *t == 0; } break;
        case opLT: { int* t = stk.topIntRef(); *t = *t < 0; } break;
        case opLE: { int* t = stk.topIntRef(); *t = *t <= 0; } break;
        case opGE: { int* t = stk.topIntRef(); *t = *t >= 0; } break;
        case opGT: { int* t = stk.topIntRef(); *t = *t > 0; } break;
        case opNE: { int* t = stk.topIntRef(); *t = *t != 0; } break;

        // typecasts
        case opLargeToInt: stk.pushInt(stk.popLarge()); break;
        case opIntToLarge: stk.pushLarge(stk.popInt()); break;
        case opIntToStr: stk.pushPtr(*pptr(stkbase + (p++)->offs_) = itostr10(stk.popInt())); break;
        case opLargeToStr: stk.pushPtr(*pptr(stkbase + (p++)->offs_) = itostr10(stk.popLarge())); break;

        // --- BINARY OPERATORS -------------------------------------------- //
#ifdef PTR64
        case opMkSubrange:
            {
                large hi = large(stk.popInt()) << 32;
                stk.pushLarge(unsigned(stk.popInt()) | hi);
            }
            break;
#else
        case opMkSubrange: /* two ints become a subrange, haha! */ break;
#endif

        case opAdd: { int r = stk.popInt(); *stk.topIntRef() += r; } break;
        case opAddLarge: { stk.pushLarge(stk.popLarge() + stk.popLarge()); } break;
        case opSub: { int r = stk.popInt(); *stk.topIntRef() -= r; } break;
        case opSubLarge: { stk.pushLarge(stk.popLarge() - stk.popLarge()); } break;
        case opMul: { int r = stk.popInt(); *stk.topIntRef() *= r; } break;
        case opMulLarge: { stk.pushLarge(stk.popLarge() * stk.popLarge()); } break;
        case opDiv: { int r = stk.popInt(); *stk.topIntRef() /= r; } break;
        case opDivLarge: { stk.pushLarge(stk.popLarge() / stk.popLarge()); } break;
        case opMod: { int r = stk.popInt(); *stk.topIntRef() %= r; } break;
        case opModLarge: { stk.pushLarge(stk.popLarge() % stk.popLarge()); } break;
        case opBitAnd: { int r = stk.popInt(); *stk.topIntRef() &= r; } break;
        case opBitAndLarge: { stk.pushLarge(stk.popLarge() & stk.popLarge()); } break;
        case opBitOr: { int r = stk.popInt(); *stk.topIntRef() |= r; } break;
        case opBitOrLarge: { stk.pushLarge(stk.popLarge() | stk.popLarge()); } break;
        case opBitXor: { int r = stk.popInt(); *stk.topIntRef() ^= r; } break;
        case opBitXorLarge: { stk.pushLarge(stk.popLarge() ^ stk.popLarge()); } break;
        case opBitShl: { int r = stk.popInt(); *stk.topIntRef() <<= r; } break;
        case opBitShlLarge: { stk.pushLarge(stk.popLarge() << stk.popInt()); } break;
        case opBitShr: { int r = stk.popInt(); *stk.topIntRef() >>= r; } break;
        case opBitShrLarge: { stk.pushLarge(stk.popLarge() >> stk.popInt()); } break;

        // --- UNARY OPERATORS --------------------------------------------- //

        case opNeg: { int* t = stk.topIntRef(); *t = -*t; } break;
        case opNegLarge: { stk.pushLarge(-stk.popLarge()); } break;
        case opBitNot: { int* t = stk.topIntRef(); *t = ~*t; } break;
        case opBitNotLarge: { stk.pushLarge(~stk.popLarge()); } break;
        case opBoolNot: { int* t = stk.topIntRef(); *t = !*t; } break;

        // --- JUMPS ------------------------------------------------------- //

        case opJumpOr: if (stk.topInt()) p += p->offs_; else stk.popInt(); p++; break;
        case opJumpAnd: if (stk.topInt()) stk.popInt(); else p += p->offs_; p++; break;
        case opJumpTrue: if (stk.popInt()) p += p->offs_; p++; break;
        case opJumpFalse: if (!stk.popInt()) p += p->offs_; p++; break;
        case opJump: p += p->offs_; p++; break;

        // --- MISC. ------------------------------------------------------- //

        case opEcho: doEcho((ShType*)(p++)->ptr_); break;
        case opEchoSp: putc(' ', echostm); break;
        case opEchoLn: fputs("\n", echostm); break;
        case opAssert: { pconst fn = pconst((p++)->ptr_); doAssert(fn, (p++)->int_); } break;
        case opLinenum: { pconst fn = pconst((p++)->ptr_); doLinenum(fn, (p++)->int_); } break;

        default: fatal(CRIT_FIRST + 50, ("[VM] Unknown opcode " + itostring((--p)->op_, 16, 8, '0')).c_str());
        }
    }
}



VmCodeSegment::VmCodeSegment()
        : code(), reserveStack(0), reserveLocals(0)  { }

void VmCodeSegment::append(const VmCodeSegment& seg)
{
    reserveStack = imax(reserveStack, seg.reserveStack);
    reserveLocals = imax(reserveLocals, seg.reserveLocals);
    code.append(seg.code);
}


void VmCodeSegment::execute(pchar dataseg, ptr retval)
{
    if (code.empty())
        return;

    // TODO: function arguments (no need for finalization)

    // make room on the stack for this function so that no reallocations are needed
    stk.reservebytes(reserveStack + reserveLocals);

    // reserve space for locals and temps; to be restored to this point later.
    // we also reset the local storage to zero so that any unused tempvars can 
    // be finalized properly, and in the future this will help to unwind the 
    // stack after an exception.
    char* savebase = stk.pushrbytes(reserveLocals);
    memset(savebase, 0, reserveLocals);

    // run, rabbit, run!
    run(getCode(), dataseg, savebase, retval);

    // restore stack base
    stk.restoreendr(savebase);
}



