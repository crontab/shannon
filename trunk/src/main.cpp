

#include "runtime.h"
#include "source.h"
#include "symbols.h"
#include "typesys.h"


// TODO: reimplement with a simple dynamic buffer
class varstack: protected tuple_impl, public noncopyable
{
public:
    varstack() { }
    ~varstack() { }
    void push(const variant& v)     { push_back(v); }
    void pushn(mem n)               { resize(size() + n); }
    variant& top()                  { return back(); }
    variant& top(mem n)             { return *(end() - n); }
    void pop()                      { pop_back(); }
    void popn(mem n)                { resize(size() - n); }
};



// TODO: "unsafe" versions for operations that expect particular types
// TODO: implement safe typecasts from any type to any type (for opToXXX)

enum OpCode
{
    opInv,  // to detect corrupt code segments
    opEnd,
    opNop,
    
    // Load constants
    opLoadNull,
    opLoadFalse,
    opLoadTrue,
    opLoadChar,         // [8]
    opLoad0,
    opLoad1,
    opLoadInt,          // [int]
    opLoadNullStr,
    opLoadNullRange,
    opLoadNullTuple,
    opLoadNullDict,
    opLoadNullOrdset,
    opLoadNullSet,
    opLoadNullFifo,
    opLoadConst,        // [const-index: 8]
    opLoadTypeRef,      // [Type*]

    // Loaders: each of these can be replaced by a corresponding storer if
    // the object turns out to be a L-value.
    opLoadLocal,        // [stack-index: signed-8 (retval(-N), args(-N), temp(N))]
    opLoadThis,         // [var-index: 8]
    opLoadOuter,        // [level: 8, var-index: 8]
    opLoadStatic,       // [module: 8, var-index: 8]
    opLoadStrElem,      // pop index, pop str, push char
    opLoadTupleElem,    // pop index, pop tuple, push val
    opLoadDictElem,     // pop key, pop dict, push val
    opLoadMember,       // [var-index: 8] pop obj, push val

    // Storers
    opStoreLocal,       // [stack-index]
    opStoreThis,        // [var-index: 8]
    opStoreOuter,       // [level: 8, var-index: 8]
    opStoreStatic,      // [module: 8, var-index: 8]
    opStoreStrElem,     // pop char, pop index, pop str
    opStoreTupleElem,   // pop val, pop index, pop tuple
    opStoreDictElem,    // pop val, pop key, pop dict
    opStoreMember,      // [var-index: 8] pop val, pop obj

    // Vector/string concatenation
    opCharToStr,        // pop char, push str
    opCatCharStr,       // pop char
    opVarToTuple,       // pop val, push tuple
    opCatElemTuple,     // pop elem
    
    // Range operations
    opMkBoolRange,      // stupid but accepted by the compiler
    opMkCharRange,      // pop right-char, pop left-char, push range
    opMkIntRange,       // pop right-int, pop left-int, push range
    opBoolInRange,      // pop range, pop bool, push {0,1}
    opCharInRange,      // pop range, pop int, push {0,1}
    opIntInRange,       // pop range, pop int, push {0,1}
    
    // Comparators
    opCmp,              // pop var, pop var, push {-1,0,1}
    opCmpNull,          // pop var, push {0,1}
    opCmpFalse,         // pop bool, push {0,1}
    opCmpTrue,          // pop bool, push {-1,0}
    opCmpCharStr,       // pop str, pop char, push {-1,0,1}
    opCmpStrChar,       // pop char, pop str, push {-1,0,1}
    opCmpInt0,          // pop int, push {-1,0,1}
    opCmpInt1,          // pop int, push {-1,0,1}
    opCmpNullStr,       // pop str, push {0,1}
    opCmpNullTuple,     // pop str, push {0,1}
    opCmpNullDict,      // pop str, push {0,1}
    opCmpNullOrdset,    // pop ordset, push {0,1}
    opCmpNullSet,       // pop set, push {0,1}
    opCmpNullFifo,      // pop str, push {0,1}

    // Compare the stack top with 0 and replace it with a bool value.
    // The order of these opcodes is in sync with tokEqual..tokNotEq
    opEqual, opLessThan, opLessEq, opGreaterEq, opGreaterThan, opNotEq,
    
    // Case labels: cmp the top element with the arg and leave 0 or 1 for
    // further boolean jump
    opCase,             // pop var, push {0,1}
    opCaseRange,        // pop int, push {0,1}
    
    // Safe variant typecasts
    opToBool,
    opToChar,
    opToInt,
    opToStr,
    opToType,           // [Type*]
    
    // Arithmetic
    opAdd, opSub, opMul, opDiv, opMod, opBitAnd, opBitOr, opBitXor, opBitShl, opBitShr,
    opNeg, opBitNot, opNot,
    
    // Jumps; [dst] is a relative offset
    //   short bool evaluation: pop if jump, leave it otherwise
    opJumpOr, opJumpAnd,
    opJumpTrue, opJumpFalse, opJump,

    // Function call
    opCall,             // [Type*]

    // Helpers
    opEcho, opEchoLn,
    opAssert,           // [line-num: 16]
    opLinenum,          // [line-num: 16]
    
    opMaxCode
};


class Context: noncopyable
{
    friend class CodeSeg;
protected:
    List<Module> modules;
    List<tuple> datasegs;
    varstack stack;
    Module* topModule;
    void resetDatasegs();
public:
    Context();
    Module* addModule(const str& name);
    void run();
};


class CodeSeg: noncopyable
{
protected:
    str code;
    varstack consts;
public:
    int addOp(OpCode c);
    void add8(uchar i);
    void add16(unsigned short i);
    void addInt(integer i);
    void addPtr(void* p);
    void close();
};


class CodeGen: noncopyable
{
protected:

    struct stkinfo
    {
        Type* type;
        variant value;
        stkinfo(Type* t): type(t) { }
        stkinfo(Type* t, const variant& v): type(t), value(v)  { }
    };

    CodeSeg codeseg;
    std::vector<stkinfo> genStack;
    mem stkMax;
    Context* context;

    void stkPush(Type* t, const variant& v);
    void stkPush(Type* t)
            { stkPush(t, null); }
    void stkPush(Constant* c)
            { stkPush(c->type, c->value); }
    const stkinfo& stkTop() const;
    Type* topType() const
            { return stkTop().type; }
    const variant& topValue() const
            { return stkTop().value; }
    void stkPop();

public:
    CodeGen(Context*);
};


// --- EXECUTION CONTEXT --------------------------------------------------- //


Context::Context()
    : topModule(NULL)  { }


Module* Context::addModule(const str& name)
{
    if (modules.size() == 255)
        throw emessage("Maximum number of modules reached");
    topModule = new Module(name, modules.size());
    modules.add(topModule);
    datasegs.add(new tuple());
    return topModule;
}


void Context::resetDatasegs()
{
    assert(modules.size() == datasegs.size());
    for (mem i = 0; i < modules.size(); i++)
    {
        tuple* d = datasegs[i];
        d->clear();
        d->resize(modules[i]->dataSize());
    }
}


// --- CODE SEGMENT -------------------------------------------------------- //


int CodeSeg::addOp(OpCode c)
    { code.push_back(c); return code.size() - 1; }

void CodeSeg::add8(uchar i)
    { code.push_back(i); }

void CodeSeg::add16(unsigned short i)
    { code.append((char*)&i, 2); }

void CodeSeg::addInt(integer i)
    { code.append((char*)&i, sizeof(i)); }

void CodeSeg::addPtr(void* p)
    { code.append((char*)&p, sizeof(p)); }

void CodeSeg::close()
    { addOp(opEnd); }


// --- CODE GENERATOR ------------------------------------------------------ //


CodeGen::CodeGen(Context* _context)
    : stkMax(0), context(_context)  { }


void CodeGen::stkPush(Type* type, const variant& value)
{
    genStack.push_back(stkinfo(type, value));
    if (genStack.size() > stkMax)
        stkMax = genStack.size();
}


const CodeGen::stkinfo& CodeGen::stkTop() const
    { return genStack.back(); }


void CodeGen::stkPop()
    { genStack.pop_back(); }


// --- tests --------------------------------------------------------------- //

#define check(x) assert(x)


int main()
{
    initTypeSys();
    try
    {
        Parser parser("x", new in_text("x"));
        fout << "opcodes: " << opMaxCode << endl;
        
//        Context context;
        CodeGen codegen(NULL);
        
    }
    catch (std::exception& e)
    {
        ferr << "Exception: " << e.what() << endl;
    }
    doneTypeSys();
#ifdef DEBUG
    assert(object::alloc == 0);
#endif
}

