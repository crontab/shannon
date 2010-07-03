
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


static void invOpcode()                 { fatal(0x5002, "Invalid opcode"); }
static void doExit(const variant& r)    { throw eexit(r); }


static void failAssertion(const str& cond, const str& modname, integer linenum)
    { throw emessage("Assertion failed \"" + cond + "\" at " + modname + '(' + to_string(linenum) + ')'); }

static void typecastError()
    { throw evariant("Invalid typecast"); }

static void constExprErr()
    { throw emessage("Variable in constant expression"); }


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


static void byteDictDelete(varvec& v, integer i)
{
    memint size = v.size();
    if (uinteger(i) >= umemint(size) || v[memint(i)].is_null())
        container::keyerr();
    if (memint(i) == size - 1)
        v.pop_back();
    else
        v.replace(memint(i), variant());
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


#define BINARY_INT(op) { (stk - 1)->_int() op stk->_int(); POPPOD(); }
#define UNARY_INT(op)  { stk->_int() = op stk->_int(); }


void runRabbitRun(variant* outer, variant* bp, register const char* ip)
{
    // TODO: check for stack overflow (during function call?)
    register variant* stk = bp - 1;
    variant* self = NULL;
    integer linenum = -1;
    variant* callOuter = NULL;
    try
    {
loop:  // use goto instead of while(1) {} so that compilers don't complain
        switch(*ip++)
        {

        // --- 1. MISC CONTROL -----------------------------------------------
        case opEnd:             goto exit;
        case opConstExprErr:    constExprErr(); break;
        case opExit:            doExit(*stk); break;
        case opEnter:
            self = stk + 1;
            stk += ADV(uchar);
            break;
        case opLeave:
            for (memint i = ADV(uchar); i--; )
                POP();
            goto exit; // !
        case opEnterCtor:
            self = cast<stateobj*>((bp + ADV(char))->_rtobj())->varbase();
            break;


        // --- 2. CONST LOADERS ----------------------------------------------
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
        case opLoadThis:
            PUSH(stateobj::objbase(outer));
            break;


        // --- 3. DESIGNATOR LOADERS -----------------------------------------
        case opLoadSelfVar:
            PUSH(*(self + ADV(uchar)));
            break;
        case opLoadOuterVar:
            PUSH(*(outer + ADV(uchar)));
            break;
        case opLoadStkVar:
            PUSH(*(bp + ADV(char)));
            break;
        case opLoadMember:
            *stk = *cast<stateobj*>(stk->_rtobj())->member(ADV(uchar));
            break;
        case opDeref:
            {
                reference* r = stk->_ref();
                SETPOD(stk, r->var);
                r->release();
            }
            break;

        case opLeaSelfVar:
            PUSH((rtobject*)NULL);  // no need to lock "self", should be locked anyway
            PUSH(self + ADV(uchar));
            break;
        case opLeaOuterVar:
            PUSH((rtobject*)NULL);
            PUSH(outer + ADV(uchar));
            break;
        case opLeaStkVar:
            PUSH((rtobject*)NULL);
            PUSH(bp + ADV(char));
            break;
        case opLeaMember:
            PUSH(cast<stateobj*>(stk->_rtobj())->member(ADV(uchar)));
            break;
        case opLeaRef:
            PUSH(&stk->_ref()->var);
            break;


        // --- 4. STORERS ----------------------------------------------------
        case opInitSelfVar:
            INITTO(self + ADV(uchar));
            break;
        case opInitStkVar:
            INITTO(bp + memint(ADV(char)));
            break;
        case opStoreSelfVar:
            POPTO(self + ADV(uchar));
            break;
        case opStoreOuterVar:
            POPTO(outer + ADV(uchar));
            break;
        case opStoreStkVar:
            POPTO(bp + memint(ADV(char)));
            break;
        case opStoreMember:
            POPTO(cast<stateobj*>((stk - 1)->_rtobj())->member(ADV(uchar)));
            POP();
            break;
        case opStoreRef:
            POPTO(&((stk - 1)->_ref()->var));
            POP();
            break;


        // --- 5. DESIGNATOR OPS, MISC ---------------------------------------
        case opMkSubrange:
            {
                Ordinal* t = ADV(Ordinal*)->createSubrange((stk - 1)->_int(), stk->_int());
                ADV(State*)->registerType(t);
                *(stk - 1) = t;
                POPPOD();
            }
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
        case opPopPod:
            POPPOD();
            break;
        case opCast:
            if (!ADV(Type*)->isCompatibleWith(*stk))
                typecastError();
            break;
        case opIsType:
            *stk = int(ADV(Type*)->isCompatibleWith(*stk));
            break;


        // --- 6. STRINGS, VECTORS -------------------------------------------
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
        case opSubstr:  // -{int,void} -int -str +str
            {
                memint pos = memint((stk - 1)->_int());  // *OVR
                str& s = (stk - 2)->_str();
                s = stk->is_null() ? s.substr(pos)
                    : s.substr(pos, stk->_int() - pos + 1);  // *OVR
                POPPOD(); POPPOD();
            }
            break;
        case opSubvec:  // -{int,void} -int -vec +vec
            {
                memint pos = memint((stk - 1)->_int());  // *OVR
                varvec& v = (stk - 2)->_vec();
                v = stk->is_null() ? v.subvec(pos)
                    : v.subvec(pos, stk->_int() - pos + 1);  // *OVR
                POPPOD(); POPPOD();
            }
            break;
        case opStoreStrElem:    // -char -int -ptr -obj
            (stk - 2)->_ptr()->_str().replace((stk - 1)->_int(), stk->_uchar());  // *OVR
            POPPOD(); POPPOD(); POPPOD(); POP();
            break;
        case opStoreVecElem:    // -var -int -ptr -obj
            (stk - 2)->_ptr()->_vec().replace((stk - 1)->_int(), *stk);  // *OVR
            POP(); POPPOD(); POPPOD(); POP();
            break;
        case opDelStrElem:      // -int -ptr -obj
            (stk - 1)->_ptr()->_str().erase(stk->_int(), 1);  // *OVR
            POPPOD(); POPPOD(); POP();
            break;
        case opDelVecElem:      // -int -ptr -obj
            (stk - 1)->_ptr()->_vec().erase(stk->_int());  // *OVR
            POPPOD(); POPPOD(); POP();
            break;
        // *OVR: integer type is reduced to memint in some configs


        // --- 7. SETS -------------------------------------------------------
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
        case opInSet:
            *(stk - 1) = int(stk->_set().find(*(stk - 1)));
            POP();
            break;
        case opInByteSet:
            (stk - 1)->_int() = int(stk->_ordset().find((stk - 1)->_int()));
            POP();
            break;
        case opInBounds:
            stk->_int() = int(ADV(Ordinal*)->isInRange(stk->_int()));
            break;
        case opInRange:
            {
                integer i = (stk - 2)->_int();
                (stk - 2)->_int() = int(i >= (stk - 1)->_int() && i <= stk->_int());
                POPPOD(); POPPOD();
            }
            break;
        case opSetElem:
            POP(); POP(); PUSH(); // see CodeGen::loadContainerElem()
            break;
        case opByteSetElem:
            POPPOD(); POP(); PUSH();
            break;
        case opDelSetElem:     // -var -ptr -obj
            (stk - 1)->_ptr()->_set().find_erase(*stk);
            POP(); POPPOD(); POP();
            break;
        case opDelByteSetElem:     // -int -ptr -obj
            (stk - 1)->_ptr()->_ordset().find_erase(stk->_int());
            POPPOD(); POPPOD(); POP();
            break;


        // --- 8. DICTIONARIES -----------------------------------------------
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
        case opInDict:
            *(stk - 1) = int(stk->_dict().find_key(*(stk - 1)));
            POP();
            break;
        case opInByteDict:
            {
                integer i = (stk - 1)->_int();
                const varvec& v = stk->_vec();
                (stk - 1)->_int() = int(i >= 0 && i < v.size() && !v[memint(i)].is_null());
                POP();
            }
            break;
        case opStoreDictElem:  // -var -var -ptr -obj
            (stk - 2)->_ptr()->_dict().find_replace(*(stk - 1), *stk);
            POP(); POP(); POPPOD(); POP();
            break;
        case opStoreByteDictElem:   // -var -int -ptr -obj
            byteDictReplace((stk - 2)->_ptr()->_vec(), (stk - 1)->_int(), *stk);
            POP(); POPPOD(); POPPOD(); POP();
            break;
        case opDelDictElem:     // -var -ptr -obj
            (stk - 1)->_ptr()->_dict().find_erase(*stk);
            POP(); POPPOD(); POP();
            break;
        case opDelByteDictElem: // -int -ptr -obj
            byteDictDelete((stk - 1)->_ptr()->_vec(), stk->_int());
            POPPOD(); POPPOD(); POP();
            break;


        // --- 9. ARITHMETIC -------------------------------------------------
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
        case opNeg:         UNARY_INT(-); break;
        case opBitNot:      UNARY_INT(~); break;
        case opNot:         UNARY_INT(!); break;


        // --- 10. BOOLEAN ---------------------------------------------------
        case opCmpOrd:
            (stk - 1)->_int() -= stk->_int();
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

        case opCaseOrd:     stk->_int() = int(stk->_int() == (stk - 1)->_int()); break;
        case opCaseRange:
            {
                integer i = (stk - 2)->_int();
                (stk - 1)->_int() = int(i >= (stk - 1)->_int() && i <= stk->_int());
                POPPOD();
            }
            break;
        case opCaseStr:     *stk = int(stk->_str() == (stk - 1)->_str()); break;
        case opCaseVar:     *stk = int(*stk == *(stk - 1)); break;


        // --- 11. JUMPS, CALLS ----------------------------------------------
        case opJump:
            {
                jumpoffs offs = ADV(jumpoffs); // beware of strange behavior of the GCC optimizer: this should be done in 2 steps
                ip += offs;
            }
            break;
        case opJumpFalse:
            {
                jumpoffs offs = ADV(jumpoffs);
                if (!stk->_int())
                    ip += offs;
                POP();
            }
            break;
        case opJumpTrue:
            {
                jumpoffs offs = ADV(jumpoffs);
                if (stk->_int())
                    ip += offs;
                POP();
            }
            break;
        case opJumpAnd:
            {
                jumpoffs offs = ADV(jumpoffs);
                if (!stk->_int())
                    ip += offs;
                else
                    POP();
            }
            break;
        case opJumpOr:
            {
                jumpoffs offs = ADV(jumpoffs);
                if (stk->_int())
                    ip += offs;
                else
                    POP();
            }
            break;

        case opSelfCall:
            callOuter = self;
doStaticCall:
            {
                State* state = ADV(State*);
                runRabbitRun(callOuter, stk + 1, state->getCode());
                memint argCount = state->popArgCount;
                while (argCount--)
                    POP();
            }
            break;
        case opOuterCall:
            callOuter = outer;
            goto doStaticCall;
        case opStaticCall:
            callOuter = NULL;
            goto doStaticCall;


        // --- 12. DEBUGGING, DIAGNOSTICS ------------------------------------
        case opLineNum:
            linenum = ADV(integer);
            break;
        case opAssert:
            {
                str& cond = ADV(str);
                if (!stk->_int())
                    // TODO: module name
                    failAssertion(cond, "?", linenum);
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

        case opInv:  // silence the opcode checkers (opcodes.sh in particular)
        default:
            invOpcode();
            break;
        }
        goto loop;
exit:
#ifndef DEBUG
        while (stk >= bp)
            POP();
#endif
        assert(stk == bp - 1);
    }
    catch(exception&)
    {
        while (stk >= bp)
            POP();
        throw;
    }
}


eexit::eexit(const variant& r) throw(): exception(), result(r)  {}
eexit::~eexit() throw()  { }
const char* eexit::what() throw()  { return "Exit called"; }


Type* CodeGen::runConstExpr(Type* resultType, variant& result)
{
    if (resultType == NULL)
        resultType = stkTop();
    storeRet(resultType);
    end();

    rtstack stack(codeseg.stackSize + 1);
    stack.push(variant::null);  // storage for the return value
    try
    {
        runRabbitRun(NULL, stack.bp, codeseg.getCode());
    }
    catch (exception&)
    {
        stack.pop();
        throw;
    }
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
    stack.push(obj.get());
    try
    {
        runRabbitRun(obj->varbase(), stack.bp, module->getCode());
    }
    catch (exception&)
    {
        stack.pop();
        throw;
    }
    stack.pop();
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
    vmListing(true), compileOnly(false), stackSize(8192)
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
    : Scope(false, NULL), queenBeeInst(addModule(queenBee))  { }


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
    // TODO: store the current file name in a named const, say __FILE__
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
    // TODO: Linear search, can be improved later
    for (memint i = 0; i < instances.size(); i++)
        if (instances[i]->module == m)
            return instances[i]->obj;
    fatal(0x5003, "Module not found");
    return NULL;
}


void Context::instantiateModules()
{
    for (memint i = 0; i < instances.size(); i++)
    {
        ModuleInstance* inst = instances[i];
        if (!inst->module->isComplete())
            fatal(0x5004, "Module not compiled");
        inst->obj = inst->module->newInstance();
    }
}


void Context::clear()
{
    for (memint i = instances.size(); i--; )
        instances[i]->finalize();
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
    if (options.compileOnly)
        return variant();

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
    catch (eexit& e)
    {
        // Program exit variable (not necessarily int, can be anything)
        *queenBeeInst->obj->member(queenBee->resultVar->id) = e.result;
    }
    catch (exception&)
    {
        clear();
        throw;
    }

    variant result = *queenBeeInst->obj->member(queenBee->resultVar->id);
    clear();
    return result;
}


void initVm()  { if (opMaxCode > 255) fatal(0x5001, "Opcodes > 255"); }
void doneVm()  { }

