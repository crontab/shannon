
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

static int typeRefsEqual(PType a, PType b)
{
    return a == b || a->equals(b);
}


void finalize(ShType* type, ptr data)
{
    switch (type->storageModel)
    {
        case stoVec:
        {
            if (PVector(type)->isPodVector())
                string::_finalize(*pptr(data));
            else
            {
                if (PTR_TO_STRING(*pptr(data))._unlock() == 0)
                {
                    pchar p = pchar(*pptr(data));
                    ShType* elementType = PVector(type)->elementType;
                    int itemSize = elementType->staticSize;
                    int count = PTR_TO_STRING(p).size() / itemSize - 1;
                    for (; count >= 0; count--, p += itemSize)
                        finalize(elementType, p);
                    PTR_TO_STRING(*pptr(data))._free();
                }
//                else
//                    PTR_TO_STRING(*pptr(data))._empty();
            }
        }
        break;

        default: internal(102);
    }
}


static void popByType(ShType* type, ShValue& result)
{
    switch (type->storageModel)
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
        default: internal(100);
    }
}


static void pushByType(ShType* type, podvalue& result)
{
    switch (type->storageModel)
    {
        case stoByte:
        case stoInt: stk.pushInt(result.int_); break;
        case stoLarge: stk.pushLarge(result.large_); break;
        case stoPtr: 
        case stoVec: stk.pushPtr(result.ptr_); break;
        case stoVoid: break;
        default: internal(104);
    }
}


static void copyElems(ShType* elemType, ptr src_, ptr dst_, int count)
{
    pptr src = pptr(src_); // couldn't find a better way to please gcc 4.x
    pptr dst = pptr(dst_);
    switch (elemType->storageModel)
    {
        case stoVec:
            if (count > 0)
                while (1)
                {
                    *pptr(dst) = string::_initialize(*pptr(src));
                    if (--count == 0)
                        break;
                    src++; dst++;
                }
            break;
        default: internal(103);
    }
}


static void growNonPodVec(ShType* elemType, ptr* pvec, int count)
{
    ptr vec = *pvec;
    if (count == 0)
        return;
    if (PTR_TO_STRING(vec).empty())
        *pvec = string::_new(count * elemType->staticSize);
    else if (PTR_TO_STRING(vec).refcount() == 1)
        *pvec = string::_grow(vec, count * elemType->staticSize);
    else
    {
        int oldsize = PTR_TO_STRING(vec).size();
        ptr newvec = string::_new(oldsize + (count * elemType->staticSize));
        copyElems(elemType, vec, newvec, oldsize / elemType->staticSize);
        string::_finalize(*pvec);
        *pvec = newvec;
    }
}


static ptr catVecElem(ShType* elemType)
{
    ptr* pvec = NULL;
    switch (elemType->storageModel)
    {
        case stoByte:
            {
                int elem = stk.popInt();
                pvec = stk.topPtrRef();
                PTR_TO_STRING(*pvec).append(char(elem));
            }
            break;
        case stoInt:
            {
                int elem = stk.popInt();
                pvec = stk.topPtrRef();
                *pint(PTR_TO_STRING(*pvec).appendn(4)) = elem;
            }
            break;
        case stoLarge:
            {
                large elem = stk.popLarge();
                pvec = stk.topPtrRef();
                *plarge(PTR_TO_STRING(*pvec).appendn(8)) = elem;
            }
            break;
        case stoPtr:
            {
                ptr elem = stk.popPtr();
                pvec = stk.topPtrRef();
                *pptr(PTR_TO_STRING(*pvec).appendn(sizeof(ptr))) = elem;
            }
            break;
        case stoVec:
            {
                ptr elem = string::_initialize(stk.popPtr());
                pvec = stk.topPtrRef();
                int oldsize = PTR_TO_STRING(*pvec).size();
                growNonPodVec(elemType, pvec, 1);
                *pptr((pchar(*pvec) + oldsize)) = elem;
            }
            break;
        default: internal(101);
    }
    return *pvec;
}


static ptr vecCat(ShType* elemType)
{
    ptr r = stk.popPtr();
    ptr* pvec = stk.topPtrRef();
    if (elemType->isPod())
        PTR_TO_STRING(*pvec).append(PTR_TO_STRING(r));
    else
    {
        int rcount = PTR_TO_STRING(r).size() / elemType->staticSize;
        int oldsize = PTR_TO_STRING(*pvec).size();
        growNonPodVec(elemType, pvec, rcount);
        copyElems(elemType, r, pchar(*pvec) + oldsize, rcount);
    }
    return *pvec;
}


static ptr elemToVec(ShType* type)
{
    ptr vec = string::_new(type->staticSize);
    switch (type->storageModel)
    {
        case stoByte: *puchar(vec) = uchar(stk.popInt());; break;
        case stoInt: *pint(vec) = stk.popInt(); break;
        case stoLarge: *plarge(vec) = stk.popLarge(); break;
        case stoPtr: *pptr(vec) = stk.popPtr(); break;
        case stoVec: *pptr(vec) = string::_initialize(stk.popPtr()); break;
        default: internal(106);
    }
    return vec;
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
    case opStore##KIND##Ptr: { ptr t = stk.popPtr(); *pptr(PTR) = t; } break; \
    case opStore##KIND##Vec: { ptr t = stk.popPtr(); *pptr(PTR) = string::_initialize(t); } break; \
    case opStore##KIND##Void: break; \
    case opFin##KIND##PodVec: { string::_finalize(*pptr(PTR)); } break; \
    case opFin##KIND: { PType t = PType((p++)->ptr_); finalize(t, PTR); } break;


// TODO: try to pass the stack pointer as an arg to this func and see the 
// difference in asm.

// TODO: use a static variable for comparison results, instead of pushing them 
// onto the stack

// TODO: indirect goto instead of switch?

void VmCodeSegment::run(VmQuant* p, pchar thisseg, pchar stkbase, ptr retval)
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
        case opRetVec: *pptr(retval) = string::_initialize(stk.popPtr()); break;
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
        case opLoadNullVec: stk.pushPtr(emptystr); break;
        case opLoadVecConst: stk.pushPtr((p++)->ptr_); break;
        case opLoadTypeRef: stk.pushPtr((p++)->ptr_); break;

        // --- VAR LOAD/STORE ---------------------------------------------- //

        GEN_LOADERS(This, thisseg + (p++)->offs_)
        GEN_STORERS(This, thisseg + (p++)->offs_)

        GEN_LOADERS(Loc, stkbase + (p++)->offs_)
        GEN_STORERS(Loc, stkbase + (p++)->offs_)
        
        case opPopInt: stk.popInt(); break;
        case opPopLarge: stk.popLarge(); break;
        case opPopPtr: stk.popPtr(); break;

        // --- SOME VECTOR MAGIC ------------------------------------------- //

        case opCopyToTmpVec:
            *pptr(stkbase + (p++)->offs_) = string::_initialize(stk.topPtr());
            break;
        case opElemToVec:
            {
                ShType* type = PType((p++)->ptr_);
                stk.pushPtr(*pptr(stkbase + (p++)->offs_) = elemToVec(type));
            }
            break;
        case opVecCat:
            {
                ShType* t = PType((p++)->ptr_);
                *pptr(stkbase + (p++)->offs_) = vecCat(t);
            }
            break;
        case opVecElemCat:
            {
                PType t = PType((p++)->ptr_);
                *pptr(stkbase + (p++)->offs_) = catVecElem(t);
            }
            break;


        // --- COMPARISONS ------------------------------------------------- //

        case opCmpInt:
            {
                int r = stk.popInt();
                int* t = stk.topIntRef();
                *t = compareInt(*t, r);
            }
            break;
        case opCmpLarge:
            {
                large r = stk.popLarge();
                stk.pushInt(compareLarge(stk.popLarge(), r));
            }
            break;
        case opCmpStrChr:
            {
                int r = stk.popInt();
                ptr l = stk.popPtr();
                stk.pushInt(compareStrChr(l, r));
            }
            break;
        case opCmpChrStr:
            {
                ptr r = stk.popPtr();
                int* t = stk.topIntRef();
                *t = -compareStrChr(r, *t);
            }
            break;
        case opCmpPodVec:
            {
                ptr r = stk.popPtr();
                ptr l = stk.popPtr();
                stk.pushInt(PTR_TO_STRING(l).compare(PTR_TO_STRING(r)));
            }
            break;
        case opCmpTypeRef:
            stk.pushInt(!typeRefsEqual(PType(stk.popPtr()), PType(stk.popPtr())));
            break;

        // case labels
        case opCaseInt: stk.pushInt(stk.topInt() == (p++)->int_); break;
        case opCaseRange:
            {
                int lo = (p++)->int_;
                int l = stk.topInt();
                stk.pushInt(l >= lo && l <= (p++)->int_);
            }
            break;
        case opCaseStr:
            {
                ptr r = (p++)->ptr_;
                ptr l = stk.topPtr();
                stk.pushInt(PTR_TO_STRING(l).equal(PTR_TO_STRING(r)));
            }
            break;
        case opCaseTypeRef:
            stk.pushInt(typeRefsEqual(PType(stk.topPtr()), PType((p++)->ptr_)));
            break;

        //
        case opEQ: { int* t = stk.topIntRef(); *t = *t == 0; } break;
        case opLT: { int* t = stk.topIntRef(); *t = *t < 0; } break;
        case opLE: { int* t = stk.topIntRef(); *t = *t <= 0; } break;
        case opGE: { int* t = stk.topIntRef(); *t = *t >= 0; } break;
        case opGT: { int* t = stk.topIntRef(); *t = *t > 0; } break;
        case opNE: { int* t = stk.topIntRef(); *t = *t != 0; } break;

        // typecasts
        case opLargeToInt: stk.pushInt(stk.popLarge()); break;
        case opIntToLarge: stk.pushLarge(stk.popInt()); break;
        case opIntToStr:
            stk.pushPtr(*pptr(stkbase + (p++)->offs_) = itostr10(stk.popInt()));
            break;
        case opLargeToStr:
            stk.pushPtr(*pptr(stkbase + (p++)->offs_) = itostr10(stk.popLarge()));
            break;

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

        // --- CALLS ------------------------------------------------------- //
        
        case opCallThis:
            {
                ShFunction* func = PFunction((p++)->ptr_);
                // the reason we keep the return value here and not on our stack
                // is because the VM stack may occasionally reallocate and thus
                // invalidate any pointers
                podvalue val;
                stkbase = func->execute(thisseg, &val);
                pushByType(func->returnVar->type, val);
            }
            break;

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


pchar VmCodeSegment::execute(pchar thisseg, ptr retval)
{
#ifdef DEBUG
    if (code.empty())
        internal(107);
#endif

    // make room on the stack for this function so that no reallocations happen
    // at least during its execution
    stk.reserve(reserveLocals + reserveStack);

    // reserve space for locals and temps; to be restored to this point later.
    // we also reset the local storage to zero so that any unused tempvars can 
    // be finalized properly, and in the future this will help to unwind the 
    // stack after an exception
    char* savebase = stk.pushrbytes(reserveLocals);
    memset(savebase, 0, reserveLocals);

    // run, rabbit, run!
    run(getCode(), thisseg, savebase, retval);

#ifdef DEBUG
    stk.verifyend(savebase + reserveLocals);
#endif
    // restore stack base and return the base pointer to the caller
    return stk.poprbytes(reserveLocals);
}



