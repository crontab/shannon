#ifndef __TYPESYS_H
#define __TYPESYS_H

#include "runtime.h"


class Symbol;
class Variable;
class SelfVar;
class LocalVar;
class Definition;
class Scope;
class Type;
class Reference;
class Ordinal;
class Enumeration;
class Container;
class Fifo;
class Prototype;
class State;
class Module;

typedef Symbol* PSymbol;
typedef Variable* PVariable;
typedef Definition* PDefinition;
typedef Scope* PScope;
typedef Type* PType;
typedef Reference* PReference;
typedef Ordinal* POrdinal;
typedef Enumeration* PEnumeration;
typedef Container* PContainer;
typedef Fifo* PFifo;
typedef Prototype* PPrototype;
typedef State* PState;
typedef Module* PModule;


class CodeSeg; // defined in vm.h
class CodeGen;


// --- Symbols & Scope ----------------------------------------------------- //


class Symbol: public symbol
{
public:
    enum SymbolId { LOCALVAR, SELFVAR, DEFINITION, MODULEINST };

    SymbolId const symbolId;
    Type* const type;
    State* const host;

    Symbol(const str&, SymbolId, Type*, State*);
    ~Symbol();

    void fqName(fifo&) const;
    void dump(fifo&) const;

    bool isAnyVar() const           { return symbolId <= SELFVAR; }
    bool isSelfVar() const          { return symbolId == SELFVAR; }
    bool isLocalVar() const         { return symbolId == LOCALVAR; }
    bool isDefinition() const       { return symbolId == DEFINITION; }
    bool isTypeAlias() const;
    bool isModuleInstance() const   { return symbolId == MODULEINST; }
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


class LocalVar: public Variable
{
public:
    LocalVar(const str&, Type*, memint, State*);
};


class SelfVar: public Variable
{
public:
    SelfVar(const str&, Type*, memint, State*);
    Module* getModuleType() const
        { return cast<Module*>(type); }
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


extern template class symtbl<Symbol>;


class Scope
{
    friend void test_typesys();
protected:
    symtbl<Symbol> symbols;         // symbol table for search
    void addUnique(Symbol* s);
public:
    Scope* const outer;
    Scope(Scope* _outer);
    ~Scope();
    Symbol* find(const str& ident) const            // returns NULL or Symbol
        { return symbols.find(ident); }
    Symbol* findShallow(const str& _name) const;    // throws EUnknown
};


class BlockScope: public Scope
{
protected:
    objvec<LocalVar> localVars;      // owned
    memint startId;
    CodeGen* gen;
public:
    BlockScope(Scope* outer, CodeGen*);
    ~BlockScope();
    LocalVar* addLocalVar(const str&, Type*);
    void deinitLocals();
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
    enum TypeId {
        TYPEREF, VOID, VARIANT, REF,
        BOOL, CHAR, INT, ENUM,
        NULLCONT, VEC, SET, DICT,
        FIFO, PROTOTYPE, FUNC, CLASS, MODULE };

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
    bool isDerefable() const    { return !isAnyState() & !isFifo(); }

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

    bool isFifo() const         { return typeId == FIFO; }

    bool isPrototype() const    { return typeId == PROTOTYPE; }
    bool isFunction() const     { return typeId == FUNC; }
    bool isClass() const        { return typeId == CLASS; }
    bool isModule() const       { return typeId == MODULE; }
    bool isAnyState() const     { return typeId >= FUNC && typeId <= MODULE; }

    bool empty() const;  // override
    void dump(fifo&) const;  // override
    void dumpDef(fifo&) const;
    virtual void dumpValue(fifo&, const variant&) const;
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
    Ordinal* createSubrange(integer, integer);
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
    void addValue(State*, const str&);
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
    friend class Type;
    friend class QueenBee;
protected:
    Fifo(Type*);
public:
    Type* const elem;
    ~Fifo();
    void dump(fifo&) const;
    bool identicalTo(Type*) const;
};


// --- Prototype ----------------------------------------------------------- //


class Prototype: public Type
{
protected:
    Type* returnType;
    // TODO: define class Argument
    objvec<Variable> args;          // owned
public:
    Prototype(Type* retType);
    ~Prototype();
    void dump(fifo&) const;
    memint argCount()                   { return args.size(); }
    memint retVarId()                   { return - argCount() - 1; }
    bool identicalTo(Type*) const; // override
    bool identicalTo(Prototype* t) const;
};


// --- State --------------------------------------------------------------- //


class State: public Type, public Scope
{
protected:
    Type* _registerType(Type*, Definition* = NULL);

public:
    objvec<Type> types;             // owned
    objvec<Definition> defs;        // owned
    objvec<SelfVar> selfVars;      // owned
    // Local vars are stored in Scope::localVars; arguments are in prototype->args

    State* const parent;
    State* const selfPtr;
    Prototype* const prototype;
    objptr<object> codeseg;

    State(TypeId, Prototype* proto, State* parent, State* self);
    ~State();
    void fqName(fifo&) const;
    Module* getParentModule() const;
    void dump(fifo&) const;
    void dumpAll(fifo&) const;
    memint selfVarCount() const     { return selfVars.size(); } // TODO: plus inherited
    // TODO: bool identicalTo(Type*) const;
    Definition* addDefinition(const str&, Type*, const variant&);
    Definition* addTypeAlias(const str&, Type*);
    SelfVar* addSelfVar(const str&, Type*);
    virtual stateobj* newInstance();
    template <class T>
        T* registerType(T* t)       { return cast<T*>(_registerType(t)); }
    Container* getContainerType(Type* idx, Type* elem);
    CodeSeg* getCodeSeg();
};


// --- Module -------------------------------------------------------------- //


class Module: public State
{
protected:
    strvec constStrings;
    objvec<CodeSeg> codeSegs;   // for dumps
    bool complete;
public:
    str const filePath;
    objvec<SelfVar> uses; // used module instances are stored in static vars
    Module(const str& name, const str& filePath);
    ~Module();
    void dump(fifo&) const;
    str getName() const         { return defName; }
    bool isComplete() const     { return complete; }
    void setComplete()          { complete = true; }
    void addUses(Module*);
    void registerString(str&); // registers a string literal for use at run-time
    void registerCodeSeg(CodeSeg* c);
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
    Variable* sioVar;
    Variable* serrVar;
    Variable* resultVar;
};


// --- Globals ------------------------------------------------------------- //


void initTypeSys();
void doneTypeSys();

extern objptr<TypeReference> defTypeRef;
extern objptr<Void> defVoid;
extern objptr<Prototype> defPrototype;
extern objptr<QueenBee> queenBee;

#endif // __TYPESYS_H
