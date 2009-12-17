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

    opInitRet,          // -var
    opDeref,            // -ref +var
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
    friend class CodeGen;
    friend class State;

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

    // Execution
    void run(varpool& stack, rtobject* self, variant* result); // <-- this is the VM itself
    void failAssertion();

public:
    CodeSeg(State*);
    ~CodeSeg();

    State* getType() const          { return cast<State*>(_type); }
    memint size() const             { return code.size(); }
    bool empty() const;
};


template<class T>
    inline T ADV(const uchar*& ip)
        { T t = *(T*)ip; ip += sizeof(T); return t; }


DEF_EXCEPTION(eexit, "exit called");


#endif // __VM_H
