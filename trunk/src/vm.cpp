
#include "vm.h"
#include "compiler.h"


CodeSeg::CodeSeg(State* s)
    : object(), state(s)
#ifdef DEBUG
    , closed(false)
#endif
    , stackSize(0)
    { }


CodeSeg::~CodeSeg()
    { }


str CodeSeg::cutOp(memint offs)
{
    memint len = oplen((*this)[offs]);
    str s = code.substr(offs, len);
    code.erase(offs, len);
    return s;
}


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


static void failAssertion(const str& cond, const str& modname, integer linenum)
    { throw emessage("Assertion failed \"" + cond + "\" at " + modname + '(' + to_string(linenum) + ')'); }


static void typecastError()
    { throw evariant("Invalid typecast"); }

// static void objectGone()
//     { throw emessage("Object lost before assignment"); }


static void dumpVar(const str& expr, const variant& var, Type* type)
{
    // TODO: dump to serr?
    sio << "# " << expr;
    if (type)
    {
        sio << ": ";
        type->dumpDef(sio);
    }
    sio << " = ";
    dumpVariant(sio, var, type);
    sio << endl;
}


static void byteDictReplace(varvec& v, integer i, const variant& val)
{
    memint size = v.size();
    if (uinteger(i) > 255)
        container::keyerr();
    if (memint(i) == size)
        v.push_back(val);
    else
    {
        if (memint(i) > size)
            v.grow(memint(i) - size + 1);
        v.replace(memint(i), val);
    }
}


#define ADV(T) \
    (ip += sizeof(T), *(T*)(ip - sizeof(T)))

#define PUSH(v) \
    { ::new(stk + 1) variant(v); stk++; }

// #define PUSHT(t,v) { ::new(++stk) variant(variant::Type(t), v); }

#define POP() \
    { (*stk--).~variant(); }

#define POPPOD() \
    { assert(!stk->is_anyobj()); stk--; }

#define INITTO(dest) \
    { *(podvar*)(dest) = *(podvar*)stk; stk--; } // pop to to uninitialized area

#define POPTO(dest) \
    { variant* d = dest; d->~variant(); INITTO(d); }

template <class T>
   inline void SETPOD(variant* dest, const T& v)
        { ::new(dest) variant(v); }

/*
static void MODIFY_CONT(object* o)
{
    // Prepare an object on the stack for modification and assignment:
    // if the object is not unique, decrement its refcount so that when
    // assigning it back to the original holder, no copying takes place.
    // In case of a race condition (in a MT environment), throw an exception.
    if (!o->isunique())
        if (o->release() == 0)
            objectGone();
}
*/

#define BINARY_INT(op) { (stk - 1)->_int() op stk->_int(); POPPOD(); }
#define UNARY_INT(op)  { stk->_int() = op stk->_int(); }


void runRabbitRun(stateobj* self, rtstack& stack, register const char* ip)
{
    // TODO: check for stack overflow
    register variant* stk = stack.bp - 1;
    memint offs; // used in jump calculations
    integer linenum = -1;
    try
    {
loop:  // use goto's instead of while(1) {} so that compilers don't complain
        switch(*ip++)
        {
        // --- 1. MISC CONTROL
        case opEnd:             goto exit;
        case opNop:             break;
        case opExit:            doExit(); break;

        // --- 2. CONST LOADERS
        case opLoadTypeRef:
            PUSH(ADV(Type*));
            break;
        case opLoadNull:
            PUSH(variant::null);
            break;
        case opLoad0:
            PUSH(integer(0));
            break;
        case opLoad1:
            PUSH(integer(1));
            break;
        case opLoadByte:
            PUSH(integer(ADV(uchar)));
            break;
        case opLoadOrd:
            PUSH(ADV(integer));
            break;
        case opLoadStr:
            PUSH(ADV(str));
            break;
        case opLoadEmptyVar:
            PUSH(variant::Type(ADV(char)));
            break;
        case opLoadConst:
            PUSH(ADV(Definition*)->value);  // TODO: better?
            break;

        // --- 3. DESIGNATOR LOADERS
        case opLoadSelfVar:
            PUSH(*self->member(ADV(uchar)));
            break;
        case opLeaSelfVar:
            PUSH((rtobject*)NULL);  // no need to lock "self", should be locked anyway
            PUSH(self->member(ADV(uchar)));
            break;
        case opLoadStkVar:
            PUSH(*(stack.bp + ADV(char)));
            break;
        case opLeaStkVar:
            PUSH((rtobject*)NULL);
            PUSH(stack.bp + ADV(char));
            break;
        case opLoadMember:
            *stk = *cast<stateobj*>(stk->_rtobj())->member(ADV(uchar));
            break;
        case opLeaMember:
            PUSH(cast<stateobj*>(stk->_rtobj())->member(ADV(uchar)));
            break;
        case opDeref:
            {
                reference* r = stk->_ref();
                SETPOD(stk, r->var);
                r->release();
            }
            break;
        case opLeaRef:
            PUSH(&stk->_ref()->var);
            break;

        // --- 4. STORERS
        case opInitSelfVar:
            INITTO(self->member(ADV(uchar)));
            break;
        case opInitStkVar:
            INITTO(stack.bp + memint(ADV(char)));
            break;
        case opStoreSelfVar:
            POPTO(self->member(ADV(uchar)));
            break;
        case opStoreStkVar:
            POPTO(stack.bp + memint(ADV(char)));
            break;
        case opStoreMember:
            POPTO(cast<stateobj*>((stk - 1)->_rtobj())->member(ADV(uchar)));
            POP();
            break;
        case opStoreRef:
            POPTO(&((stk - 1)->_ref()->var));
            POP();
            break;

        // --- 5. DESIGNATOR OPS, MISC
        case opMkSubrange:
            *(stk - 1) = ADV(Ordinal*)->createSubrange((stk - 1)->_int(), stk->_int());
            POP();
            break;
        case opMkRef:
            SETPOD(stk, new reference((podvar*)stk));
            break;
        case opNonEmpty:
            *stk = int(!stk->empty());
            break;
        case opPop:
            POP();
            break;
        case opCast:
            if (!ADV(Type*)->isCompatibleWith(*stk))
                typecastError();
            break;

        // --- 6. STRINGS, VECTORS
        case opChrToStr:
            *stk = str(stk->_int());
            break;
        case opChrCat:
            (stk - 1)->_str().push_back(stk->_uchar());
            POPPOD();
            break;
        case opStrCat:
            (stk - 1)->_str().append(stk->_str());
            POP();
            break;
        case opVarToVec:
            *stk = varvec(*stk);
            break;
        case opVarCat:
            (stk - 1)->_vec().push_back(*stk);
            POP();
            break;
        case opVecCat:
            (stk - 1)->_vec().append(stk->_vec());
            POP();
            break;
        case opStrLen:
            *stk = integer(stk->_str().size());
            break;
        case opVecLen:
            *stk = integer(stk->_vec().size());
            break;
        case opStrElem:
            *(stk - 1) = (stk - 1)->_str().at(memint(stk->_int()));  // *OVR
            POPPOD();
            break;
        case opVecElem:
            *(stk - 1) = (stk - 1)->_vec().at(memint(stk->_int()));  // *OVR
            POPPOD();
            break;
        case opStoreStrElem:    // -char -int -ptr -obj
            (stk - 2)->_ptr()->_str().replace((stk - 1)->_int(), stk->_uchar());
            POPPOD(); POPPOD(); POPPOD(); POP();
            break;

        // --- 7. SETS
        case opElemToSet:
            *stk = varset(*stk);
            break;
        case opSetAddElem:
            (stk - 1)->_set().find_insert(*stk);
            POP();
            break;
        case opElemToByteSet:
            *stk = ordset(stk->_int());
            break;
        case opRngToByteSet:
            *(stk - 1) = ordset((stk - 1)->_int(), stk->_int());
            POPPOD();
            break;
        case opByteSetAddElem:
            (stk - 1)->_ordset().find_insert(stk->_int());
            POPPOD();
            break;
        case opByteSetAddRng:
            (stk - 2)->_ordset().find_insert((stk - 1)->_int(), stk->_int());
            POPPOD();
            POPPOD();
            break;

        // --- 8. DICTIONARIES
        case opPairToDict:
            *(stk - 1) = vardict(*(stk - 1), *stk);
            POP();
            break;
        case opDictAddPair:
            (stk - 2)->_dict().find_replace(*(stk - 1), *stk);
            POP();
            POP();
            break;
        case opPairToByteDict:
            {
                integer i = (stk - 1)->_int();
                SETPOD(stk - 1, varvec());
                byteDictReplace((stk - 1)->_vec(), i, *stk);
                POP();
            }
            break;
        case opByteDictAddPair:
            byteDictReplace((stk - 2)->_vec(), (stk - 1)->_int(), *stk);
            POP();
            POPPOD();
            break;
        case opDictElem:
            {
                const variant* v = (stk - 1)->_dict().find(*stk);
                POP();
                if (v)
                    *stk = *v;  // potentially dangerous if dict has refcount=1, which it shouldn't
                else
                    container::keyerr();
            }
            break;
        case opByteDictElem:
            {
                integer i = stk->_int();
                POPPOD();
                if (i < 0 || i >= stk->_vec().size())
                    container::keyerr();
                const variant& v = stk->_vec()[memint(i)];
                if (v.is_null())
                    container::keyerr();
                *stk = v;  // same as for opDictElem
            }
            break;


        // --- 9. ARITHMETIC
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
        // case opBoolXor:     SETPOD(stk - 1, bool((stk - 1)->_int() ^ stk->_int())); POPPOD(stk); break;
        case opNeg:         UNARY_INT(-); break;
        case opBitNot:      UNARY_INT(~); break;
        case opNot:         UNARY_INT(!); break;

        // --- 10. BOOLEAN
        case opCmpOrd:
            SETPOD(stk - 1, (stk - 1)->_int() - stk->_int());
            POPPOD();
            break;
        case opCmpStr:
            *(stk - 1) = integer((stk - 1)->_str().compare(stk->_str()));
            POP();
            break;
        case opCmpVar:
            *(stk - 1) = int(*(stk - 1) == *stk) - 1;
            POP();
            break;

        case opEqual:       stk->_int() = stk->_int() == 0; break;
        case opNotEq:       stk->_int() = stk->_int() != 0; break;
        case opLessThan:    stk->_int() = stk->_int() < 0; break;
        case opLessEq:      stk->_int() = stk->_int() <= 0; break;
        case opGreaterThan: stk->_int() = stk->_int() > 0; break;
        case opGreaterEq:   stk->_int() = stk->_int() >= 0; break;

        // --- 11. JUMPS
        case opJump:
                // beware of strange behavior of the GCC optimizer: this should be done in 2 steps
            offs = ADV(jumpoffs);
            ip += offs;
            break;
        case opJumpFalse:
            UNARY_INT(!);
        case opJumpTrue:
            offs = ADV(jumpoffs);
            if (stk->_int())
                ip += offs;
            POP();
            break;
        case opJumpAnd:
            UNARY_INT(!);
        case opJumpOr:
            offs = ADV(jumpoffs);
            if (stk->_int())
                ip += offs;
            else
                POP();
            break;

        // --- 12. DEBUGGING, DIAGNOSTICS
        case opLineNum:
            linenum = ADV(integer);
            break;
        case opAssert:
            {
                str& cond = ADV(str);
                if (!stk->_int())
                    failAssertion(cond,
                        self == NULL ? str("*") :
                            self->getType()->getParentModule()->getName(), linenum);
                POPPOD();
            }
            break;
        case opDump:
            {
                str& expr = ADV(str);
                dumpVar(expr, *stk, ADV(Type*));
                POP();
            }
            break;

        default:
            invOpcode();
            break;
        }
        goto loop;
exit:
#ifndef DEBUG
        while (stk >= stack.bp)
            POP();
#endif
        assert(stk == stack.bp - 1);
    }
    catch(exception&)
    {
        while (stk >= stack.bp)
            POP();
        throw;
    }
}


eexit::eexit() throw(): emessage("Exit called")  {}
eexit::~eexit() throw()  { }


Type* CodeGen::runConstExpr(Type* resultType, variant& result)
{
    if (resultType == NULL)
        resultType = stkTop();
    storeRet(resultType);
    end();
    rtstack stack(codeseg.stackSize + 1);
    stack.push(variant::null);  // storage for the return value
    runRabbitRun(NULL, stack, codeseg.getCode());
    stack.popto(result);
    return resultType;
}


// --- Execution Context --------------------------------------------------- //


ModuleInstance::ModuleInstance(Module* m)
    : Symbol(m->getName(), MODULEINST, m, NULL), module(m), obj()  { }


void ModuleInstance::run(Context* context, rtstack& stack)
{
    assert(module->isComplete());

    // Assign module vars. This allows to generate code that accesses module
    // static data by variable id, so that code is context-independant
    for (memint i = 0; i < module->uses.size(); i++)
    {
        SelfVar* v = module->uses[i];
        stateobj* o = context->getModuleObject(v->getModuleType());
        *obj->member(v->id) = o;
    }

    // Run module initialization or main code
    runRabbitRun(obj, stack, module->getCodeSeg()->getCode());
}


void ModuleInstance::finalize()
{
    if (!obj.empty())
    {
        try
        {
            obj->collapse();   // destroy possible circular references first
            obj.clear();       // now free the object itself
        }
        catch (exception&)
        {
            fatal(0x5006, "Exception in destructor");
        }
    }
}


CompilerOptions::CompilerOptions()
  : enableDump(true), enableAssert(true), lineNumbers(true),
    vmListing(true), stackSize(8192)
        { modulePath.push_back("./"); }


void CompilerOptions::setDebugOpts(bool flag)
{
    enableDump = flag;
    enableAssert = flag;
    lineNumbers = flag;
    vmListing = flag;
}


static str moduleNameFromFileName(const str& n)
    { return remove_filename_path(remove_filename_ext(n)); }


Context::Context()
    : Scope(NULL), queenBeeInst(addModule(queenBee))  { }


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
    objptr<Module> m = new Module(modName, filePath);
    addModule(m);
    Compiler compiler(*this, *m, new intext(NULL, filePath));
    compiler.compileModule();
    if (options.enableDump || options.vmListing)
        dump(remove_filename_ext(filePath) + ".lst");
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
    // TODO: to have a global cache of compiled modules, not just within the context
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
        fatal(0x5003, "Module not found");
    return *o;
}


void Context::instantiateModules()
{
    for (memint i = 0; i < instances.size(); i++)
    {
        ModuleInstance* inst = instances[i];
        if (!inst->module->isComplete())
            fatal(0x5004, "Module not compiled");
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


void Context::dump(const str& listingPath)
{
    outtext stm(NULL, listingPath);
    stm << "#FLAG INT_SIZE " << sizeof(integer) * 8 << endl;
    stm << "#FLAG PTR_SIZE " << sizeof(void*) * 8 << endl;
    for (memint i = 0; i < instances.size(); i++)
        instances[i]->module->dump(stm);
}


variant Context::execute()
{
    // loadModule(filePath);

    // Now that all modules are compiled and their dataseg sizes are known, we can
    // instantiate the objects
    instantiateModules();

    // Run init code segments for all modules; the last one is the main program
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

    // Program exit variable (not necessarily int, can be anything)
    variant result = *queenBeeInst->obj->member(queenBee->resultVar->id);
    clear();
    return result;
}


void initVm()  { if (opMaxCode > 255) fatal(0x5001, "Opcodes > 255"); }
void doneVm()  { }

