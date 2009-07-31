#ifndef __VM_H
#define __VM_H


#include "common.h"
#include "runtime.h"
#include "source.h"
#include "typesys.h"

#include <stack>
#include <set>

// Implementation is in codegen.cpp and vm.cpp

enum OpCode
{
    // NOTE: the order of many of these instructions in their groups is significant!

    opInv,  // to detect corrupt code segments
    opEnd,  // also return from function
    opNop,
    opExit, // throws eexit()
    
    // Const loaders
    opLoadNull,         // +null
    opLoadFalse,        // +false
    opLoadTrue,         // +true
    opLoadChar,         // [8] +char
    opLoad0,            // +0
    opLoad1,            // +1
    opLoadInt,          // [int] +int
    opLoadNullRange,    // [Range*] +range
    opLoadNullDict,     // [Dict*] +dict
    opLoadNullStr,      // +str
    opLoadNullVec,      // [Vector*] +vec
    opLoadNullArray,    // [Array*] +array
    opLoadNullOrdset,   // [Ordset*] +ordset
    opLoadNullSet,      // [Set*] +set
    opLoadNullComp,     // +null-obj
    opLoadConst,        // [const-index: 8] +var // compound values only
    opLoadConst2,       // [const-index: 16] +var // compound values only
    opLoadTypeRef,      // [Type*] +typeref

    opPop,              // -var
    opSwap,
    opDup,              // +var

    // Safe typecasts
    opToBool,           // -var, +bool
    opToStr,            // -var, +str
    opToType,           // [Type*] -var, +var
    opToTypeRef,        // -type, -var, +var
    opIsType,           // [Type*] -var, +bool
    opIsTypeRef,        // -type, -var, +bool

    // Arithmetic binary: -ord, -ord, +ord
    opAdd,              // -int, +int, +int
    opSub,              // -int, +int, +int
    opMul,              // -int, +int, +int
    opDiv,              // -int, +int, +int
    opMod,              // -int, +int, +int
    opBitAnd,           // -int, +int, +int
    opBitOr,            // -int, +int, +int
    opBitXor,           // -int, +int, +int
    opBitShl,           // -int, +int, +int
    opBitShr,           // -int, +int, +int

    // Boolean operations are performed with short evaluation using jumps,
    // except NOT and XOR
    opBoolXor,          // -bool, -bool, +bool
    
    // Arithmetic unary: -ord, +ord
    opNeg,              // -int, +int
    opBitNot,           // -int, +int
    opNot,              // -bool, +bool

    // Range operations (work for all ordinals)
    opMkRange,          // [Ordinal*] -right-int, -left-int, +range
    opInRange,          // -range, -int, +bool

    // Comparators
    opCmpOrd,           // -ord, -ord, +{-1,0,1}
    opCmpStr,           // -str, -str, +{-1,0,1}
    opCmpVar,           // -var, -var, +{0,1}

    // Compare the stack top with 0 and replace it with a bool value.
    // The order of these opcodes is in sync with tokens tokEqual..tokGreaterEq
    opEqual,            // -int, +bool
    opNotEq,            // -int, +bool
    opLessThan,         // -int, +bool
    opLessEq,           // -int, +bool
    opGreaterThan,      // -int, +bool
    opGreaterEq,        // -int, +bool

    // Initializers
    opInitRet,          // [ret-index] -var
    // opInitLocal,        // [stack-index: 8]
    opInitThis,         // [this-index: 8]

    // Loaders
    // NOTE: opLoadRet through opLoadArg are in sync with Symbol::symbolId
    opLoadRet,          // [ret-index] +var
    opLoadLocal,        // [stack-index: 8] +var
    opLoadThis,         // [this-index: 8] +var
    opLoadArg,          // [stack-neg-index: 8] +var
    opLoadStatic,       // [Module*, var-index: 8] +var
    opLoadMember,       // [var-index: 8] -obj, +val
    opLoadOuter,        // [level: 8, var-index: 8] +var

    // Container read operations
    opLoadDictElem,     // -key, -dict, +val
    opDictHas,          // -key, -dict, +bool
    opLoadStrElem,      // -index, -str, +char
    opLoadVecElem,      // -index, -vector, +val
    opLoadArrayElem,    // -index, -array, +val
    opOrdsetHas,        // -ord, -ordset, +bool
    opSetHas,           // -key, -ordset, +bool

    // Storers
    // NOTE: opStoreRet through opStoreArg are in sync with Symbol::symbolId
    opStoreRet,         // [ret-index] -var
    opStoreLocal,       // [stack-index: 8] -var
    opStoreThis,        // [this-index: 8] -var
    opStoreArg,         // [stack-neg-index: 8] -var
    opStoreStatic,      // [Module*, var-index: 8] -var
    opStoreMember,      // [var-index: 8] -val, -obj
    opStoreOuter,       // [level: 8, var-index: 8] -var

    // Container write operations
    opStoreDictElem,    // -val, -key, -dict
    opDelDictElem,      // -key, -dict
//    opStoreStrElem,     // -char, -index, -str
    opStoreVecElem,     // -val, -index, -vector
    opStoreArrayElem,   // -val, -index, -array
    opAddToOrdset,      // -ord, -ordset
    opDelOrdsetElem,    // -key, -ordset
    opAddToSet,         // -key, -set
    opDelSetElem,       // -key, -set

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
    opRangeDiff,        // -range, +int
    opRangeLow,         // -range, +ord
    opRangeHigh,        // -range, +ord

    // Jumps; [dst] is a relative 16-bit offset.
    opJump,             // [dst 16]
    opJumpTrue,         // [dst 16] -bool
    opJumpFalse,        // [dst 16] -bool
    // Short bool evaluation: pop if jump, leave it otherwise
    opJumpOr,           // [dst 16] (-)bool
    opJumpAnd,          // [dst 16] (-)bool

    // Case labels
    // TODO: these ops can sometimes be used with simple conditions (if a == 1...)
    opCaseInt,          // [int], +bool
    opCaseRange,        // [int, int], +bool
    opCaseStr,          // -str, +bool
    opCaseTypeRef,      // -typeref, +bool

    // Function call
    opCall,             // [Type*]

    // Helpers
    opEcho,             // -var
    opEchoSpace,
    opEchoLn,
    opLineNum,          // [file-id: 16, line-num: 16]
    opAssert,           // -bool

    opMaxCode,
    
    // Special values
    opLoadBase = opLoadRet, opStoreBase = opStoreRet,
    opCmpFirst = opEqual, opCmpLast = opGreaterEq,
};


inline bool isLoadOp(OpCode op)
    { return (op >= opLoadNull && op <= opLoadTypeRef)
        || (op >= opLoadRet && op <= opLoadOuter); }

inline bool isCmpOp(OpCode op)
    { return op >= opCmpFirst && op <= opCmpLast; }

inline bool isJump(OpCode op)
    { return op >= opJump && op <= opJumpAnd; }

inline bool isBoolJump(OpCode op)
    { return op >= opJumpTrue && op <= opJumpAnd; }


template<class T>
    inline T ADV(const uchar*& ip)
        { T t = *(T*)ip; ip += sizeof(T); return t; }


DEF_EXCEPTION(eexit, "exit called");


class ConstCode: public CodeSeg
{
public:
    ConstCode(): CodeSeg(NULL, NULL) { }
    void run(variant&);
};


// --- CODE GENERATOR ------------------------------------------------------ //

class BlockScope;

class CodeGen: noncopyable
{
    typedef std::map<str, mem> StringMap;
protected:

    struct stkinfo
    {
        Type* type;
        variant value;
        bool hasValue;
        stkinfo(Type* t): type(t), value(), hasValue(false) { }
        stkinfo(Type* t, const variant& v): type(t), value(v), hasValue(true) { }
    };

    CodeSeg* codeseg;
    State* state;
    mem lastOpOffs;
    StringMap stringMap;
    typedef std::vector<stkinfo> stkImpl;
    stkImpl genStack;
    
    mem stkMax;
    mem locals;
#ifdef DEBUG
    mem stkSize;
#endif

    mem addOp(OpCode);
    void addOpPtr(OpCode, void*);
    void add8(uint8_t i);
    void add16(uint16_t i);
    void addJumpOffs(joffs_t i);
    void addInt(integer i);
    void addPtr(void* p);
    void revertLastLoad();
    OpCode lastOp();
    void close();

    void stkPush(Type* t, const variant& v);
    void stkPush(Type* t);
    void stkPush(Constant* c)
            { stkPush(c->type, c->value); }
    const stkinfo& stkTop() const;
    const stkinfo& stkTop(mem) const;
    Type* stkTopType() const
            { return stkTop().type; }
    bool  stkTopIsValue() const
            { return stkTop().hasValue; }
    const variant& stkTopValue() const
            { return stkTop().value; }
    Type* stkTopType(mem i) const
            { return stkTop(i).type; }
    Type* stkPop();
    void stkReplace(Type*);

    void loadConstById(mem id);
    mem  loadCompoundConst(const variant&);
    void doStaticVar(ThisVar* var, OpCode);
    void loadStoreVar(Variable* var, bool load);
    void canAssign(Type* from, Type* to, const char* errmsg);
    bool tryImplicitCastTo(Type* to);
    void setOp(OpCode ordsOp, OpCode sOp);
    void dictOp(OpCode op);

public:
    CodeGen(CodeSeg*);
    ~CodeGen();

    mem getLocals() { return locals; }
    State* getState() { return state; }
    Type* getTopType() { return stkTopType(); }
    Type* getTopTypeRefValue();

    void end();
    void endConstExpr(Type*);

    void exit();
    void loadNone();
    void loadBool(bool b);
    void loadChar(uchar c);
    void loadInt(integer i);
    void loadStr(const str& s);
    void loadTypeRef(Type*);
    void loadConst(Type*, const variant&, bool asVariant = false);
    void loadDefinition(Definition* def)
            { loadConst(def->type, def->value); }
    void loadNullComp(Type* type);
    void loadSymbol(Symbol*);
    void discard();
    void swap();    // not used currently
    void dup();

    void initRetVal(Type*);
    void initLocalVar(Variable*);
    void deinitLocalVar(Variable*);
    void initThisVar(Variable*);
    
    void loadVar(Variable*);
    void storeVar(Variable*);
    void loadMember(const str& ident);
    void loadContainerElem();
    void storeContainerElem();
    void delDictElem()
            { dictOp(opDelDictElem); }
    void dictHas();
    void addToSet()
            { setOp(opAddToOrdset, opAddToSet); }
    void delSetElem()
            { setOp(opDelOrdsetElem, opDelSetElem); }
    void setHas();

    void implicitCastTo(Type*, const char* errmsg);
    void explicitCastTo(Type*, const char* errmsg);
    void toBool();
    void dynamicCast();
    void testType(Type*);
    void testType();
    void arithmBinary(OpCode);
    void arithmUnary(OpCode);
    void _not();
    void boolXor();
    void elemToVec();
    void elemCat();
    void cat();
    void mkRange();
    void inRange();
    void cmp(OpCode op);
    
    void empty();
    void count();
    void lowHigh(bool high);
    
    mem  getCurPos()
            { return codeseg->size(); }
    void discardCode(mem from);
    void genPop()  // pop a value off the generator's stack
            { stkPop(); }
    void jump(mem target);
    mem  jumpForward(OpCode op = opJump);
    mem  boolJumpForward(OpCode);
    void resolveJump(mem jumpOffs);
    void nop()  // my favorite
            { addOp(opNop); }
    void caseLabel(Type*, const variant&);
    
    void echo()
            { stkPop(); addOp(opEcho); }
    void echoSpace()
            { addOp(opEchoSpace); }
    void echoLn()
            { addOp(opEchoLn); }
    void assertion();
    void linenum(integer file, integer line);
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

