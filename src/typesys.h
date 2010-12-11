#ifndef __TYPESYS_H
#define __TYPESYS_H

#include "runtime.h"


class Symbol;
class Variable;
class InnerVar;
class StkVar;
class ArgVar;
class PtrVar;
class ResultVar;
class Definition;
class Builtin;
class Scope;
class BlockScope;
class Type;
class Reference;
class Ordinal;
class Enumeration;
class Range;
class Container;
class Fifo;
class SelfStub;
class State;
class FuncPtr;
class Module;

typedef Symbol* PSymbol;
typedef Variable* PVariable;
typedef InnerVar* PInnerVar;
typedef StkVar* PStkVar;
typedef ArgVar* PArgVar;
typedef PtrVar* PPtrVar;
typedef ResultVar* PResultVar;
typedef Definition* PDefinition;
typedef Builtin* PBuiltin;
typedef Scope* PScope;
typedef BlockScope* PBlockScope;
typedef Type* PType;
typedef Reference* PReference;
typedef Ordinal* POrdinal;
typedef Enumeration* PEnumeration;
typedef Range* PRange;
typedef Container* PContainer;
typedef Fifo* PFifo;
typedef State* PState;
typedef FuncPtr* PFuncPtr;
typedef Module* PModule;


class CodeSeg; // defined in vm.h
class CodeGen;

class Compiler; // defined in compiler.h


// --- Symbols & Scope ----------------------------------------------------- //


class Symbol: public symbol
{
public:
    enum SymbolId { STKVAR, ARGVAR, PTRVAR, RESULTVAR, INNERVAR,
        FORMALARG, DEFINITION, BUILTIN };

    SymbolId const symbolId;
    Type* const type;
    State* const host;

    Symbol(const str&, SymbolId, Type*, State*) throw();
    ~Symbol() throw();

    void fqName(fifo&) const;
    void dump(fifo&) const;

    bool isAnyVar() const           { return symbolId <= INNERVAR; }
    bool isStkVar() const           { return symbolId == STKVAR; }
    bool isArgVar() const           { return symbolId == ARGVAR; }
    bool isPtrVar() const           { return symbolId == PTRVAR; }
    bool isResultVar() const        { return symbolId == RESULTVAR; }
    bool isInnerVar() const         { return symbolId == INNERVAR; }
    bool isFormalArg() const        { return symbolId == FORMALARG; }
    bool isDef() const              { return symbolId == DEFINITION; }
    bool isBuiltin() const          { return symbolId == BUILTIN; }
};


class Definition: public Symbol
{
public:
    variant const value;
    Definition(const str&, Type*, const variant&, State*) throw();
    ~Definition() throw();
    Type* getAliasedType() const;
};


class Variable: public Symbol
{
protected:
    Variable(const str&, SymbolId, Type*, memint, State*) throw();
public:
    memint const id;
    ~Variable() throw();
    memint getMemSize();
};


class StkVar: public Variable
{
public:
    StkVar(const str&, Type*, memint, State*) throw();
};


class ArgVar: public Variable
{
public:
    ArgVar(const str&, Type*, memint, State*) throw();
};


class PtrVar: public Variable
{
public:
    PtrVar(const str&, Type*, memint, State*) throw();
};


class ResultVar: public Variable
{
public:
    ResultVar(Type*, State*) throw();
};


class InnerVar: public Variable
{
public:
    InnerVar(const str&, Type*, memint, State*) throw();
    Module* getModuleType() const
        { return cast<Module*>(type); }
};


class FormalArg: public Symbol
{
public:
    memint const id;
    bool const isPtr;
    bool const hasDefValue;
    variant /*const*/ defValue;
    FormalArg(const str&, Type*, memint, bool isPtr, variant*) throw();
    ~FormalArg() throw();
    memint getMemSize()
        { return 1 + int(isPtr); }
};


class Builtin: public Symbol
{
public:
    typedef void (*CompileFunc)(Compiler*, Builtin*);

    CompileFunc const compile;   // either call the compiler function
    State* const staticFunc;     // ... or generate a static call to this function
    FuncPtr* const prototype;    // optional

    Builtin(const str&, CompileFunc, FuncPtr*, State*) throw();
    Builtin(const str&, CompileFunc, State*, State*) throw();
    ~Builtin() throw();
    void dump(fifo&) const;
};


struct EDuplicate: public exception
{
    str const ident;
    EDuplicate(const str& _ident) throw();
    ~EDuplicate() throw();
    const char* what() throw(); // shouldn't be called
};


struct EUnknownIdent: public exception
{
    str const ident;
    EUnknownIdent(const str& _ident) throw();
    ~EUnknownIdent() throw();
    const char* what() throw(); // shouldn't be called
};


class Scope
{
    friend void test_typesys();
protected:
    symtbl<Symbol> symbols;          // symbol table for search
public:
    Scope* const outer;
    Scope(Scope* _outer) throw()
        : outer(_outer) { }
    ~Scope() throw()  { }
    void addUnique(Symbol*);
    void replaceSymbol(Symbol*);
    Symbol* find(const str& ident) const            // returns NULL or Symbol
        { return symbols.find(ident); }
    Symbol* findShallow(const str& _name) const;    // throws EUnknown
};


class BlockScope: public Scope
{
protected:
    objvec<StkVar> stkVars;      // owned
    memint startId;
    memint varCount;
    CodeGen* gen;
public:
    BlockScope(Scope* outer, CodeGen*) throw();
    ~BlockScope() throw();
    StkVar* addStkVar(const str&, Type*);
    void deinitLocals();    // generates POPs via CodeGen (currently used only in AutoScope)
};


// --- Type ---------------------------------------------------------------- //

// Note: type objects (all descendants of Type) should not be modified once
// created. This will allow to reuse loaded modules in a multi-threaded server
// environment for serving concurrent requests without actually re-compiling
// or reloading used modules.

class Type: public rtobject
{
    friend class State;
    friend class Reference; // for access to dump()
public:

#if defined(BOOL) || defined(CHAR) || defined(INT)
#  error "I don't like your macro names and I'm not going to change mine."
#endif

    enum TypeId {
        TYPEREF, VOID, VARIANT, REF, RANGE,
        BOOL, CHAR, INT, ENUM,      // ordinal types; see isAnyOrd()
        NULLCONT, VEC, SET, DICT,   // containers; see isAnyCont()
        FIFO, SELFSTUB, FUNCPTR,
        STATE, MODULE               // note isAnyState()
    };

protected:
    objptr<Reference> refType;
    State* host;    // State that "owns" a given type
    str defName;    // for more readable diagnostics output, but not really needed

    Type(TypeId) throw();
    static TypeId contType(Type* i, Type* e) throw();
    // void setTypeId(TypeId id)
    //     { const_cast<TypeId&>(typeId) = id; }

public:
    TypeId const typeId;

    ~Type() throw();

    bool isTypeRef() const      { return typeId == TYPEREF; }
    bool isVoid() const         { return typeId == VOID; }
    bool isVariant() const      { return typeId == VARIANT; }
    bool isReference() const    { return typeId == REF; }
    bool isDerefable() const    { return !isAnyState() && !isAnyFifo(); }
    bool isRange() const        { return typeId == RANGE; }

    bool isBool() const         { return typeId == BOOL; }
    bool isChar() const         { return typeId == CHAR; }
    bool isInt() const          { return typeId == INT; }
    bool isEnum() const         { return typeId == ENUM || isBool(); }
    bool isAnyOrd() const       { return typeId >= BOOL && typeId <= ENUM; }
    bool isByte() const;
    bool isBit() const;
    bool isFullChar() const;

    bool isNullCont() const     { return typeId == NULLCONT; }
    bool isAnyVec() const       { return typeId == VEC; }
    bool isAnySet() const       { return typeId == SET; }
    bool isAnyDict() const      { return typeId == DICT; }
    bool isAnyCont() const      { return typeId >= NULLCONT && typeId <= DICT; }
    bool isByteVec() const;
    bool isByteSet() const;
    bool isByteDict() const;
    bool isContainer(Type* idx, Type* elem) const;
    bool isVectorOf(Type* elem) const;

    bool isAnyFifo() const      { return typeId == FIFO; }
    bool isByteFifo() const;
    bool isFifo(Type*) const;

    bool isSelfStub() const     { return typeId == SELFSTUB; }
    bool isFuncPtr() const      { return typeId == FUNCPTR; }
    bool isAnyState() const     { return typeId >= STATE; }
    bool isState() const        { return typeId == STATE; }
    bool isModule() const       { return typeId == MODULE; }

    bool isPod() const          { return isAnyOrd() || isVoid(); }
    memint getMemSize() const   { return 1; }

    bool empty() const;  // override
    void dump(fifo&) const;  // override
    void dumpDef(fifo&) const;
    virtual void dumpValue(fifo&, const variant&) const;

    // NOTE: the difference between identicalTo() and canAssignTo() is very subtle.
    // Identicality is mostly (but not only) tested for container compatibility,
    // when testing compatibility of function prototypes, etc. What can I say, 
    // "Careful with that axe, Eugene"
    virtual bool identicalTo(Type*) const;
    virtual bool canAssignTo(Type*) const;
    bool isCompatibleWith(const variant&);

    Reference* getRefType()     { return isReference() ? PReference(this) : refType.get(); }
    Type* getValueType();
    Container* deriveVec(State*);
    Container* deriveSet(State*);
    Container* deriveContainer(State*, Type* idxType);
    Fifo* deriveFifo(State*);
};


void dumpVariant(fifo&, const variant&, Type* = NULL);


inline memint Variable::getMemSize()
        { return type->getMemSize(); }



// --- General Types ------------------------------------------------------- //


class TypeReference: public Type
{
    friend void initTypeSys();
protected:
    TypeReference() throw();
    ~TypeReference() throw();
    void dumpValue(fifo&, const variant&) const;
};


class Void: public Type
{
    friend void initTypeSys();
protected:
    Void() throw();
    ~Void() throw();
};


class Variant: public Type
{
    friend class QueenBee;
protected:
    Variant() throw();
    ~Variant() throw();
};


class Reference: public Type
{
    friend class Type;
protected:
    Reference(Type*) throw();
public:
    Type* const to;
    ~Reference() throw();
    bool canAssignTo(Type*) const;
    bool identicalTo(Type* t) const;
    void dump(fifo&) const;
    void dumpValue(fifo&, const variant&) const;
};


inline Type* Type::getValueType()
    { return isReference() ? PReference(this)->to : this; }


// --- Ordinals ------------------------------------------------------------ //


class Ordinal: public Type
{
    friend class QueenBee;
protected:
    Ordinal(TypeId, integer, integer) throw();
    ~Ordinal() throw();
    void reassignRight(integer r)
        { assert(r == right + 1); (integer&)right = r; }
    virtual Ordinal* _createSubrange(integer, integer);

    objptr<Range> rangeType;

public:
    integer const left;
    integer const right;

    void dump(fifo&) const;
    void dumpValue(fifo&, const variant&) const;
    bool identicalTo(Type* t) const;
    bool canAssignTo(Type*) const;
    bool isInRange(integer v) const
        { return v >= left && v <= right; }
    bool isByte() const
        { return left >= 0 && right <= 255; }
    bool isBit() const
        { return left == 0 && right == 1; }
    bool isFullChar() const
        { return isChar() && left == 0 && right == 255; }
    integer getRange() const
        { return right - left + 1; }
    Ordinal* createSubrange(integer, integer);
    Ordinal* createSubrange(const range& r)
        { return createSubrange(r.left(), r.right()); }
    Range* getRangeType()
        { return rangeType; }
};


class Enumeration: public Ordinal
{
    friend class QueenBee;
protected:
    typedef objvec<Definition> EnumValues;
    EnumValues values;
    Enumeration(TypeId _typeId);            // built-in enums, e.g. bool
    Enumeration(const EnumValues&, integer, integer);     // subrange
    Ordinal* _createSubrange(integer, integer);     // override
public:
    Enumeration() throw();                          // user-defined enums
    ~Enumeration() throw();
    void dump(fifo&) const;
    void dumpValue(fifo&, const variant&) const;
    bool identicalTo(Type* t) const;
    bool canAssignTo(Type*) const;
    void addValue(State*, Scope*, const str&);
};


class Range: public Type
{
    friend class Ordinal;
protected:
    Range(Ordinal*) throw();
    ~Range() throw();
public:
    Ordinal* const elem;
    void dump(fifo&) const;
    void dumpValue(fifo&, const variant&) const;
    bool identicalTo(Type* t) const;
    bool canAssignTo(Type*) const;
};


// --- Containers ---------------------------------------------------------- //


class Container: public Type
{
    friend class State;
    friend class QueenBee;

protected:
    Container(Type* i, Type* e) throw();

public:
    Type* const index;
    Type* const elem;

    ~Container() throw();
    void dump(fifo&) const;
    void dumpValue(fifo&, const variant&) const;
    bool identicalTo(Type*) const;
    bool hasByteIndex() const
        { return index->isByte(); }
    bool hasByteElem() const
        { return elem->isByte(); }
};


// --- Fifo ---------------------------------------------------------------- //


class Fifo: public Type
{
    friend class State;
    friend class QueenBee;
protected:
    Fifo(Type*) throw();
public:
    Type* const elem;
    ~Fifo() throw();
    void dump(fifo&) const;
    bool identicalTo(Type*) const;
    bool isByteFifo() const
        { return elem->isByte(); }
};


// --- Prototype/FuncPtr --------------------------------------------------- //


class FuncPtr: public Type
{
public:
    Type* returnType;
    objvec<FormalArg> formalArgs;          // owned
    memint popArgCount;
    bool const returns; // VM helper

    FuncPtr(Type* retType) throw();
    ~FuncPtr() throw();
    void dump(fifo&) const;
    bool identicalTo(Type*) const; // override
    bool identicalTo(FuncPtr* t) const;
    bool canAssignTo(Type*) const; // override
    bool canAssignTo(FuncPtr* t) const;
    FormalArg* addFormalArg(const str&, Type*, bool isPtr, variant*);
    void resolveSelfType(State*);
};


// --- SelfStub ------------------------------------------------------------ //


class SelfStub: public Type
{
    friend class QueenBee;
protected:
    SelfStub() throw();
    ~SelfStub() throw();
public:
    bool identicalTo(Type*) const;
    bool canAssignTo(Type*) const;
};


// --- State --------------------------------------------------------------- //


typedef void (*ExternFuncProto)(variant* result, stateobj* outerobj, variant args[]);

// "i" below is 1-based; arguments are numbered from right to left
#define SHN_ARG(i) (args-(i))


class State: public Type, public Scope
{
protected:
    Type* _registerType(Type*, Definition* = NULL) throw();
    void addTypeAlias(const str&, Type*);

    objvec<Type> types;             // owned
    objvec<Definition> defs;        // owned
    objvec<Variable> args;          // owned, copied from prototype

    void setup();
    InnerVar* addInnerVar(InnerVar*);
    static Module* getParentModule(State*) throw();

    // Compiler helpers:
    bool complete;
    int innerObjUsed;
    int outsideObjectsUsed;

public:
    objvec<InnerVar> innerVars;     // owned

    State* const parent;
    Module* const parentModule;
    FuncPtr* const prototype;
    objptr<ResultVar> resultVar;    // may be NULL

    objptr<object> codeseg;
    ExternFuncProto const externFunc;

    // VM helpers:
    memint varCount;
    bool isCtor;

    State(State* parent, FuncPtr*) throw();
    State(State* parent, FuncPtr*, ExternFuncProto) throw();
    ~State() throw();
    void fqName(fifo&) const;
    void dump(fifo&) const;
    void dumpAll(fifo&) const;

    bool isComplete() const
        { return complete; }
    void setComplete()
        { assert(!complete); complete = true; }
    bool isStatic() const
        { return isComplete() && outsideObjectsUsed == 0; }
    int isInnerObjUsed() const
        { assert(complete); return innerObjUsed; }
    bool isExternal() const
        { return externFunc != NULL; }
    void useInnerObj()
        { innerObjUsed++; }
    void useOutsideObject()
        { outsideObjectsUsed++; }

    Definition* addDefinition(const str&, Type*, const variant&, Scope*);
    Variable* addArgument(FormalArg*);
    void addResultVar(Type*);
    InnerVar* addInnerVar(const str&, Type*);
    InnerVar* reclaimArg(ArgVar*, Type*);
    virtual stateobj* newInstance();
    template <class T>
        T* registerType(T* t) throw()
            { return cast<T*>(_registerType(t)); }
    Container* getContainerType(Type* idx, Type* elem);
    Fifo* getFifoType(Type* elem);
    FuncPtr* registerProto(Type* ret);
    FuncPtr* registerProto(Type* ret, Type* arg1);
    FuncPtr* registerProto(Type* ret, Type* arg1, Type* arg2);
    CodeSeg* getCodeSeg() const;
    const uchar* getCodeStart() const;
    // TODO: identicalTo(), canAssignTo()
};


inline void FuncPtr::resolveSelfType(State* state)
    { returnType = state; }


inline stateobj::stateobj(State* t) throw()
        : rtobject(t)
#ifdef DEBUG
          , varcount(t->varCount)
#endif
        { }


// --- Module -------------------------------------------------------------- //


class Module: public State
{
protected:
    strvec constStrings;
    objvec<CodeSeg> codeSegs;   // for dumps
public:
    str const filePath;
    objvec<InnerVar> usedModuleVars; // used module instances are stored in static vars
    Module(const str& name, const str& filePath) throw();
    ~Module() throw();
    void dump(fifo&) const;
    str getName() const         { return defName; }
    void addUsedModule(Module*);
    InnerVar* findUsedModuleVar(Module*);
    void registerString(str&); // registers a string literal for use at run-time
    void registerCodeSeg(CodeSeg* c); // collected here for dumps
};


// --- QueenBee (system module) -------------------------------------------- //


class QueenBee: public Module
{
    typedef Module parent;
    friend void initTypeSys();
protected:
    symtbl<Builtin> builtinScope;
    objvec<Builtin> builtins;
    QueenBee();
    ~QueenBee() throw();
    stateobj* newInstance(); // override
    Builtin* addBuiltin(Builtin*);
    Builtin* addBuiltin(const str&, Builtin::CompileFunc, FuncPtr*);
    Builtin* addBuiltin(const str&, Builtin::CompileFunc, State*);
    State* registerState(FuncPtr* proto, ExternFuncProto);
public:
    Variant* const defVariant;
    Ordinal* const defInt;
    Ordinal* const defChar;
    Ordinal* const defByte;
    Enumeration* const defBool;
    Container* const defNullCont;
    Container* const defStr;
    Container* const defCharSet;
    Fifo* const defCharFifo;
    SelfStub* const defSelfStub;
    Variable* sioVar;
    Variable* serrVar;
    Variable* resultVar;
    Builtin* findBuiltin(const str& ident)  // returns Builtin* or NULL
        { return builtinScope.find(ident); }
};


// --- Globals ------------------------------------------------------------- //


void initTypeSys();
void doneTypeSys();

extern objptr<TypeReference> defTypeRef;
extern objptr<Void> defVoid;
extern objptr<QueenBee> queenBee;

#endif // __TYPESYS_H
