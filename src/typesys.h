#ifndef __TYPESYS_H
#define __TYPESYS_H

#include "runtime.h"


class Symbol;
class Variable;
class InnerVar;
class StkVar;
class ArgVar;
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
    enum SymbolId { STKVAR, ARGVAR, RESULTVAR, INNERVAR, FORMALARG, DEFINITION, BUILTIN };

    SymbolId const symbolId;
    Type* const type;
    State* const host;

    Symbol(const str&, SymbolId, Type*, State*);
    ~Symbol();

    void fqName(fifo&) const;
    void dump(fifo&) const;

    bool isAnyVar() const           { return symbolId <= INNERVAR; }
    bool isStkVar() const           { return symbolId == STKVAR; }
    bool isArgVar() const           { return symbolId == ARGVAR; }
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
    Definition(const str&, Type*, const variant&, State*);
    ~Definition();
    Type* getAliasedType() const;
};


class Variable: public Symbol
{
protected:
    Variable(const str&, SymbolId, Type*, memint, State*);
public:
    memint const id;
    ~Variable();
};


class StkVar: public Variable
{
public:
    StkVar(const str&, Type*, memint, State*);
};


class ArgVar: public Variable
{
public:
    ArgVar(const str&, Type*, memint, State*);
};


class ResultVar: public Variable
{
public:
    ResultVar(Type*, State*);
};


class InnerVar: public Variable
{
public:
    InnerVar(const str&, Type*, memint, State*);
    Module* getModuleType() const
        { return cast<Module*>(type); }
};


class FormalArg: public Symbol
{
public:
    FormalArg(const str&, Type*);
};


class Builtin: public Symbol
{
public:
    typedef void (*CompileFunc)(Compiler*, Builtin*);

    CompileFunc const compileFunc;
    FuncPtr* const prototype; // optional

    Builtin(const str&, CompileFunc, FuncPtr*, State*);
    ~Builtin();
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
    bool const local;
    symtbl<Symbol> symbols;          // symbol table for search
public:
    Scope* const outer;
    Scope(bool local, Scope* outer);
    ~Scope();
    bool isLocal() const        { return local; }
    bool isStateScope() const   { return !local; }
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
    CodeGen* gen;
public:
    BlockScope(Scope* outer, CodeGen*);
    ~BlockScope();
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
        FIFO,
        SELFSTUB, STATE, FUNCPTR };

protected:
    objptr<Reference> refType;
    State* host;    // State that "owns" a given type
    str defName;    // for more readable diagnostics output, but not really needed

    Type(TypeId);
    static TypeId contType(Type* i, Type* e);

public:
    TypeId const typeId;

    ~Type();

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
    bool isCharFifo() const;
    bool isFifo(Type*) const;

    bool isSelfStub() const     { return typeId == SELFSTUB; }
    bool isAnyState() const     { return typeId == STATE; }
    bool isState() const        { return typeId == STATE; }
    bool isFuncPtr() const      { return typeId == FUNCPTR; }

    bool isPod() const          { return isAnyOrd() || isVoid(); }

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



// --- General Types ------------------------------------------------------- //


class TypeReference: public Type
{
    friend void initTypeSys();
protected:
    TypeReference();
    ~TypeReference();
    void dumpValue(fifo&, const variant&) const;
};


class Void: public Type
{
    friend void initTypeSys();
protected:
    Void();
    ~Void();
};


class Variant: public Type
{
    friend class QueenBee;
protected:
    Variant();
    ~Variant();
};


class Reference: public Type
{
    friend class Type;
protected:
    Reference(Type*);
public:
    Type* const to;
    ~Reference();
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
    Ordinal(TypeId, integer, integer);
    ~Ordinal();
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
    Enumeration();                          // user-defined enums
    ~Enumeration();
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
    Range(Ordinal*);
    ~Range();
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
    Container(Type* i, Type* e);

public:
    Type* const index;
    Type* const elem;

    ~Container();
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
    Fifo(Type*);
public:
    Type* const elem;
    ~Fifo();
    void dump(fifo&) const;
    bool identicalTo(Type*) const;
};


// --- Prototype/FuncPtr --------------------------------------------------- //


class FuncPtr: public Type
{
public:
    Type* returnType;
    objvec<FormalArg> formalArgs;          // owned

    FuncPtr(Type* retType);
    ~FuncPtr();
    void dump(fifo&) const;
    bool identicalTo(Type*) const; // override
    bool identicalTo(FuncPtr* t) const;
    bool canAssignTo(Type*) const; // override
    bool canAssignTo(FuncPtr* t) const;
    FormalArg* addFormalArg(const str&, Type*);
    void resolveSelfType(State*);
    int getPopArgs() const
        { return formalArgs.size(); }
    bool isVoidFunc() const
        { return returnType->isVoid(); }
};


// --- SelfStub ------------------------------------------------------------ //


class SelfStub: public Type
{
    friend class QueenBee;
protected:
    SelfStub();
    ~SelfStub();
public:
    bool identicalTo(Type*) const;
    bool canAssignTo(Type*) const;
};


// --- State --------------------------------------------------------------- //


typedef void (*ExternFuncProto)(variant* result, stateobj* outerobj, variant args[]);

// "i" below is 1-based; arguments are numbered from right to left
#define EXTERN_ARG(i) (args-(i))


class State: public Type, public Scope
{
protected:
    Type* _registerType(Type*, Definition* = NULL);
    void addTypeAlias(const str&, Type*);

    objvec<Type> types;             // owned
    objvec<Definition> defs;        // owned
    objvec<ArgVar> args;            // owned, copied from prototype

    InnerVar* addInnerVar(InnerVar*);
    static Module* getParentModule(State*);

    // Compiler helpers:
    bool complete;
    int innerObjUsed;
    int outsideObjectsUsed;

public:
    objvec<InnerVar> innerVars;     // owned

    State* const parent;
    Module* const parentModule;
    FuncPtr* const prototype;
    objptr<ResultVar> resultVar;              // may be NULL

    // Code (for extern functions codeseg contains stub code, otherwise, just the main code)
    objptr<object> codeseg;
    ExternFuncProto externFunc;

    // VM helpers:
    memint popArgCount;
    bool returns;
    memint varCount;

    State(State* parent, FuncPtr*);
    ~State();
    void fqName(fifo&) const;
    void dump(fifo&) const;
    void dumpAll(fifo&) const;

    bool isComplete() const
        { return complete; }
    void setComplete()
        { assert(!complete); complete = true; }
    bool isConstructor() const
        { return prototype->returnType->isSelfStub() || prototype->returnType == this; }
    bool isStatic() const
        { return isComplete() && outsideObjectsUsed == 0; }
    bool innerObjUsedSoFar() const
        { return innerObjUsed; }
    bool isExternal() const
        { return externFunc != NULL; }
    void useInnerObj()
        { innerObjUsed++; }
    void useOutsideObject()
        { outsideObjectsUsed++; }

    Definition* addDefinition(const str&, Type*, const variant&, Scope*);
    ArgVar* addArgument(const str&, Type*, memint);
    void addResultVar(Type*);
    InnerVar* addInnerVar(const str&, Type*);
    InnerVar* reclaimArg(ArgVar*, Type*);
    virtual stateobj* newInstance();
    template <class T>
        T* registerType(T* t)
            { return cast<T*>(_registerType(t)); }
    Container* getContainerType(Type* idx, Type* elem);
    Fifo* getFifoType(Type* elem);
    CodeSeg* getCodeSeg() const;
    const uchar* getCodeStart() const;
    // TODO: identicalTo(), canAssignTo()
};


inline void FuncPtr::resolveSelfType(State* state)
    { returnType = state; }


inline stateobj::stateobj(State* t)
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
    objvec<Builtin> builtins;
public:
    str const filePath;
    objvec<InnerVar> usedModuleVars; // used module instances are stored in static vars
    Module(const str& name, const str& filePath);
    ~Module();
    void dump(fifo&) const;
    str getName() const         { return defName; }
    void addUsedModule(Module*);
    InnerVar* findUsedModuleVar(Module*);
    void registerString(str&); // registers a string literal for use at run-time
    void registerCodeSeg(CodeSeg* c); // collected here for dumps
    Builtin* addBuiltin(Builtin*);
    Builtin* addBuiltin(const str&, Builtin::CompileFunc, FuncPtr*);
};


// --- QueenBee (system module) -------------------------------------------- //


class QueenBee: public Module
{
    typedef Module parent;
    friend void initTypeSys();
protected:
    QueenBee();
    ~QueenBee();
    stateobj* newInstance(); // override
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
};


// --- Globals ------------------------------------------------------------- //


void initTypeSys();
void doneTypeSys();

extern objptr<TypeReference> defTypeRef;
extern objptr<Void> defVoid;
extern objptr<QueenBee> queenBee;

#endif // __TYPESYS_H
