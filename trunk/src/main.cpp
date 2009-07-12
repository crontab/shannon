

#include "runtime.h"
#include "source.h"
#include "symbols.h"
#include "typesys.h"
#include "vm.h"


class CodeGen: noncopyable
{
protected:

    struct stkinfo
    {
        Type* type;
        stkinfo(Type* t): type(t) { }
    };

    CodeSeg& codeseg;

    std::vector<stkinfo> genStack;
    varlist valStack;
    mem stkMax;

    void stkPush(Type* t, const variant& v);
    void stkPush(Type* t)
            { stkPush(t, null); }
    void stkPush(Constant* c)
            { stkPush(c->type, c->value); }
    const stkinfo& stkTop() const;
    Type* topType() const
            { return stkTop().type; }
    const variant& topValue() const
            { return valStack.back(); }
    void stkPop();

public:
    CodeGen(CodeSeg&);
    ~CodeGen();
};


// --- THE VIRTUAL MACHINE ------------------------------------------------- //


void invOpcode()        { throw EInvOpcode(); }


template<class T>
    inline void PUSH(variant*& stk, const T& v)  { ::new(++stk) variant(v);  }
inline void POP(variant*& stk)              { (*stk--).~variant(); }


void CodeSeg::doRun(variant*& stk, const char* ip)
{
    while (1)
    {
        switch(uchar(*ip++))
        {
        case opInv: invOpcode(); break;
        case opEnd: return;
        case opNop: break;
        }
    }
}


void CodeSeg::run(varstack& stack)
{
    if (code.empty())
        return;
    variant* stkbase = stack.reserve(stksize);
    variant* stk = stkbase - 1; // point to the top element
    try
    {
        doRun(stk, code.data());
    }
    catch(exception& e)
    {
        while (stk <= stkbase)
            POP(stk);
        stack.free(stksize);
        throw e;
    }
    assert(stk == stkbase - 1);
    stack.free(stksize);
}


// --- CODE GENERATOR ------------------------------------------------------ //


CodeGen::CodeGen(CodeSeg& _codeseg)
    : codeseg(_codeseg), stkMax(0)
{
    assert(codeseg.empty());
}


CodeGen::~CodeGen()  { codeseg.close(stkMax); }


void CodeGen::stkPush(Type* type, const variant& value)
{
    genStack.push_back(stkinfo(type));
    valStack.push_back(value);
    if (genStack.size() > stkMax)
        stkMax = genStack.size();
}


const CodeGen::stkinfo& CodeGen::stkTop() const
    { return genStack.back(); }


void CodeGen::stkPop()
{
    assert(genStack.size() == valStack.size());
    genStack.pop_back();
    valStack.pop_back();
}


// --- varlist ------------------------------------------------------------- //


// --- tests --------------------------------------------------------------- //


#define check(x) assert(x)


int main()
{
    {
        varlist v;
        v.push_back(1);
        v.push_back(new object());
        fout << v.back() << endl;
        v.pop_back();
        fout << v.back() << endl;
        v.push_back(new object());
        v.push_back(new range(0, 1));
        v.erase(1);
    }


    initTypeSys();
    try
    {
        Parser parser("x", new in_text("x"));
//        fout << "opcodes: " << opMaxCode << endl;
        
//        Context context;
        CodeSeg codeseg(NULL, NULL);
        CodeGen codegen(codeseg);
        
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

