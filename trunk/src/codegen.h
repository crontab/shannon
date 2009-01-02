#ifndef __CODEGEN_H
#define __CODEGEN_H


#include "common.h"
#include "source.h"
#include "baseobj.h"
#include "vm.h"
#include "langobj.h"


class VmCodeGen: public Base
{
protected:
    struct GenStackInfo
    {
        ShType* type;
        podvalue value;
        bool isValue;
    };

    VmCodeSegment codeseg;
    VmCodeSegment finseg;
    PodStack<GenStackInfo> genStack;
    int genStackSize;
    bool needsRuntimeContext;
    ShVariable* deferredVar;
    
    GenStackInfo& genPush(ShType* v);
    void genPushIntValue(ShType*, int);
    void genPushLargeValue(ShType*, large);
    void genPushPtrValue(ShType*, ptr);
    void genPushVecValue(ShType* type, ptr v)  { genPushPtrValue(type, v); }
    const GenStackInfo& genTop()               { return genStack.top(); }
    ShVariable* genPopDeferred();
    ShType* genPopType()                       { return genPop().type; }

    void genCmpOp(OpCode op, OpCode cmp);
    void genStoreVar(ShVariable* var);
    void genEnd();
    void runFinCode();
    void verifyClean();

public:
    VmCodeGen();
    
    void clear();
    
    ShType* resultTypeHint; // used by the parser for vector/array constructors

    void genLoadIntConst(ShType*, int);
    void genLoadLargeConst(ShType*, large);
    void genLoadVecConst(ShType*, const char*);
    void genLoadTypeRef(ShType*);
    void genLoadConst(ShType*, podvalue);
    void genMkSubrange();
    void genComparison(OpCode);
    void genStaticCast(ShType*);
    void genBinArithm(OpCode op, ShInteger*);
    void genUnArithm(OpCode op, ShInteger*);
    void genBoolXor()
            { genPop(); codeseg.addOp(opBitXor); }
    void genBoolNot()
            { codeseg.addOp(opBoolNot); }
    void genBitNot(ShInteger* type)
            { codeseg.addOp(OpCode(opBitNot + type->isLargeInt())); }
    offs genElemToVec(ShVector*);
    offs genForwardBoolJump(OpCode op);
    offs genForwardJump(OpCode op = opJump);
    void genResolveJump(offs jumpOffset);
    void genLoadVar(ShVariable*);
    void genLoadVarRef(ShVariable*);
    void genStore();
    void genInitVar(ShVariable*);
    void genFinVar(ShVariable*);
    offs genCopyToTempVec();
    void genVecCat(offs tempVar);
    void genVecElemCat(offs tempVar);
    void genIntToStr();
    void genEcho()
            { codeseg.addOp(opEcho); codeseg.addPtr(genPopType()); }
    void genAssert(Parser& parser);
    void genLinenum(Parser& parser);
    void genOther(OpCode op)
            { codeseg.addOp(op); }
    void genReturn();

    offs genOffset() const
            { return codeseg.size(); }
    const GenStackInfo& genPop();
    ShType* genTopType()
            { return genTop().type; }
    offs genReserveLocalVar(ShType*);
    offs genReserveTempVar(ShType*);

    void runConstExpr(ShValue& result);
    ShType* runTypeExpr(bool anyObj, bool* cantEval);
    void genFinalizeTemps();

    VmCodeSegment getCodeSeg();
};


// defined in vm.cpp; made public, because constants need it too
void finalize(ShType* type, ptr data);


#endif

