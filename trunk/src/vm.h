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

    // Loaders
    opLoadSelfVar,      // [self-idx8] +var
    opLoadStkVar,       // [stk-idx8] +var

    // Storers
    opStoreSelfVar,     // [self-idx8] -var
    opStoreStkVar,      // [stk-idx8] -var

    opDeref,            // -ptr +var
    opPop,              // -var

    opChrToStr,         // -ord +str
    opVarToVec,         // -var +vec

    opInv,
    opMaxCode = opInv,
};


inline bool isUndoableLoadOp(OpCode op)
    { return (op >= opLoadTypeRef && op <= opLoadConstObj)
        || (op >= opLoadSelfVar && op <= opLoadStkVar); }

inline bool isDesignatorLoadOp(OpCode op)
    { return op >= opLoadSelfVar && op <= opLoadStkVar; }

inline OpCode designatorLoadToStore(OpCode op)
    { return OpCode(op + opStoreSelfVar - opLoadSelfVar); }


#define DEFAULT_STACK_SIZE 8192


class CodeSeg: public rtobject
{
    friend class CodeGen;
    typedef rtobject parent;

    str code;

protected:
    memint stackSize;

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
    OpCode operator[] (memint i) const { return OpCode(code[i]); }
    void close(memint s)            { assert(stackSize == 0); stackSize = s; }

public:
    CodeSeg(State*) throw();
    ~CodeSeg() throw();

    State* getType() const          { return cast<State*>(parent::getType()); }
    memint size() const             { return code.size(); }
    bool empty() const;

    // Return a NULL-terminated string ready to be run: NULL char is an opcode
    // to exit the function
    const char* getCode() const     { return code.c_str(); }
};


// The Virtual Machine. This routine is used for both evaluating const
// expressions at compile time and, obviously, running runtime code. It is
// thread-safe and can be launched concurrently in one process so long as
// the arguments passed belong to one thread (except the code seggment which
// is read-only anyway).

void runRabbitRun(rtstack& stack, register const char* ip, variant* self);


DEF_EXCEPTION(eexit, "exit called");


// --- Code Generator ------------------------------------------------------ //


class CodeGen: noncopyable
{
protected:
    State* codeOwner;
    CodeSeg& codeseg;

    struct SimStackItem
    {
        Type* type;
        memint offs;
        SimStackItem(Type* t, memint o)
            : type(t), offs(o)  { }
    };

    podvec<SimStackItem> simStack;  // exec simulation stack
    memint locals;                  // number of local vars allocated
    memint maxStack;                // total stack slots needed without function calls

    template <class T>
        void add(const T& t)                        { codeseg.append<T>(t); }
    void addOp(OpCode op)                           { add<uchar>(op); }
    void addOp(Type*, OpCode op);
    template <class T>
        void addOp(OpCode op, const T& a)           { addOp(op); add<T>(a); }
    template <class T>
        void addOp(Type* t, OpCode op, const T& a)  { addOp(t, op); add<T>(a); }
    Type* stkPop();
    void stkReplaceTop(Type* t);
    Type* stkTop()
        { return simStack.back().type; }
    memint stkTopOffs()
        { return simStack.back().offs; }
    Type* stkTop(memint i)
        { return simStack.back(i).type; }
    static void error(const char*);
    void undoLastLoad();
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

     // NOTE: compound consts should be held by a smart pointer somewhere else
    void loadConst(Type*, const variant&);

    void loadEmptyCont(Container* type);
    void loadVariable(Variable*);
    Type* undoDesignatorLoad(str& loader);
    void storeDesignator(str loaderCode, Type* type);
    void storeRet(Type*);
    void end();
    void runConstExpr(Type* expectType, variant& result); // defined in vm.cpp
};


#endif // __VM_H
