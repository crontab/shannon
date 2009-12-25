
#include "vm.h"
#include "compiler.h"


CodeSeg::CodeSeg(State* stateType) throw()
    : rtobject(stateType), stackSize(-1)  { }


CodeSeg::~CodeSeg() throw()
    { }


bool CodeSeg::empty() const
    { return code.empty(); }


void CodeSeg::close(memint s)
{
    assert(stackSize == -1);
    stackSize = s;
    append(opEnd);
}


// --- VIRTUAL MACHINE ----------------------------------------------------- //


static void invOpcode()             { fatal(0x5002, "Invalid opcode"); }
static void doExit()                { throw eexit(); }

template<class T>
    inline T& ADV(const char*& ip)
        { T& t = *(T*)ip; ip += sizeof(T); return t; }

template<class T>
    inline void PUSH(variant*& stk, const T& v)
        { ::new(++stk) variant(v);  }

inline void PUSH(variant*& stk, int type, object* obj)
        { ::new(++stk) variant(variant::Type(type), obj);  }

inline void POP(variant*& stk)
        { (*stk--).~variant(); }

inline void POPPOD(variant*& stk)
        { assert(!stk->is_anyobj()); stk--; }

inline void POPTO(variant*& stk, variant* dest)     // ... to uninitialized area
        { *(podvar*)dest = *(podvar*)stk; stk--; }

inline void STORETO(variant*& stk, variant* dest)
        { dest->~variant(); POPTO(stk, dest); }

#define SETPOD(dest,v) (::new(dest) variant(v))

#define BINARY_INT(op) { (stk - 1)->_ord() op stk->_ord(); POPPOD(stk); }
#define UNARY_INT(op)  { stk->_ord() = op stk->_ord(); }


void runRabbitRun(Context* context, stateobj* self, rtstack& stack, const char* ip)
{
    // TODO: check for stack overflow
    register variant* stk = stack.bp - 1;
    try
    {
loop:
        switch(*ip++)
        {
        case opEnd:             while (stk >= stack.bp) POP(stk); goto exit;
        case opNop:             break;
        case opExit:            doExit(); break;

        case opLoadTypeRef:     PUSH(stk, ADV<Type*>(ip)); break;
        case opLoadNull:        PUSH(stk, variant::null); break;
        case opLoad0:           PUSH(stk, integer(0)); break;
        case opLoad1:           PUSH(stk, integer(1)); break;
        case opLoadOrd8:        PUSH(stk, integer(ADV<uchar>(ip))); break;
        case opLoadOrd:         PUSH(stk, ADV<integer>(ip)); break;
        case opLoadStr:         PUSH(stk, ADV<str>(ip)); break;
        case opLoadEmptyVar:    PUSH(stk, variant::Type(ADV<char>(ip))); break;
        case opLoadConst:       PUSH(stk, ADV<Definition*>(ip)->value); break;

        case opLoadSelfVar:     PUSH(stk, self->var(ADV<char>(ip))); break;
        case opLoadStkVar:      PUSH(stk, *(stack.bp + ADV<char>(ip))); break;
        case opLoadMember:      *stk = cast<stateobj*>(stk->_rtobj())->var(ADV<char>(ip)); break;

        case opStoreStkVar:     STORETO(stk, stack.bp + ADV<char>(ip)); break;

        case opDeref:           *stk = *(stk->_ptr()); break;
        case opPop:             POP(stk); break;
        case opChrToStr:        *stk = str(stk->_uchar()); break;
        case opVarToVec:        { varvec v; v.push_back(*stk); *stk = v; } break;

        // Arithmetic
        // TODO: range checking in debug mode
        case opAdd:         BINARY_INT(+=); break;
        case opSub:         BINARY_INT(-=); break;
        case opMul:         BINARY_INT(*=); break;
        case opDiv:         BINARY_INT(/=); break;
        case opMod:         BINARY_INT(%=); break;
        case opBitAnd:      BINARY_INT(&=); break;
        case opBitOr:       BINARY_INT(|=); break;
        case opBitXor:      BINARY_INT(^=); break;
        case opBitShl:      BINARY_INT(<<=); break;
        case opBitShr:      BINARY_INT(>>=); break;
        case opBoolXor:     SETPOD(stk - 1, bool((stk - 1)->_ord() ^ stk->_ord())); POPPOD(stk); break;
        case opNeg:         UNARY_INT(-); break;
        case opBitNot:      UNARY_INT(~); break;
        case opNot:         SETPOD(stk, ! stk->_ord()); break;

        default:                invOpcode(); break;
        }
        goto loop;
exit:
        assert(stk == stack.bp - 1);
    }
    catch(exception&)
    {
        while (stk >= stack.bp)
            POP(stk);
        throw;
    }
}


eexit::eexit() throw(): ecmessage("Exit called")  {}
eexit::~eexit() throw()  { }


Type* CodeGen::runConstExpr(Type* resultType, variant& result)
{
    if (resultType == NULL)
        resultType = stkTop();
    storeRet(resultType);
    end();
    rtstack stack(codeseg.stackSize + 1);
    stack.push(variant::null);
    runRabbitRun(NULL, NULL, stack, codeseg.getCode());
    stack.popto(result);
    return resultType;
}


// --- Execution Context --------------------------------------------------- //


CompilerOptions::CompilerOptions()
  : enableDump(true), enableAssert(true), linenumInfo(true),
    vmListing(true), stackSize(8192)
        { modulePath.push_back("./"); }


static str moduleNameFromFileName(const str& n)
    { return remove_filename_path(remove_filename_ext(n)); }


Context::Context()
    : Scope(NULL), options(), modules(), queenBeeDef(new QueenBeeDef())
        { addModuleDef(queenBeeDef); }


Context::~Context()
    { modules.release_all(); }


ModuleDef* Context::addModuleDef(ModuleDef* m)
{
    addUnique(m);
    modules.push_back(m->ref<ModuleDef>());
    return m;
}


ModuleDef* Context::loadModule(const str& filePath)
{
    str modName = moduleNameFromFileName(filePath);
    ModuleDef* mod = addModuleDef(new ModuleDef(modName));
    Compiler compiler(*this, *mod, new intext(NULL, filePath));
    compiler.module();
    return mod;
}


str Context::lookupSource(const str& modName)
{
    for (memint i = 0; i < options.modulePath.size(); i++)
    {
        str t = options.modulePath[i] + "/" + modName + SOURCE_EXT;
        if (isFile(t.c_str()))
            return t;
    }
    throw emessage("Module not found: " + modName);
}


ModuleDef* Context::getModule(const str& modName)
{
    ModuleDef* m = cast<ModuleDef*>(Scope::find(modName));
    if (m == NULL)
        m = loadModule(lookupSource(modName));
    return m;
}


ModuleDef* Context::findModuleDef(Module* m)
{
    for (memint i = 0; i < modules.size(); i++)
        if (modules[i]->module == m)
            return modules[i];
    fatal(0x5003, "Internal: module not found");
    return NULL;
}


variant Context::execute(const str& filePath)
{
    loadModule(filePath);
    rtstack stack(options.stackSize);
    try
    {
        for (memint i = 0; i < modules.size(); i++)
        {
            ModuleDef* m = modules[i];
            m->initialize(this);
            runRabbitRun(this, m->instance, stack, m->getCodeSeg()->getCode());
        }
    }
    catch (eexit&)
    {
        // exit operator called, we are ok with it
    }
    catch (exception&)
    {
        for (memint i = modules.size() - 1; i--; )
            modules[i]->finalize();
        throw;
    }
    variant result = queenBeeDef->instance->var(queenBee->resultVar->id);
    for (memint i = modules.size(); i--; )
        modules[i]->finalize();
    return result;
}
