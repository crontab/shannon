
#include <stdlib.h>
#include <stdio.h>

#include "langobj.h"
#include "vm.h"

// grep '^ *op[A-Za-z0-9]*,' vm.h|sed 's/ *op//;s| *// *||;s/\].*//;s/,\[$//;s/\[/ /;s/^/       OP(/;s/$/),/

#ifdef DEBUG


typedef void (*argfunc)(VmQuant* q);

struct OpInfo
{
    OpCode op;
    const char* name;
    argfunc arg1;
    argfunc arg2;
};

#define OP(o) {op##o, #o}
#define OP1(o,a) {op##o, #o, show_##a}
#define OP2(o,a,b) {op##o, #o, show_##a, show_##b}

static void show_int(VmQuant* q) { printf("%d", q->int_); }
static void show_offs(VmQuant* q) { printf("*%d", q->offs_); }
static void show_ptr(VmQuant* q) { printf("'%s'", pconst(q->ptr_)); }
static void show_ShType(VmQuant* q) { printf("%s", PType(q->ptr_)->getDefinition().c_str()); }
static void show_ShFunction(VmQuant* q) { printf("%s", PFunction(q->ptr_)->getDefinition().c_str()); }

#ifdef PTR64
static void show_large(VmQuant* q) { printf("%lld", q->large_); }
#endif

static OpInfo optable[] = 
{
    OP(End),
    OP(Nop),
    OP(RetByte),
    OP(RetInt),
    OP(RetLarge),
    OP(RetPtr),
    OP(RetVec),
    OP(RetVoid),
    OP(LoadZero),
    OP(LoadLargeZero),
    OP(LoadOne),
    OP(LoadLargeOne),
    OP1(LoadIntConst, int),
#ifdef PTR64
    OP1(LoadLargeConst, large),
#else
    OP2(LoadLargeConst, int, int),
#endif
    OP(LoadFalse),
    OP(LoadTrue),
    OP(LoadNullVec),
    OP1(LoadVecConst, ptr),
    OP1(LoadTypeRef, ShType),
    OP1(LoadThisByte, offs),
    OP1(LoadThisInt, offs),
    OP1(LoadThisLarge, offs),
    OP1(LoadThisPtr, offs),
    OP1(LoadThisVec, offs),
    OP1(LoadThisVoid, offs),
    OP1(StoreThisByte, offs),
    OP1(StoreThisInt, offs),
    OP1(StoreThisLarge, offs),
    OP1(StoreThisPtr, offs),
    OP1(StoreThisVec, offs),
    OP1(StoreThisVoid, offs),
    OP1(FinThisPodVec, offs),
    OP2(FinThis, ShType, offs),
    OP1(LoadLocByte, offs),
    OP1(LoadLocInt, offs),
    OP1(LoadLocLarge, offs),
    OP1(LoadLocPtr, offs),
    OP1(LoadLocVec, offs),
    OP1(LoadLocVoid, offs),
    OP1(StoreLocByte, offs),
    OP1(StoreLocInt, offs),
    OP1(StoreLocLarge, offs),
    OP1(StoreLocPtr, offs),
    OP1(StoreLocVec, offs),
    OP1(StoreLocVoid, offs),
    OP1(FinLocPodVec, offs),
    OP2(FinLoc, ShType, offs),
    OP1(LoadRef, offs),
    OP(PopInt),
    OP(PopLarge),
    OP(PopPtr),
    OP1(PopVec, ShType),
    OP1(CopyToLocVec, offs),
    OP1(CopyToThisVec, offs),
    OP2(ElemToVec, ShType, offs),
    OP2(VecCat, ShType, offs),
    OP2(VecElemCat, ShType, offs),
    OP(CmpInt),
    OP(CmpLarge),
    OP(CmpStrChr),
    OP(CmpChrStr),
    OP(CmpPodVec),
    OP(CmpTypeRef),
    OP1(CaseInt, int),
#ifdef PTR64
    OP1(CaseRange, large),
#else
    OP2(CaseRange, int, int),
#endif
    OP1(CaseStr, ptr),
    OP1(CaseTypeRef, ShType),
    OP(EQ),
    OP(LT),
    OP(LE),
    OP(GE),
    OP(GT),
    OP(NE),
    OP(LargeToInt),
    OP(IntToLarge),
    OP1(IntToStr, offs),
    OP1(LargeToStr, offs),
    OP(MkSubrange),
    OP(Add),
    OP(AddLarge),
    OP(Sub),
    OP(SubLarge),
    OP(Mul),
    OP(MulLarge),
    OP(Div),
    OP(DivLarge),
    OP(Mod),
    OP(ModLarge),
    OP(BitAnd),
    OP(BitAndLarge),
    OP(BitOr),
    OP(BitOrLarge),
    OP(BitXor),
    OP(BitXorLarge),
    OP(BitShl),
    OP(BitShlLarge),
    OP(BitShr),
    OP(BitShrLarge),
    OP(Neg),
    OP(NegLarge),
    OP(BitNot),
    OP(BitNotLarge),
    OP(BoolNot),
    OP1(JumpOr, offs),
    OP1(JumpAnd, offs),
    OP1(JumpTrue, offs),
    OP1(JumpFalse, offs),
    OP1(Jump, offs),
    OP1(CallThis, ShFunction),
    OP1(Echo, ShType),
    OP(EchoLn),
    OP2(Assert, ptr, int),
    OP(Linenum),
    OP(MaxCode)
};


static struct vmdebuginit
{
    vmdebuginit()
    {
        for (int i = 0; optable[i].op != opMaxCode; i++)
        {
            if (optable[i].op != i)
            {
                fprintf(stderr, "VM debug table inconsistency at %d (%s)\n",
                    i, optable[i].name);
                exit(1);
            }
        }
    }
} _vmdebuginit;


void VmCodeSegment::print()
{
    printf("  $locals(%d)  $pop(%d)\n", reserveLocals, popOnReturn);
    for (int i = 0; i < size(); i++)
    {
        OpCode op = at(i)->op_;
        if (op >= opMaxCode)
            internal(200);

        if (op == opLinenum)
        {
            pconst fn = pconst(at(++i)->ptr_);
            printf("; --- %s(%d)\n", fn, at(++i)->int_);
            continue;
        }

        OpInfo* info = optable + op;
        printf("  %-15s", info->name);
        if (info->arg1 != NULL)
        {
            (*info->arg1)(at(++i));
            if (info->arg2 != NULL)
            {
                printf(", ");
                (*info->arg2)(at(++i));
            }
        }
        printf("\n");
    }
    printf("; ------------------------------------------\n");
}


#endif

