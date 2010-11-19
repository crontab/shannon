#ifndef __VM_H
#define __VM_H

#include "common.h"
#include "runtime.h"
#include "parser.h"
#include "typesys.h"


enum OpCode
{
    // NOTE: the relative order of many of these instructions in their groups is significant

    // --- 1. MISC CONTROL
    opEnd,              // end execution and return
    opConstExprErr,     // placeholder for var loaders to generate an error
    opExit,             // throws eexit()
    opEnter,            // [varcount:u8]
    opLeave,            // [varcount:u8]
    opEnterCtor,        // [State*]

    // --- 2. CONST LOADERS
    // --- begin undoable loaders
    opLoadTypeRef,      // [Type*] +obj
    opLoadNull,         // +null
    opLoad0,            // +int
    opLoad1,            // +int
    opLoadByte,         // [int:u8] +int
    opLoadOrd,          // [int] +int
    opLoadStr,          // [str] +str
    opLoadEmptyVar,     // [variant::Type:u8] + var
    opLoadConst,        // [Definition*] +var
    opLoadOuterObj,     // +stateobj
    opLoadDataSeg,      // +module-obj
    // opLoadSelfObj,      // equivalent to opLoadStkVar 'result'
    opLoadOuterFuncPtr, // [State*] +funcptr -- see also opMkFuncPtr
    opLoadSelfFuncPtr,  // [State*] +funcptr
    opLoadNullFuncPtr,  // [State*] +funcptr -- used in const expressions

    // --- 3. DESIGNATOR LOADERS
    // --- begin grounded loaders
    opLoadSelfVar,      // [self.idx:u8] +var
    opLoadOuterVar,     // [outer.idx:u8] +var
    opLoadStkVar,       // [stk.idx:s8] +var
    // --- end undoable loaders
    opLoadMember,       // [stateobj.idx:u8] -stateobj +var
    opDeref,            // -ref +var
    // --- end grounded loaders

    opLeaSelfVar,       // [self.idx:u8] +obj(0) +ptr
    opLeaOuterVar,      // [outer.idx:u8] +obj(0) +ptr
    opLeaStkVar,        // [stk.idx:s8] +obj(0) +ptr
    opLeaMember,        // [stateobj.idx:u8] -stateobj + stateobj +ptr
    opLeaRef,           // -ref +ref +ptr

    // --- 4. STORERS
    opInitSelfVar,      // [self.idx:u8] -var
    opInitStkVar,       // [stk.idx:s8] -var
    // --- begin grounded storers
    opStoreSelfVar,     // [self.idx:u8] -var
    opStoreOuterVar,    // [outer.idx:u8] -var
    opStoreStkVar,      // [stk.idx:s8] -var
    opStoreMember,      // [stateobj.idx:u8] -var -stateobj
    opStoreRef,         // -var -ref
    // --- end grounded storers
    opIncStkVar,        // [stk.idx:s8] -- for loop helper

    // --- 5. DESIGNATOR OPS, MISC
    opMkRange,          // -int -int +range
    opMkRef,            // -var +ref
    opMkFuncPtr,        // [State*] -obj +funcptr
    opNonEmpty,         // -var +bool
    opPop,              // -var
    opPopPod,           // -int
    opCast,             // [Type*] -var +var
    opIsType,           // [Type*] -var +bool

    // --- 6. STRINGS, VECTORS
    opChrToStr,         // -char +str
    opChrCat,           // -char -str +str
    opStrCat,           // -str -str +str
    opVarToVec,         // -var +vec
    opVarCat,           // -var -vec +vec
    opVecCat,           // -vec -vec +vec
    opStrLen,           // -str +int
    opVecLen,           // -str +int
    opStrElem,          // -int -str +char
    opVecElem,          // -int -vec +var
    opSubstr,           // -{int,void} -int -str +str
    opSubvec,           // -{int,void} -int -vec +vec
    opStoreStrElem,     // -char -int -ptr -obj
    opStoreVecElem,     // -var -int -ptr -obj
    opDelStrElem,       // -int -ptr -obj
    opDelVecElem,       // -int -ptr -obj
    opDelSubstr,        // -{int,void} -int -ptr -obj
    opDelSubvec,        // -{int,void} -int -ptr -obj
    opStrIns,           // -char -int -ptr -obj
    opVecIns,           // -var -int -ptr -obj
    opSubstrIns,        // -str -{int,void} -int -ptr -obj
    opSubvecIns,        // -vec -{int,void} -int -ptr -obj
    // In-place vector concat
    opChrCatAssign,     // -char -ptr -obj
    opStrCatAssign,     // -str -ptr -obj
    opVarCatAssign,     // -var -ptr -obj
    opVecCatAssign,     // -vec -ptr -obj

    // --- 7. SETS
    opElemToSet,        // -var +set
    opSetAddElem,       // -var -set + set
    opElemToByteSet,    // -int +set
    opRngToByteSet,     // -int -int +set
    opByteSetAddElem,   // -int -set +set
    opByteSetAddRng,    // -int -int -set +set
    opInSet,            // -set -var +bool
    opInByteSet,        // -set -int +bool
    opInBounds,         // [Ordinal*] -int +bool
    opInRange,          // -range -int +bool
    opInRange2,         // -int -int -int +bool
    opSetElem,          // -var -set +void
    opByteSetElem,      // -int -set +void
    opDelSetElem,       // -var -ptr -obj
    opDelByteSetElem,   // -int -ptr -obj
    opSetLen,           // -set +int
    opSetKey,           // -int -set +var

    // --- 8. DICTIONARIES
    opPairToDict,       // -var -var +dict
    opDictAddPair,      // -var -var -dict +dict
    opPairToByteDict,   // -var -int +vec
    opByteDictAddPair,  // -var -int -vec +vec
    opDictElem,         // -var -dict +var
    opByteDictElem,     // -int -dict +var
    opInDict,           // -var -dict +bool
    opInByteDict,       // -int -dict +bool
    opStoreDictElem,    // -var -var -ptr -obj
    opStoreByteDictElem,// -var -int -ptr -obj
    opDelDictElem,      // -var -ptr -obj
    opDelByteDictElem,  // -int -ptr -obj
    opDictLen,          // -dict +int
    opDictElemByIdx,    // -int -dict +var
    opDictKeyByIdx,     // -int -dict +var

    // --- 9. ARITHMETIC
    opAdd,              // -int -int +int
    opSub,              // -int -int +int
    opMul,              // -int -int +int
    opDiv,              // -int -int +int
    opMod,              // -int -int +int
    opBitAnd,           // -int -int +int
    opBitOr,            // -int -int +int
    opBitXor,           // -int -int +int
    opBitShl,           // -int -int +int
    opBitShr,           // -int -int +int
    // Arithmetic unary
    opNeg,              // -int +int
    opBitNot,           // -int +int
    opNot,              // -bool +bool
    // Arithmetic in-place, in sync with tokAddAssign etc
    opAddAssign,        // -int -ptr -obj
    opSubAssign,        // -int -ptr -obj
    opMulAssign,        // -int -ptr -obj
    opDivAssign,        // -int -ptr -obj
    opModAssign,        // -int -ptr -obj

    // --- 10. BOOLEAN
    opCmpOrd,           // -int -int +{-1,0,1}
    opCmpStr,           // -str -str +{-1,0,1}
    opCmpVar,           // -var -var +{0,1}
    // see isCmpOp()
    opEqual,            // -int +bool
    opNotEq,            // -int +bool
    opLessThan,         // -int +bool
    opLessEq,           // -int +bool
    opGreaterThan,      // -int +bool
    opGreaterEq,        // -int +bool
    // case label helpers
    opCaseOrd,          // -int -int +int +bool
    opCaseRange,        // -int -int -int +int +bool
    opCaseStr,          // -str -str +str +bool
    opCaseVar,          // -var -var +var +bool
    // for loop helpers
    opStkVarGt,         // [stk.idx:s8] -int +bool
    opStkVarGe,         // [stk.idx:s8] -int +bool

    // --- 11. JUMPS, CALLS
    // Jumps; [dst] is a relative 16-bit offset
    opJump,             // [dst 16]
    opJumpFalse,        // [dst 16] -bool
    opJumpTrue,         // [dst 16] -bool
    // Short bool evaluation: pop if jump, leave it otherwise
    opJumpAnd,          // [dst 16] (-)bool
    opJumpOr,           // [dst 16] (-)bool

    // don't forget isCaller()
    opChildCall,        // [State*] -var -var ... +var
    opSiblingCall,      // [State*] -var -var ... +var
    opMethodCall,       // [State*] -var -var -obj ... +var

    // Misc. builtins
    opLineNum,          // [linenum:int]
    opAssert,           // [cond:str] -bool
    opDump,             // [expr:str, type:Type*] -var

    opInv,
    opMaxCode = opInv,
};


inline bool isUndoableLoader(OpCode op)
    { return (op >= opLoadTypeRef && op <= opLoadStkVar); }

inline bool isGroundedLoader(OpCode op)
    { return op >= opLoadSelfVar && op <= opDeref; }

inline bool isCmpOp(OpCode op)
    { return op >= opEqual && op <= opGreaterEq; }

inline bool isJump(OpCode op)
    { return op >= opJump && op <= opJumpOr; }

inline bool isBoolJump(OpCode op)
    { return op >= opJumpFalse && op <= opJumpOr; }

inline bool isCaller(OpCode op)
    { return op >= opChildCall && op <= opSiblingCall; }


// --- OpCode Info


enum ArgType
    { argNone, argType, argState, argUInt8, argInt, argStr, 
      argVarType8, argDefinition,
      argSelfIdx, argOuterIdx, argStkIdx, argStateIdx, 
      argJump16, argLineNum, argAssertCond, argDump,
      argMax };


extern umemint ArgSizes[argMax];


struct OpInfo
{
    const char* name;
    OpCode op;
    ArgType arg;
};


extern OpInfo opTable[];


// --- Code segment -------------------------------------------------------- //


#define DEFAULT_STACK_SIZE 8192


class CodeSeg: public object
{
    friend class CodeGen;
    typedef rtobject parent;

    State* state;
    str code;

#ifdef DEBUG
    bool closed;
#endif

protected:
    memint stackSize;

    // Code gen helpers
    template <class T>
        void append(const T& t)         { code.append((const char*)&t, sizeof(T)); }
    void append(const str& s)           { code.append(s); }
    void erase(memint from)             { code.resize(from); }
    void eraseOp(memint offs)           { code.erase(offs, oplen((*this)[offs])); }
    str cutOp(memint offs);
    template<class T>
        T at(memint i) const            { return *(T*)code.data(i); }
    template<class T>
        T& atw(memint i)                { return *(T*)code.atw(i); }
    OpCode operator[](memint i) const   { return OpCode(uchar(code.at(i))); }
    void replaceOp(memint i, OpCode op)   { *code.atw<uchar>(i) = op; }

    static inline memint oplen(OpCode op)
        { assert(op < opInv); return memint(ArgSizes[opTable[op].arg]) + 1; }

public:
    CodeSeg(State*);
    ~CodeSeg();

    State* getStateType() const         { return state; }
    memint size() const                 { return code.size(); }
    bool empty() const                  { return code.empty(); }
    void close();

    const uchar* getCode() const        { assert(closed); return (uchar*)code.data(); }
    void dump(fifo& stm) const;  // in vminfo.cpp
};


template<> inline void CodeSeg::append<OpCode>(const OpCode& op)
    { append<uchar>(uchar(op)); }

// Compiler traps, don't use these
template<> OpCode CodeSeg::at<OpCode>(memint i) const;
template<> OpCode& CodeSeg::atw<OpCode>(memint i);


inline CodeSeg* State::getCodeSeg() const { return cast<CodeSeg*>(codeseg.get()); }
inline const uchar* State::getCode() const { return getCodeSeg()->getCode(); }


// --- Code Generator ------------------------------------------------------ //


class CodeGen: noncopyable
{
protected:
    Module* const module;
    State* const codeOwner;
    State* const typeReg;  // for calling registerType()
    CodeSeg& codeseg;

    // TODO: keep at least ordinal consts so that some things can be evaluated
    // at compile time or optimized
    struct SimStackItem
    {
        Type* type;
        memint offs;
        SimStackItem(Type* t, memint o)
            : type(t), offs(o)  { }
    };

    podvec<SimStackItem> simStack;  // exec simulation stack
    memint locals;                  // number of local vars allocated
    OpCode lastOp;

    template <class T>
        void add(const T& t)                        { codeseg.append<T>(t); }
    void addOp(OpCode op)                           { codeseg.append<uchar>(lastOp = op); }
    void addOp(Type*, OpCode op);
    template <class T>
        void addOp(OpCode op, const T& a)           { addOp(op); add<T>(a); }
    template <class T>
        void addOp(Type* t, OpCode op, const T& a)  { addOp(t, op); add<T>(a); }
    Type* stkPop();
    void stkReplaceTop(Type* t);  // only if the opcode is not changed
    Type* stkTop()
        { return simStack.back().type; }
    Type* stkTop(memint i)
        { return simStack.back(i).type; }
    memint stkTopOffs()
        { return simStack.back().offs; }
    static void error(const char*);
    static void error(const str&);

    memint prevLoaderOffs;

public:
    CodeGen(CodeSeg&, Module* m, State* treg, bool compileTime);
    ~CodeGen();

    memint getStackLevel()      { return simStack.size(); }
    bool isCompileTime()        { return codeOwner == NULL; }
    memint getLocals()          { return locals; }
    State* getState()           { return codeOwner; }
    Type* getTopType()          { return stkTop(); }
    void justForget()           { stkPop(); } 
    memint getCurrentOffs()     { return codeseg.size(); }
    Type* tryUndoTypeRef();
    void undoExpr(memint from);
    void undoLoader();
    bool lastWasFuncCall();
    void deinitLocalVar(Variable*);
    void deinitFrame(memint baseLevel); // doesn't change the sim stack
    void popValue();
    bool tryImplicitCast(Type*);
    void implicitCast(Type*, const char* errmsg = NULL);
    void explicitCast(Type*);
    void isType(Type*, memint undoOffs);
    void mkRange();

    memint prolog();
    void epilog(memint prologOffs);

    bool deref();
    void mkref();
    void nonEmpty();
    void loadTypeRef(Type*);
    void loadConst(Type* type, const variant&);
    void loadDefinition(Definition*);
    void loadEmptyConst(Type* type);
    void loadSymbol(Symbol*);
    void loadLocalVar(LocalVar*);
    void loadVariable(Variable*);
    void loadMember(const str& ident, memint* undoOffs);
    void loadMember(Symbol* sym, memint* undoOffs);
    void loadMember(Variable*);
    void loadThis();
    void loadDataSeg();

    void initLocalVar(LocalVar*);
    void initSelfVar(SelfVar*);
    void incLocalVar(LocalVar*);

    Container* elemToVec(Container*);
    void elemCat();
    void cat();
    void loadContainerElem();
    void loadKeyByIndex();
    void loadDictElemByIndex();
    void loadSubvec();
    void length();
    Container* elemToSet();
    Container* rangeToSet();
    void setAddElem();
    void checkRangeLeft();
    void setAddRange();
    Container* pairToDict();
    void checkDictKey();
    void dictAddPair();
    void inCont();
    void inBounds();
    void inRange();
    void inRange2(bool isCaseLabel = false);

    void arithmBinary(OpCode op);
    void arithmUnary(OpCode op);
    void cmp(OpCode);
    void caseCmp();
    void caseInRange()
        { inRange2(true); }
    void _not(); // 'not' is something reserved, probably only with Apple's GCC

    void localVarCmp(LocalVar*, OpCode);
    void localVarCmpLength(LocalVar* var, LocalVar* vec);

    void boolJump(memint target, OpCode op);
    memint boolJumpForward(OpCode op);
    memint jumpForward(OpCode = opJump);
    void resolveJump(memint target);
    void _jump(memint target, OpCode op = opJump);
    void jump(memint target)
        { _jump(target, opJump); }
    void linenum(integer);
    void assertion(const str& cond);
    void dumpVar(const str& expr);
    void programExit();

    str lvalue();
    str arithmLvalue(Token);
    void catLvalue();
    str insLvalue();
    void assignment(const str& storerCode);
    void deleteContainerElem();
    void catAssign();
    void fifoPush();

    State* mkFuncPtr();
    void call(FuncPtr*);

    void end();
    Type* runConstExpr(variant& result); // defined in vm.cpp
};


struct evoidfunc: public exception
{
    evoidfunc() throw();
    ~evoidfunc() throw();
    const char* what() throw();
};


// --- Execution context --------------------------------------------------- //


struct CompilerOptions
{
    bool enableDump;
    bool enableAssert;
    bool lineNumbers;
    bool vmListing;
    bool compileOnly;
    memint stackSize;
    strvec modulePath;

    CompilerOptions();
    void setDebugOpts(bool);
};


class Context;

class ModuleInstance: public Symbol
{
public:
    objptr<Module> module;
    objptr<stateobj> obj;
    ModuleInstance(Module* m);
    void run(Context*, rtstack&);
    void finalize();
};


class Context: protected Scope
{
protected:
    objvec<ModuleInstance> instances;
    ModuleInstance* queenBeeInst;

    ModuleInstance* addModule(Module*);
    str lookupSource(const str& modName);
    void instantiateModules();
    void clear();
    void dump(const str& listingPath);

public:
    CompilerOptions options;

    Context();
    ~Context();

    Module* getModule(const str& name);     // for use by the compiler, "uses" clause
    stateobj* getModuleObject(Module*);     // for initializing module vars in ModuleInstance::run()
    Module* loadModule(const str& filePath);
    variant execute();                      // after compilation only (loadModule())
};


// The Virtual Machine. This routine is used for both evaluating const
// expressions at compile time and, obviously, running runtime code. It is
// reenterant and can be launched concurrently in one process as long as
// the arguments are thread safe. It doesn't use any global/static data.
// Besides, code segments never have any relocatble data elements, so that any
// module can be reused in the multithreaded server environment too.

void runRabbitRun(stateobj* dataseg, variant* outer, variant* bp, const uchar* code);


struct eexit: public exception
{
    variant result;
    eexit(const variant&) throw();
    ~eexit() throw();
    const char* what() throw();
};


void initVm();
void doneVm();


#endif // __VM_H
