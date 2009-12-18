#ifndef __TYPESYS_H
#define __TYPESYS_H

#include "runtime.h"


class Symbol;
class Variable;
class Definition;
class Scope;
class Type;
class Reference;
class Ordinal;
class Enumeration;
class Container;
class Fifo;
class Module;
class QueenBee;

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
typedef Module* PModule;


class CodeSeg; // defined in vm.h
class CodeGen; // defined in vm.h


// --- Symbols & Scope ----------------------------------------------------- //


class Symbol: public symbol
{
public:
    enum SymbolId { RESULTVAR, LOCALVAR, SELFVAR, ARGVAR, // in sync with loaders and storers
                    DEFINITION,
                    FIRSTVAR = RESULTVAR };

    SymbolId const symbolId;
    Type* const type;

    Symbol(const str&, SymbolId, Type*) throw();
    ~Symbol() throw();

    bool isVariable() const     { return symbolId <= ARGVAR; }
    bool isDefinition() const   { return symbolId == DEFINITION; }
    bool isTypeAlias() const;
    bool isSelfVar() const      { return symbolId == SELFVAR; }
    bool isResultVar() const    { return symbolId == RESULTVAR; }
    bool isLocalVar() const     { return symbolId == LOCALVAR; }
    bool isArgVar() const       { return symbolId == ARGVAR; }
};


class Definition: public Symbol
{
public:
    variant const value;
    Definition(const str&, Type*, const variant&) throw();
    ~Definition() throw();
    Type* getAliasedType() const;
};


class Variable: public Symbol
{
public:
    memint const id;
    State* const state;
    Variable(const str&, SymbolId, Type*, memint, State*) throw();
    ~Variable() throw();
};


struct EDuplicate: public exception
{
    str const ident;
    EDuplicate(const str& _ident) throw();
    ~EDuplicate() throw();
    const char* what() const throw(); // shouldn't be called
};


struct EUnknownIdent: public exception
{
    str const ident;
    EUnknownIdent(const str& _ident) throw();
    ~EUnknownIdent() throw();
    const char* what() const throw(); // shouldn't be called
};


class Scope
{
    friend void test_typesys();
protected:
    symtbl symbols;         // symbol table for search
    Symbol* find(const str&) const;
    void addUnique(Symbol* s);
public:
    Scope* const outer;
    Scope(Scope* _outer);
    virtual ~Scope();
    Symbol* findShallow(const str& _name) const;
    virtual Symbol* findDeep(const str&) const;
};


class BlockScope: public Scope
{
protected:
    objvec<Variable> localVars;      // owned
    memint startId;
    CodeGen* gen;
public:
    BlockScope(Scope* outer, CodeGen*);
    ~BlockScope();
    Variable* addLocalVar(const str&, Type*);
    void deinitLocals();
};


// --- Type ---------------------------------------------------------------- //

// Note: type objects (all descendants of Type) should not be modified once
// created. This will allow to reuse loaded modules in a multi-threaded server
// environment for serving concurrent requests without actually re-compiling
// or reloading used modules.


class Type: public rtobject
{
public:
    enum TypeId {
        TYPEREF, NONE, VARIANT, REF,
        BOOL, CHAR, INT, ENUM,
        NULLCONT, VEC, SET, DICT,
        FIFO, FUNC, PROC, CLASS, MODULE };

protected:
    objptr<Reference> refType;
    str alias;      // for more readable diagnostics output, but not really needed

    Type(TypeId) throw();
    bool empty() const;
    static TypeId contType(Type* i, Type* e);

public:
    TypeId const typeId;

    ~Type() throw();
    void setAlias(const str& s) { if (alias.empty()) alias = s; }
    str getAlias() const        { return alias; }
    Reference* getRefType()     { return refType; }

    bool isTypeRef() const      { return typeId == TYPEREF; }
    bool isNone() const         { return typeId == NONE; }
    bool isVariant() const      { return typeId == VARIANT; }
    bool isReference() const    { return typeId == REF; }

    bool isBool() const         { return typeId == BOOL; }
    bool isChar() const         { return typeId == CHAR; }
    bool isInt() const          { return typeId == INT; }
    bool isEnum() const         { return typeId == ENUM || isBool(); }
    bool isAnyOrd() const       { return typeId >= BOOL && typeId <= ENUM; }
    bool isSmallOrd() const;
    bool isBitOrd() const;

    bool isNullCont() const     { return typeId == NULLCONT; }
    bool isVec() const          { return typeId == VEC; }
    bool isSet() const          { return typeId == SET; }
    bool isDict() const         { return typeId == DICT; }
    bool isAnyCont() const      { return typeId >= NULLCONT && typeId <= DICT; }

    bool isFifo() const         { return typeId == FIFO; }

    bool isFunction() const     { return typeId == FUNC; }
    bool isProc() const         { return typeId == PROC; }
    bool isClass() const        { return typeId == CLASS; }
    bool isModule() const       { return typeId == MODULE; }
    bool isAnyState() const     { return typeId >= FUNC && typeId <= MODULE; }

    virtual str definition(const str& ident) const;
    virtual bool identicalTo(Type*) const;
    virtual bool canAssignTo(Type*) const;

    Container* deriveVec();
    Container* deriveSet();
    Container* deriveDict(Type* elem);
    Fifo* deriveFifo();
};


void typeMismatch();


// --- General Types ------------------------------------------------------- //


class TypeReference: public Type
{
    friend void initTypeSys();
    friend class QueenBee;
protected:
    TypeReference() throw();
    ~TypeReference() throw();
};


class None: public Type
{
    friend void initTypeSys();
    friend class QueenBee;
protected:
    None() throw();
    ~None() throw();
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
    Reference(Type* _to) throw();
public:
    Type* const to;
    ~Reference() throw();
    str definition(const str& ident) const;
    bool identicalTo(Type* t) const;
};


// --- Ordinals ------------------------------------------------------------ //


class Ordinal: public Type
{
    friend class QueenBee;
protected:
    Ordinal(TypeId, integer, integer) throw();
    ~Ordinal() throw();
    void reassignRight(integer r) // for enums during their definition
        { assert(r == right + 1); (integer&)right = r; }
    virtual Ordinal* _createSubrange(integer, integer);
public:
    integer const left;
    integer const right;
    str definition(const str& ident) const;
    bool identicalTo(Type* t) const;
    bool canAssignTo(Type*) const;
    bool isSmallOrd() const
        { return left >= 0 && right <= 255; }
    bool isBitOrd() const
        { return left == 0 && right == 1; }
    Ordinal* createSubrange(integer, integer);
};


class Enumeration: public Ordinal
{
    friend class QueenBee;
protected:
    typedef objvec<Definition> EnumValues;
    EnumValues values;
    Enumeration(TypeId _typeId) throw();            // built-in enums, e.g. bool
    Enumeration(const EnumValues&, integer, integer) throw();     // subrange
    Ordinal* _createSubrange(integer, integer);     // override
public:
    Enumeration() throw();                          // user-defined enums
    ~Enumeration() throw();
    str definition(const str& ident) const;
    bool identicalTo(Type* t) const;
    bool canAssignTo(Type*) const;
    void addValue(State*, const str&);
};


// --- Containers ---------------------------------------------------------- //


class Container: public Type
{
    friend class Type;
    friend class QueenBee;
protected:
    Container(Type* i, Type* e) throw();
public:
    Type* const index;
    Type* const elem;
    ~Container() throw();
    str definition(const str& ident) const;
    bool identicalTo(Type*) const;
    bool hasSmallIndex() const
        { return index->isSmallOrd(); }
    bool hasSmallElem() const
        { return elem->isSmallOrd(); }
};


// --- Fifo ---------------------------------------------------------------- //


class Fifo: public Type
{
    friend class Type;
    friend class QueenBee;
protected:
    Fifo(Type*) throw();
public:
    Type* const elem;
    ~Fifo() throw();
    bool identicalTo(Type*) const;
};


// --- State --------------------------------------------------------------- //


class State: public Type, public Scope
{
protected:
    objvec<Type> types;             // owned
    objvec<Definition> defs;        // owned
    objvec<Variable> selfVars;      // owned
    Type* _registerType(Type*);
public:
    State(TypeId, State* parent) throw();
    ~State() throw();
    memint selfVarCount()               { return selfVars.size(); } // TODO: plus inherited
    template <class T>
        T* registerType(T* t)           { return (T*)_registerType(t); }
    template <class T>
        T* registerType(objptr<T> t)    { return (T*)_registerType(t); }
    Definition* addDefinition(const str&, Type*, const variant&);
    Definition* addTypeAlias(const str&, Type*);
    Variable* addSelfVar(const str&, Type*);
    stateobj* newInstance();
};


class StateDef: public Definition
{
public:
    StateDef(State*, CodeSeg*) throw();
    ~StateDef() throw();
    State* getStateType() const { return cast<State*>(type); }
    CodeSeg* getCodeSeg() const;
};


// --- Module -------------------------------------------------------------- //


class Module: public State
{
protected:
    objvec<Module> uses;    // not owned
    vector<str> constStrings;
public:
    Module(const str& _name) throw();
    ~Module() throw();
    Symbol* findDeep(const str&) const; // override
    void addUses(Module*, CodeSeg*);
    void registerString(str&); // returns a previously registered string if found
};


class ModuleDef: public StateDef
{
protected:
    // The module type is owned by its definition, because unlike other types
    // it's not registered anywhere else (all other types are registered and 
    // owned by their enclosing states).
    objptr<Module> module;

    // Besides, because module is a static object, its instance is also held 
    // here. So ModuleDef is a definition and a variable at the same time.
    objptr<stateobj> instance;

public:
    ModuleDef(const str&) throw();          // creates default Module and CodeSeg objects
    ModuleDef(Module*, CodeSeg*) throw();   // for custom Module and CodeSeg objects
    ~ModuleDef() throw();
    Module* getModuleType() const { return module; }
};


// --- QueenBee ------------------------------------------------------------ //


class QueenBee: public Module
{
    friend void initTypeSys();
protected:
    QueenBee() throw();
    ~QueenBee() throw();
public:
    Variant* const defVariant;
    Ordinal* const defInt;
    Ordinal* const defChar;
    Enumeration* const defBool;
    Container* const defNullCont;
    Container* const defStr;
    Container* const defCharSet;
    Fifo* const defCharFifo;
};


// --- Globals ------------------------------------------------------------- //


void initTypeSys();
void doneTypeSys();

extern objptr<TypeReference> defTypeRef;
extern objptr<None> defNone;
extern objptr<QueenBee> queenBee;
extern objptr<ModuleDef> queenBeeDef;

#endif // __TYPESYS_H
