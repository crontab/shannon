
#include <stdio.h>

#include "str.h"
#include "except.h"
#include "langobj.h"


// --- VIRTUAL MACHINE ----------------------------------------------------- //


class VmStack: public noncopyable
{
protected:
    PodStack<ShQuant> stack;
public:
    VmStack();
    ShQuant& push()           { return stack.push(); }
    ShQuant  pop()            { return stack.pop(); }
    void  pushInt(int v)      { push().int_ = v; }
    void  pushPtr(ptr v)      { push().ptr_ = v; }
    void  pushLarge(large v);
    int   popInt()            { return pop().int_; }
    ptr   popPtr()            { return pop().ptr_; }
    large popLarge();
    int   topInt()            { return stack.top().int_; }
    ptr   topPtr()            { return stack.top().ptr_; }
    large topLarge();
};


enum OpCode
{
    opNone = 0,
    
    opLoad0 = 256,  // []
    opLoadInt,      // [int]
    opLoadLarge,    // [int,int]
    opLoadChar,     // [int]
    opLoadFalse,    // []
    opLoadTrue,     // []
    opLoadNull,     // []
    opLoadStr,      // [string-index]
};


class VmCode: public noncopyable
{
protected:
    PodArray<ShQuant> code;

public:
    ShType* const returnType;

    VmCode(ShType* iReturnType)
            : code(), returnType(iReturnType)  { }
};



// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //


VmStack::VmStack()
        : stack()  { }

void VmStack::pushLarge(large v)
        { push().int_ = int(v); push().int_ = int(v >> 32); }

large VmStack::popLarge()
        { return (large(popInt()) << 32) | popInt(); }

large VmStack::topLarge()
        { return (large(stack.at(-1).int_) << 32) | stack.at(-2).int_; }


// ------------------------------------------------------------------------- //


class _AtExit
{
public:
    ~_AtExit()
    {
        if (Base::objCount != 0)
            fprintf(stderr, "Internal: objCount = %d\n", Base::objCount);
        if (stralloc != 0)
            fprintf(stderr, "Internal: stralloc = %d\n", stralloc);
        if (FifoChunk::chunkCount != 0)
            fprintf(stderr, "Internal: chunkCount = %d\n", FifoChunk::chunkCount);
        if (stackimpl::stackAlloc != 0)
            fprintf(stderr, "Internal: stackAlloc = %d\n", stackimpl::stackAlloc);
    }
} _atexit;




int main()
{
    try
    {
        initLangObjs();
        
#ifdef DEBUG
        ShModule module("tests/test.sn");
#else
        ShModule module("../../src/tests/test.sn");
#endif
        module.compile();
        
        doneLangObjs();
    }
    catch (Exception& e)
    {
        fprintf(stderr, "\n*** Exception: %s\n", e.what().c_str());
    }

    return 0;
}

