#ifndef __VM_H
#define __VM_H

#include "common.h"
#include "runtime.h"
#include "typesys.h"


enum OpCode
{
    // NOTE: the order of many of these instructions in their groups is significant

    opEnd = 0,          // end execution and return
    opNop,
    opExit,             // throws eexit()

    // Const loaders
    opLoadTypeRef,      // [Type*] +obj
    opLoadNull,         // +null
    opLoad0,            // +ord
    opLoad1,            // +ord
    opLoadOrd8,         // [int8] +ord
    opLoadOrd,          // [int] +ord
    opLoadNullCont,     // +null
    opLoadConstObj,     // [variant::Type:8, object*]

    opStore,            // -var - ptr

    opInitRet,          // -var
    opDeref,            // -ptr +var
    opPop,              // -var

    opChrToStr,         // -ord +str
    opVarToVec,         // -var +vec

    opInv,
    opMaxCode = opInv,
};


inline bool isUndoableLoadOp(OpCode op)
    { return (op >= opLoadTypeRef && op <= opLoadConstObj); }


class CodeSeg: public rtobject
{
    typedef rtobject parent;
    friend class CodeGen;

    str code;           // Hide even from "friends"

protected:
    memint stackSize;   // Max stack size used without calls; to prealloc at runtime

    // Code gen helpers
    void append(uchar u)            { code.push_back(u); }
    template <class T>
        void append(const T& t)     { code.push_back<T>(t); }
    void append(const str& s)       { code.append(s); }
    void resize(memint s)           { code.resize(s); }
    str  cutTail(memint start)
        { str t = code.substr(start); resize(start); return t; }
    template<class T>
        const T& at(memint i) const { return *code.data<T>(i); }
    template<class T>
        T& atw(memint i)            { return *code.atw<T>(i); }
    void close(memint s)            { assert(stackSize == 0); stackSize = s; }

    void run(rtstack& stack, variant self[], variant result[]); // <-- this is the VM itself

public:
    CodeSeg(State*) throw();
    ~CodeSeg() throw();

    State* getType() const          { return cast<State*>(parent::getType()); }
    memint size() const             { return code.size(); }
    bool empty() const;
};


template<class T>
    inline T ADV(const uchar*& ip)
        { T t = *(T*)ip; ip += sizeof(T); return t; }


DEF_EXCEPTION(eexit, "exit called");


// --- Code Generator ------------------------------------------------------ //


class CodeGen: noncopyable
{
protected:
    State* codeOwner;
    CodeSeg& codeseg;

    objvec<Type> simStack;      // exec simulation stack
    memint locals;              // number of local vars currently on the stack
    memint maxStack;            // total stack slots needed without function calls
    memint lastOpOffs;

    template <class T>
        void add(const T& t)    { codeseg.append<T>(t); }
    memint addOp(OpCode op);
    memint addOp(OpCode op, object* o);
    void stkPush(Type*);
    Type* stkPop();
    void stkReplaceTop(Type* t);
    Type* stkTop()
        { return simStack.back(); }
    static void error(const char*);
    void discardCode(memint from);
    void revertLastLoad();
    OpCode lastOp();
    void canAssign(Type* from, Type* to, const char* errmsg = NULL);
    bool tryImplicitCast(Type*);
    void implicitCast(Type*, const char* errmsg = NULL);

public:
    CodeGen(CodeSeg&);
    ~CodeGen();
    
    memint getLocals()      { return locals; }
    State* getState()       { return codeOwner; }
    void deinitLocalVar(Variable*);
    void discard();

     // NOTE: compound consts shoudl be held by a smart pointer somewhere else
    void loadConst(Type*, const variant&);

    void loadEmptyCont(Container* type);
    void store();
    void initRet()
        { addOp(opInitRet); stkPop(); }
    void end();
    void runConstExpr(Type* expectType, variant& result);
};


#endif // __VM_H
