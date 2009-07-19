#ifndef __VM_H
#define __VM_H


#include "common.h"
#include "runtime.h"
#include "typesys.h"

#include <stack>


enum OpCode
{
    opInv,  // to detect corrupt code segments
    opEnd,  // also return from function
    opNop,
    opExit, // throws eexit()
    
    // Const loaders
    opLoadNull,
    opLoadFalse,
    opLoadTrue,
    opLoadChar,         // [8]
    opLoad0,
    opLoad1,
    opLoadInt,          // [int]
    opLoadNullRange,    // [Range*]
    opLoadNullDict,     // [Dict*]
    opLoadNullStr,
    opLoadNullVec,      // [Vector*]
    opLoadNullArray,    // [Array*]
    opLoadNullOrdset,   // [Ordset*]
    opLoadNullSet,      // [Set*]
    opLoadConst,        // [const-index: 8] // compound values only
    opLoadConst2,       // [const-index: 16] // compound values only
    opLoadTypeRef,      // [Type*]
    opLoadDataseg,      // [module-index: 8] // used in unit tests, otherwise use opLoadStatic

    opPop,              // -var
    opSwap,
    opDup,              // +var

    // Safe typecasts
    opToBool,
    opToStr,
    opToType,           // [Type*] -var, +var
    opToTypeRef,        // -type, -var, +var
    opIsType,           // [Type*] -var, +bool
    opIsTypeRef,        // -type, -var, +bool

    // Arithmetic binary: -ord, -ord, +ord
    opAdd, opSub, opMul, opDiv, opMod, opBitAnd, opBitOr, opBitXor, opBitShl, opBitShr,
    // Arithmetic unary: -ord, +ord
    opNeg, opBitNot, opNot,

    // Range operations (work for all ordinals)
    opMkRange,          // [Ordinal*] -right-int, -left-int, +range
    opInRange,          // -range, -int, +{0,1}

    // Comparators
    opCmpOrd,           // -ord, -ord, +{-1,0,1}
    opCmpStr,           // -str, -str, +{-1,0,1}
    opCmpVar,           // -var, -var, +{0,1}

    // Compare the stack top with 0 and replace it with a bool value.
    // The order of these opcodes is in sync with tokens tokEqual..tokGreaterEq
    opEqual, opNotEq, opLessThan, opLessEq, opGreaterThan, opGreaterEq,

    // Initializers
    opInitRet,          // [ret-index] -var
    opInitLocal,        // [stack-index: 8]
    opInitThis,         // [this-index: 8]

    // Loaders: each of these can be replaced by a corresponding storer if
    // the object turns out to be a L-value.
    opLoadRet,          // [ret-index] +var
    opLoadLocal,        // [stack-index: 8] +var
    opLoadThis,         // [this-index: 8] +var
    opLoadArg,          // [stack-neg-index: 8] +var
    opLoadStatic,       // [module: 8, var-index: 8] +var
    opLoadMember,       // [var-index: 8] -obj, +val
    opLoadOuter,        // [level: 8, var-index: 8] +var

    // Container read operations
    opLoadDictElem,     // -key, -dict, +val
    opInDictKeys,       // -dict, -key, +bool
    opLoadStrElem,      // -index, -str, +char
    opLoadVecElem,      // -index, -vector, +val
    opLoadArrayElem,    // -index, -array, +val
    opInOrdset,         // -ordset, -ord, +bool
    opInSet,            // -ordset, -key, +bool

    // Storers
    opStoreRet,         // [ret-index] -var
    opStoreLocal,       // [stack-index: 8] -var
    opStoreThis,        // [this-index: 8] -var
    opStoreArg,         // [stack-neg-index: 8] -var
    opStoreStatic,      // [module: 8, var-index: 8] -var
    opStoreMember,      // [var-index: 8] -val, -obj
    opStoreOuter,       // [level: 8, var-index: 8] -var

    // Container write operations
    opStoreDictElem,    // -val, -key, -dict
    opDelDictElem,      // -key, -dict
    // TODO: implement a shared string class (not copy-on-write)
//    opStoreStrElem,     // -char, -index, -str
    opStoreVecElem,     // -val, -index, -vector
    opStoreArrayElem,   // -val, -index, -array
    opAddToOrdset,      // -ord, -ordset
    opAddToSet,         // -key, -set

    // Concatenation
    opCharToStr,        // -char, +str
    opCharCat,          // -char, -str, +str
    opStrCat,           // -str, -str, +str
    opVarToVec,         // [Vector*] -var, +vec
    opVarCat,           // -var, -vec, +vec
    opVecCat,           // -var, -vec, +vec

    // Misc. built-ins
    opEmpty,            // -var, +bool
    opStrLen,           // -str, +int
    opVecLen,           // -vec, +int
    opLow,              // -range, +ord
    opHigh,             // -range, +ord

    // Case labels: cmp the top element with the arg and leave 0 or 1 for
    // further boolean jump
    opCase,             // -var, +{0,1}
    opCaseRange,        // -int, +{0,1}

    // Jumps; [dst] is a relative offset -128..127
    //   short bool evaluation: pop if jump, leave it otherwise
    // TODO: 16-bit versions of these
    opJumpOr, opJumpAnd,                // [dst 8] (-)bool
    opJumpTrue, opJumpFalse, opJump,    // [dst 8]

    // Function call
    opCall,             // [Type*]

    // Helpers
    opEcho, opEchoLn,
    opAssert,           // [line-num: 16]
    opLinenum,          // [line-num: 16]

    opMaxCode
};


inline bool isLoadOp(OpCode op)
    { return (op >= opLoadNull && op <= opLoadTypeRef)
        || (op >= opLoadRet && op <= opLoadMember); }

inline bool isCmpOp(OpCode op)
    { return op >= opEqual && op <= opGreaterEq; }


DEF_EXCEPTION(eexit, "exit called");


class ConstCode: public CodeSeg
{
public:
    ConstCode(): CodeSeg(NULL, NULL) { }
    void run(variant&) const;
};


class Context: protected BaseTable<ModuleAlias>
{
    friend class CodeSeg;
protected:
    List<ModuleAlias> defs;
    List<Module> modules;
    List<langobj> datasegs;
    Module* registerModule(const str& name, Module*);   // for built-in modules
public:
    Context();
    ~Context();
    Module* addModule(const str& name);

    // Executation of the program starts here. The value of system.sresult is
    // returned. Can be called multiple times.
    variant run(varstack&);
};


// --- CODE GENERATOR ------------------------------------------------------ //

class BlockScope;

class CodeGen: noncopyable
{
protected:

    struct stkinfo
    {
        Type* type;
        stkinfo(Type* t): type(t) { }
    };

    CodeSeg& codeseg;
    mem lastOpOffs;

    mem addOp(OpCode);
    void addOpPtr(OpCode, void*);
    void add8(uint8_t i);
    void add16(uint16_t i);
    void addInt(integer i);
    void addPtr(void* p);
    bool revertLastLoad();
    void close();

    typedef std::vector<stkinfo> stkImpl;
    stkImpl genStack;
    mem stkMax;
    mem locals;
#ifdef DEBUG
    mem stkSize;
#endif

    void stkPush(Type* t, const variant& v);
    void stkPush(Type* t)
            { stkPush(t, null); }
    void stkPush(Constant* c)
            { stkPush(c->type, c->value); }
    const stkinfo& stkTop() const;
    const stkinfo& stkTop(mem) const;
    Type* stkTopType() const
            { return stkTop().type; }
    Type* stkTopType(mem i) const
            { return stkTop(i).type; }
    void stkReplace(Type*);
    Type* stkPop();

    void doStaticVar(ThisVar* var, OpCode);
    void typeCast(Type* from, Type* to, const char* errmsg);

public:
    CodeGen(CodeSeg&);
    ~CodeGen();

    mem getLocals() { return locals; }

    void end();
    void endConstExpr(Type*);

    void exit();
    void loadNone();
    void loadBool(bool b);
    void loadChar(uchar c);
    void loadInt(integer i);
    void loadStr(const str& s);
    void loadTypeRef(Type*);
    void loadNullContainer(Container*);
    void loadConst(Type*, const variant&, bool asVariant = false);
    void loadDataseg(Module*);
    void discard();
    void swap();    // not used currently
    void dup();

    void initRetVal(Type*);
    void initLocalVar(Variable*);
    void deinitLocalVar(Variable*);
    void initThisVar(Variable*);
    
    void loadVar(Variable*);
    void loadMember(ThisVar*);
    void storeVar(Variable*);
    void loadContainerElem();
    void storeContainerElem();
    void delDictElem();
    void addToSet();
    void inSet();
    void inDictKeys();

    void implicitCastTo(Type*, const char* errmsg);
    void explicitCastTo(Type*);
    void dynamicCast();
    void testType(Type*);
    void testType();
    void arithmBinary(OpCode);
    void arithmUnary(OpCode);
    void elemToVec();
    void elemCat();
    void cat();
    void mkRange();
    void inRange();
    void cmp(OpCode op);
};


class BlockScope: public Scope
{
protected:
    List<Variable> localvars;
    mem startId;
    CodeGen* gen;
public:
    BlockScope(Scope* outer, CodeGen*);
    ~BlockScope();
    Variable* addLocalVar(Type*, const str&);
    void deinitLocals();
};


#endif // __VM_H

