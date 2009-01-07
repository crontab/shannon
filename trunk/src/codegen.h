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
        offs opOffset;
        bool isValue;
        bool isFuncCall;
    };

    VmCodeSegment codeseg;
    VmCodeSegment finseg;
    PodStack<GenStackInfo> genStack;
    int genStackSize;
    
    GenStackInfo& genPush(ShType* v);
    GenStackInfo& genPushValue(ShType* v);
    void genPushIntValue(ShType*, int);
    void genPushLargeValue(ShType*, large);
    void genPushPtrValue(ShType*, ptr);
    void genPushVecValue(ShType* type, ptr v)  { genPushPtrValue(type, v); }
    void genPushVarRef(ShVariable*);
    void genPushFuncCall(ShFunction*);
    const GenStackInfo& genTop() const         { return genStack.top(); }

    void genCmpOp(OpCode op, OpCode cmp);
    void genEnd();
    void verifyContext(ShVariable*);
    void runFinCode();
    void verifyClean();

public:
    VmCodeGen(ShScope* iHostScope);
    
    void clear();
    
    ShScope* hostScope;

    ShType* resultTypeHint; // used by the parser for vector/array constructors

    void genLoadIntConst(ShType*, int);
    void genLoadLargeConst(ShType*, large);
    void genLoadVecConst(ShType*, const char*);
    ShTypeRef* genLoadTypeRef(ShType*);
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
    void genJump(offs target);
    void genLoadVarRef(ShVariable*);
    ShType* genDerefVar();
    ShVariable* genUndoVar();
    ShType* genUndoTypeRef();
    void genStoreVar(ShVariable*);
    offs genCase(const ShValue&, OpCode jumpOp);
    void genPopValue(bool finalize);
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
    void genCall(ShFunction*);

    offs genOffset() const
            { return codeseg.size(); }
    bool genTopIsValue() const
            { return genTop().isValue; }
    bool genTopIsFuncCall() const
            { return genTop().isFuncCall; }
    const GenStackInfo& genPop();
    ShType* genTopType()
            { return genTop().type; }
    ShType* genPopType()
            { return genPop().type; }
    ptr genTopPtrValue();
    offs genReserveLocalVar(ShType*);
    offs genReserveTempVar(ShType*);

    void runConstExpr(ShValue& result);
    ShType* runTypeExpr(bool anyObj);
    void genFinalizeTemps();

    VmCodeSegment getCodeSeg();
};


struct ENoContext: public Exception
{
    ENoContext();
};


// defined in vm.cpp; made public, because constants need it too
void finalize(ShType* type, ptr data);


#endif

