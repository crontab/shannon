

#include "common.h"
#include "runtime.h"
#include "parser.h"
#include "typesys.h"
#include "vm.h"


// TODO: less loaders: for args and locals one instruction can be used
// TODO: assert: use constant repository for file names
// TODO: multiple return values probably not needed
// TODO: store the current file name in a named const, say __FILE__


class CodeGen: noncopyable
{
protected:
    State* codeOwner;
    CodeSeg* codeseg;

    objvec<Type> simStack;      // exec simulation stack
    memint locals;              // number of local vars currently on the stack
    memint maxStack;            // total stack slots needed without function calls
    memint lastOpOffs;

    template <class T>
        void add(const T& t)    { codeseg->append<T>(t); }
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
    CodeGen(CodeSeg*);  // for const expressions
    ~CodeGen();
    
    memint getLocals()      { return locals; }
    State* getState()       { return codeOwner; }
    void deinitLocalVar(Variable*);
    void discard();

     // NOTE: compound consts shoudl be held by a smart pointer somewhere else
    void loadConst(Type*, const variant&);
    void loadEmptyCont(Container* type);
    void initRet()
        { addOp(opInitRet); stkPop(); }
    void end();
    void runConstExpr(Type* expectType, variant& result);
};


class BlockScope: public Scope
{
protected:
    objvec<Variable> localVars;      // owned
    memint startId;
    CodeGen* gen;
public:
    BlockScope(Scope* outer, CodeGen*);
    ~BlockScope();
    Variable* addLocalVar(const str&, Type*);
    void deinitLocals();
};


// ------------------------------------------------------------------------- //


CodeGen::CodeGen(CodeSeg* c)
    : codeOwner(c ? c->getType() : NULL), codeseg(c), locals(0), maxStack(0), lastOpOffs(-1)  { }

CodeGen::~CodeGen()
    { }

void CodeGen::error(const char* msg)
    { throw ecmessage(msg); }


memint CodeGen::addOp(OpCode op)
{
    lastOpOffs = codeseg->size();
    add<uchar>(op);
    return lastOpOffs;
}


memint CodeGen::addOp(OpCode op, object* o)
{
    memint r = addOp(op);
    add(o);
    return r;
}


void CodeGen::discardCode(memint from)
{
    codeseg->resize(from);
    if (lastOpOffs >= from)
        lastOpOffs = memint(-1);
}


void CodeGen::revertLastLoad()
{
    if (isUndoableLoadOp(lastOp()))
        discardCode(lastOpOffs);
    else
        // discard();
        notimpl();
}


OpCode CodeGen::lastOp()
{
    if (lastOpOffs == memint(-1))
        return opInv;
    return OpCode(codeseg->at<uchar>(lastOpOffs));
}


void CodeGen::stkPush(Type* type)
{
    simStack.push_back(type);
    if (simStack.size() > maxStack)
        maxStack = simStack.size();
}


Type* CodeGen::stkPop()
{
    Type* result = simStack.back();
    simStack.pop_back();
    return result;
}


void CodeGen::stkReplaceTop(Type* t)
    { simStack.replace_back(t); }


void CodeGen::canAssign(Type* from, Type* to, const char* errmsg)
{
    if (!from->canAssignTo(to))
        error(errmsg == NULL ? "Type mismatch" : errmsg);
}


bool CodeGen::tryImplicitCast(Type* to)
{
    Type* from = stkTop();

    if (from == to || from->identicalTo(to))
        return true;

    if (from->canAssignTo(to) || to->isVariant())
    {
        stkReplaceTop(to);
        return true;
    }

    if (to->isVec() && from->canAssignTo(PContainer(to)->elem))
    {
        addOp(PContainer(to)->hasSmallElem() ? opChrToStr : opVarToVec);
        stkReplaceTop(to);
        return true;
    }

    if (from->isNullCont() && to->isAnyCont())
    {
        stkPop();
        revertLastLoad();
        loadEmptyCont(PContainer(to));
        return true;
    }

    if (from->isReference())
    {
        // TODO: replace the original loader with its deref version (in a separate function)
        Type* actual = PReference(from)->to;
        addOp(opDeref);
        stkReplaceTop(actual);
        return tryImplicitCast(actual);
    }

    return false;
}


void CodeGen::implicitCast(Type* to, const char* errmsg)
{
    if (!tryImplicitCast(to))
        error(errmsg == NULL ? "Type mismatch" : errmsg);
}


void CodeGen::deinitLocalVar(Variable* var)
{
    // This is called from BlockScope.
    // TODO: don't generate POPs if at the end of a function
    assert(var->isLocalVar());
    assert(locals == simStack.size());
    assert(var->id == locals - 1);
    locals--;
    discard();
}


void CodeGen::discard()
{
    stkPop();
    addOp(opPop);
}


void CodeGen::loadConst(Type* type, const variant& value)
{
    // variant: NONE, ORD, REAL, STR, VEC, SET, ORDSET, DICT, RTOBJ
    // Type: TYPEREF, NONE, VARIANT, REF, BOOL, CHAR, INT, ENUM,
    //       NULLCONT, VEC, SET, DICT, FIFO, FUNC, PROC, OBJECT, MODULE
    switch (type->typeId)
    {
    case Type::TYPEREF:     addOp(opLoadTypeRef, value._rtobj()); break;
    case Type::NONE:        addOp(opLoadNull); break;
    case Type::VARIANT:     error("Variant constants are not supported"); break;
    case Type::REF:         error("Reference constants are not supported"); break;
    case Type::BOOL:
    case Type::CHAR:
    case Type::INT:
    case Type::ENUM:
        {
            integer i = value._ord();
            if (i == 0)
                addOp(opLoad0);
            else if (i == 1)
                addOp(opLoad1);
            else if (uinteger(i) <= 255)
                { addOp(opLoadOrd8); add<uchar>(i); }
            else
                { addOp(opLoadOrd); add<integer>(i); }
        }
        break;
    case Type::NULLCONT: addOp(opLoadNull); break;

    // All dynamic objects in the system are derived from "object" so copying
    // involves only incrementing the ref counter.
    case Type::VEC:
    case Type::SET:
    case Type::DICT:
    case Type::FIFO:
    case Type::FUNC:
    case Type::PROC:
    case Type::OBJECT:
    case Type::MODULE:
        addOp(opLoadConstObj);
        add<uchar>(value.getType());
        add<object*>(value.as_anyobj());
        break;
    }
    stkPush(type);
}


void CodeGen::loadEmptyCont(Container* contType)
{
    variant v;
    switch (contType->typeId)
    {
    case Type::NULLCONT:
        error("Container type undefined");
    case Type::VEC:
        if (contType->hasSmallElem()) v = str(); else v = varvec(); break;
    case Type::SET: break;
        if (contType->hasSmallIndex()) v = ordset(); else v = varset(); break;
    case Type::DICT: break;
        if (contType->hasSmallIndex()) v = varvec(); else v = vardict(); break;
    default:
        notimpl();
    }
    loadConst(contType, v);
}


void CodeGen::end()
{
    codeseg->close(maxStack);
    assert(simStack.size() - locals == 0);
}


void CodeGen::runConstExpr(Type* expectType, variant& result)
{
    if (expectType != NULL)
        implicitCast(expectType, "Constant expression type mismatch");
    initRet();
    end();
    rtstack stack;
    result.clear();
    codeseg->run(stack, NULL, &result);
    assert(stack.size() == 0);
}


// --- BlockScope ---------------------------------------------------------- //

// In principle this belongs to typesys.cpp but defined here because it uses
// the codegen object for managing local vars.


BlockScope::BlockScope(Scope* _outer, CodeGen* _gen)
    : Scope(_outer), startId(_gen->getLocals()), gen(_gen)  { }


BlockScope::~BlockScope()  { }


void BlockScope::deinitLocals()
{
    for (memint i = localVars.size(); i--; )
        gen->deinitLocalVar(localVars[i]);
}


Variable* BlockScope::addLocalVar(const str& name, Type* type)
{
    memint id = startId + localVars.size();
    if (id >= 255)
        throw ecmessage("Maximum number of local variables within this scope is reached");
    objptr<Variable> v = new Variable(name, Symbol::LOCALVAR, type, id, gen->getState());
    addUnique(v);   // may throw
    localVars.push_back(v);
    return v;
}



// --- HIS MAJESTY THE COMPILER -------------------------------------------- //


struct CompilerOptions
{
    bool enableDump;
    bool enableAssert;
    bool linenumInfo;
    bool vmListing;

    CompilerOptions()
      : enableDump(true), enableAssert(true), linenumInfo(true),
        vmListing(true)  { }
};


class Compiler: protected Parser
{
    CompilerOptions options;

    objptr<Module> module;
    objptr<CodeSeg> codeseg;

    CodeGen* codegen;
    Scope* scope;           // for looking up symbols
    BlockScope* blockScope; // for local vars in nested blocks, can be NULL
    State* state;           // for this-vars and type objects

    void statementList();

public:
    Compiler(const str&, fifo*);
    ~Compiler();

    void compile();
    Module* getModule() const;
    CodeSeg* getCodeSeg() const;
};


str moduleNameFromFileName(const str& n)
    { return remove_filename_path(remove_filename_ext(n)); }


Compiler::Compiler(const str& modName, fifo* f)
    : Parser(f), module(new Module(modName)), codeseg(new CodeSeg(module))
       { }

Compiler::~Compiler()
    { }


void Compiler::statementList()
{
}


void Compiler::compile()
{
    CodeGen mainCodeGen(codeseg);
    codegen = &mainCodeGen;
    scope = module;
    blockScope = NULL;
    state = module;
    try
    {
        next();
        statementList();
        expect(tokEof, "End of file");
    }
    catch (EDuplicate& e)
        { error("'" + e.ident + "' is already defined within this scope"); }
    catch (EUnknownIdent& e)
        { error("'" + e.ident + "' is unknown in this context"); }
    catch (exception& e)
        { error(e.what()); }

    mainCodeGen.end();

/*
    if (options.vmListing)
    {
        outtext f(NULL, remove_filename_ext(getFileName()) + ".lst");
        mainModule.listing(f);
    }
*/
}


// --- tests --------------------------------------------------------------- //


#include "typesys.h"


void ut_fail(unsigned line, const char* e)
{
    fprintf(stderr, "%s:%u: test failed `%s'\n", __FILE__, line, e);
    exit(200);
}

#define fail(e)     ut_fail(__LINE__, e)
#define check(e)    { if (!(e)) fail(#e); }

#define check_throw(a) \
    { bool chk_throw = false; try { a; } catch(exception&) { chk_throw = true; } check(chk_throw); }


static void _codegen_load(Type* type, const variant& v)
{
    CodeSeg code(NULL);
    CodeGen gen(&code);
    gen.loadConst(type, v);
    variant result;
    gen.runConstExpr(type, result);
    check(result == v);
}


void test_codegen()
{
    _codegen_load(queenBee->defInt, 21);
    _codegen_load(queenBee->defStr, "ABC");
    _codegen_load(defTypeRef, queenBee->defInt);
    {
        varvec v;
        v.push_back(10);
        _codegen_load(queenBee->registerContainer(queenBee->defInt, defNone), v);
    }
    {
        CodeSeg code(NULL);
        CodeGen gen(&code);
        gen.loadConst(queenBee->defNullCont, variant::null);
        variant result;
        gen.runConstExpr(queenBee->defStr, result);
        check(result == "");
    }
}


int main()
{
    printf("%lu %lu\n", sizeof(object), sizeof(container));

    initTypeSys();

    test_codegen();

    doneTypeSys();

    if (object::allocated != 0)
    {
        fprintf(stderr, "object::allocated: %d\n", object::allocated);
        _fatal(0xff01);
    }

    return 0;
}
