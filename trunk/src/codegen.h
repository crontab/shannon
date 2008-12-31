#ifndef __CODEGEN_H
#define __CODEGEN_H


#include "baseobj.h"
#include "vm.h"
#include "langobj.h"


class VmCodeGen: public Base
{
protected:
    struct GenStackInfo
    {
        ShType* type;
        int codeOffs;
        GenStackInfo(ShType* iType, int iCodeOffs)
            : type(iType), codeOffs(iCodeOffs)  { }
    };

    VmCodeSegment codeseg;
    VmCodeSegment finseg;
    PodStack<GenStackInfo> genStack;
    int genStackSize;
    bool needsRuntimeContext;
    ShVariable* deferredVar;
    
    void genPush(ShType* v);
    const GenStackInfo& genTop()        { return genStack.top(); }
    ShVariable* genPopDeferred();
    ShType* genPopType()                { return genPop().type; }

    void genCmpOp(OpCode op, OpCode cmp);
    void genEnd();
    void runFinCode();
    void verifyClean();

public:
    VmCodeGen();
    
    void clear();
    
    ShType* resultTypeHint; // used by the parser for vector/array constructors

    void genLoadIntConst(ShOrdinal*, int);
    void genLoadLargeConst(ShOrdinal*, large);
    void genLoadNull();
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
    ShType* runTypeExpr(ShValue& value, bool anyObj);
    void genFinalizeTemps();

    VmCodeSegment getCodeSeg();
};


#endif

