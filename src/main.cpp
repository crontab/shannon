

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
        variant value;
        stkinfo(Type* t): type(t) { }
        stkinfo(Type* t, const variant& v): type(t), value(v)  { }
    };

    CodeSeg& codeseg;
    std::vector<stkinfo> genStack;
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
            { return stkTop().value; }
    void stkPop();

public:
    CodeGen(CodeSeg&);
    ~CodeGen();
};


// --- CODE GENERATOR ------------------------------------------------------ //


CodeGen::CodeGen(CodeSeg& _codeseg)
    : codeseg(_codeseg), stkMax(0)
{
    assert(codeseg.empty());
}


CodeGen::~CodeGen()  { codeseg.close(stkMax); }


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

