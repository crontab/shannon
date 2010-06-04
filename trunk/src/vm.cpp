
#include "vm.h"
#include "compiler.h"


CodeSeg::CodeSeg(State* stateType)
    : rtobject(stateType)
#ifdef DEBUG
    , closed(false)
#endif
    , stackSize(0)
    { }


CodeSeg::~CodeSeg()
    { }


void CodeSeg::close()
{
#ifdef DEBUG
    assert(!closed);
    closed = true;
#endif
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

template <class T>
   inline void SETPOD(variant* dest, const T& v)
        { ::new(dest) variant(v); }


#define BINARY_INT(op) { (stk - 1)->_ord() op stk->_ord(); POPPOD(stk); }
#define UNARY_INT(op)  { stk->_ord() = op stk->_ord(); }


void runRabbitRun(Context*, stateobj* self, rtstack& stack, const char* ip)
{
    // TODO: check for stack overflow
    register variant* stk = stack.bp - 1;
    try
    {
loop:
        switch(*ip++)
        {
        case opEnd:         while (stk >= stack.bp) POP(stk); goto exit;
        case opNop:         break;
        case opExit:        doExit(); break;

        case opLoadTypeRef: PUSH(stk, ADV<Type*>(ip)); break;
        case opLoadNull:    PUSH(stk, variant::null); break;
        case opLoad0:       PUSH(stk, integer(0)); break;
        case opLoad1:       PUSH(stk, integer(1)); break;
        case opLoadOrd8:    PUSH(stk, integer(ADV<uchar>(ip))); break;
        case opLoadOrd:     PUSH(stk, ADV<integer>(ip)); break;
        case opLoadStr:     PUSH(stk, ADV<str>(ip)); break;
        case opLoadEmptyVar:PUSH(stk, variant::Type(ADV<char>(ip))); break;
        case opLoadConst:   PUSH(stk, ADV<Definition*>(ip)->value); break;  // TODO: better

        case opLoadSelfVar: PUSH(stk, self->var(ADV<char>(ip))); break;
        case opLoadStkVar:  PUSH(stk, *(stack.bp + ADV<char>(ip))); break;
        case opLoadMember:  *stk = cast<stateobj*>(stk->_rtobj())->var(ADV<char>(ip)); break;
        // TODO: load "far" self var, via a pointer to a module object

        case opInitStkVar:  POPTO(stk, stack.bp + ADV<char>(ip)); break;

        case opPop:         POP(stk); break;
        case opChrToStr:    *stk = str(stk->_uchar()); break;
        case opChrCat:      (stk - 1)->_str().push_back(stk->_uchar()); POPPOD(stk); break;
        case opStrCat:      (stk - 1)->_str().append(stk->_str()); POP(stk); break;
        case opVarToVec:    { varvec v; v.push_back(*stk); *stk = v; } break;
        case opVarCat:      (stk - 1)->_vec().push_back(*stk); POP(stk); break;
        case opVecCat:      (stk - 1)->_vec().append(stk->_vec()); POP(stk); break;

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

        // Comparators
        case opCmpOrd:      SETPOD(stk - 1, (stk - 1)->_ord() - stk->_ord()); POPPOD(stk); break;
        case opCmpStr:      *(stk - 1) = integer((stk - 1)->_str().compare(stk->_str())); POP(stk); break;
        case opCmpVar:      *(stk - 1) = int(*(stk - 1) == *stk) - 1; POP(stk); break;

        case opEqual:       SETPOD(stk, stk->_ord() == 0); break;
        case opNotEq:       SETPOD(stk, stk->_ord() != 0); break;
        case opLessThan:    SETPOD(stk, stk->_ord() < 0); break;
        case opLessEq:      SETPOD(stk, stk->_ord() <= 0); break;
        case opGreaterThan: SETPOD(stk, stk->_ord() > 0); break;
        case opGreaterEq:   SETPOD(stk, stk->_ord() >= 0); break;

        // Jumps
        case opJump:        { memint o = ADV<jumpoffs>(ip); ip += o; } break; // beware of a strange bug in GCC, this should be done in 2 steps
        case opJumpTrue:    { memint o = ADV<jumpoffs>(ip); if (stk->_ord())  ip += o; POP(stk); } break;
        case opJumpFalse:   { memint o = ADV<jumpoffs>(ip); if (!stk->_ord()) ip += o; POP(stk); } break;
        case opJumpOr:      { memint o = ADV<jumpoffs>(ip); if (stk->_ord())  ip += o; else POP(stk); } break;
        case opJumpAnd:     { memint o = ADV<jumpoffs>(ip); if (!stk->_ord()) ip += o; else POP(stk); } break;

        default:            invOpcode(); break;
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


ModuleInstance::ModuleInstance(Module* m)
    : Symbol(m->getModuleName(), DEFINITION, m), module(m), obj()  { }


void ModuleInstance::run(Context* context, rtstack& stack)
{
    // Assign module vars. This allows to generate code that accesses module
    // static data by variable id, so that code is context-independant
    for (memint i = 0; i < module->uses.size(); i++)
    {
        Variable* v = module->uses[i];
        stateobj* o = context->getModuleObject(v->getModuleType());
        obj->var(v->id) = o;
    }

    // Run module initialization or main code
    runRabbitRun(context, obj, stack, module->codeseg->getCode());
}


void ModuleInstance::finalize()
{
    if (!obj.empty())
        try
        {
            obj->collapse();   // destroy possible circular references first
            obj.clear();       // now free the object itself
        }
        catch (exception&)
        {
            fatal(0x5006, "Internal: exception in destructor");
        }
}


CompilerOptions::CompilerOptions()
  : enableDump(true), enableAssert(true), linenumInfo(true),
    vmListing(true), stackSize(8192)
        { modulePath.push_back("./"); }


static str moduleNameFromFileName(const str& n)
    { return remove_filename_path(remove_filename_ext(n)); }


Context::Context()
    : Scope(NULL), options(), instances(), queenBeeInst(addModule(queenBee))  { }


Context::~Context()
    { instances.release_all(); }


ModuleInstance* Context::addModule(Module* m)
{
    objptr<ModuleInstance> inst = new ModuleInstance(m);
    Scope::addUnique(inst);
    instances.push_back(inst->grab<ModuleInstance>());
    return inst;
}


Module* Context::loadModule(const str& filePath)
{
    str modName = moduleNameFromFileName(filePath);
    objptr<Module> m = new Module(modName);
    addModule(m);
    Compiler compiler(*this, *m, new intext(NULL, filePath));
    compiler.compileModule();
    return m;
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


Module* Context::getModule(const str& modName)
{
    // TODO: find a moudle by full path, not just name (hash by path/name?)
    // TODO: to have a global cache of compiled modules, not just within the econtext
    ModuleInstance* inst = cast<ModuleInstance*>(Scope::find(modName));
    if (inst != NULL)
        return inst->module;
    else
        return loadModule(lookupSource(modName));
}


stateobj* Context::getModuleObject(Module* m)
{
    stateobj* const* o = modObjMap.find(m);
    if (o == NULL)
        fatal(0x5003, "Internal: module not found");
    return *o;
}


void Context::instantiateModules()
{
    // Now that all modules are compiled and their dataseg sizes are known, we can
    // instantiate the objects:
    for (memint i = 0; i < instances.size(); i++)
    {
        ModuleInstance* inst = instances[i];
        inst->obj = inst->module->newInstance();
        assert(modObjMap.find(inst->module) == NULL);
        modObjMap.find_replace(inst->module, inst->obj);
    }
}


void Context::clear()
{
    for (memint i = instances.size(); i--; )
        instances[i]->finalize();
    modObjMap.clear();
}


variant Context::execute(const str& filePath)
{
    loadModule(filePath);
    instantiateModules();
    rtstack stack(options.stackSize);
    try
    {
        for (memint i = 0; i < instances.size(); i++)
            instances[i]->run(this, stack);
    }
    catch (eexit&)
    {
        // exit operator called, we are ok with it
    }
    catch (exception&)
    {
        clear();
        throw;
    }
    variant result = queenBeeInst->obj->var(queenBee->resultVar->id);
    clear();
    return result;
}

