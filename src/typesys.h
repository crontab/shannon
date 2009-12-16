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
typedef Module* PModule;


// --- Symbols & Scope ----------------------------------------------------- //


class Symbol: public symbol
{
public:
    enum SymbolId { RESULTVAR, LOCALVAR, THISVAR, ARGVAR, // in sync with loaders and storers
                    DEFINITION,
                    FIRSTVAR = RESULTVAR };

    SymbolId const symbolId;
    Type* const type;

    Symbol(const str&, SymbolId, Type*);
    ~Symbol();

    bool isVariable() const     { return symbolId <= ARGVAR; }
    bool isDefinition() const   { return symbolId == DEFINITION; }
    bool isTypeAlias() const;
    bool isThisVar() const      { return symbolId == THISVAR; }
    bool isResultVar() const    { return symbolId == RESULTVAR; }
    bool isLocalVar() const     { return symbolId == LOCALVAR; }
    bool isArgVar() const       { return symbolId == ARGVAR; }
};


class Definition: public Symbol
{
public:
    variant const value;
    Definition(const str&, Type*, const variant&);
    ~Definition();
    Type* aliasedType() const;
};


class Variable: public Symbol
{
public:
    memint const id;
    State* const state;
    Variable(SymbolId, Type*, const str&, memint, State*);
    ~Variable();
};


struct EDuplicate: public exception
{
    str const ident;
    EDuplicate(const str& _ident);
    ~EDuplicate();
    const char* what() const; // shouldn't be called
};


struct EUnknownIdent: public exception
{
    str const ident;
    EUnknownIdent(const str& _ident);
    ~EUnknownIdent();
    const char* what() const; // shouldn't be called
};


class Scope
{
    friend void test_typesys();
protected:
    symtbl symbols;         // symbol table for search
    objvec<Module> uses;
    Symbol* find(const str&) const;
    void addUnique(Symbol* s);
public:
    Scope* const outer;
    Scope(Scope* _outer);
    ~Scope();
    Symbol* findShallow(const str& _name) const;
    Symbol* findDeep(const str&) const;
};


// --- Type ---------------------------------------------------------------- //


class Type: public rtobject
{
    friend class Module;
    friend class State;

public:
    enum TypeId {
        TYPEREF, NONE, VARIANT, REF,
        BOOL, CHAR, INT, ENUM,
        NULLCONT, VEC, SET, DICT,
        FIFO, FUNC, PROC, OBJECT, MODULE };

protected:
    str alias;      // for more readable diagnostics output, but not really needed
    State* host;    // derivators are inserted into the hosts's repository

    Container* derivedVec;
    Container* derivedSet;
    dict<Type*, Container*> derivedDicts;

    Type(TypeId);
    bool empty() const;
    void setAlias(const str& s) { if (alias.empty()) alias = s; }
    void setHost(State* o)     { assert(host == NULL); host = o; }
    static TypeId contType(Type* i, Type* e);

public:
    TypeId const typeId;

    ~Type();

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
    bool isString() const;

    bool isFifo() const         { return typeId == FIFO; }
    bool isCharFifo() const;

    bool isFunction() const     { return typeId == FUNC; }
    bool isProc() const         { return typeId == PROC; }
    bool isObject() const       { return typeId == OBJECT; }
    bool isModule() const       { return typeId == MODULE; }
    bool isAnyState() const     { return typeId >= FUNC && typeId <= MODULE; }

    virtual str definition(const str& ident) const;
    virtual bool identicalTo(Type*) const;
    virtual bool canAssignTo(Type*) const;

    Container* deriveVec();
    Container* deriveSet();
    Container* deriveDict(Type* elemType);
};


void typeMismatch();


// --- General Types ------------------------------------------------------- //


class TypeReference: public Type
{
    friend void initTypeSys();
    friend class QueenBee;
protected:
    TypeReference();
    ~TypeReference();
};


class None: public Type
{
    friend void initTypeSys();
    friend class QueenBee;
protected:
    None();
    ~None();
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
public:
    Type* const to;
    Reference(Type* _to);
    ~Reference();
    str definition(const str& ident) const;
    bool identicalTo(Type* t) const;
    bool canAssignTo(Type* t) const;
};


// --- Ordinals ------------------------------------------------------------ //


class Ordinal: public Type
{
    friend class QueenBee;
protected:
    Ordinal(TypeId, integer, integer);
    ~Ordinal();
    void reassignRight(integer r) // for enums during their definition
        { assert(r == right + 1); (integer&)right = r; }
    virtual Ordinal* _createSubrange(integer, integer);
public:
    integer const left;
    integer const right;
    str definition(const str& ident) const;
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
    Enumeration(TypeId _typeId);                    // built-in enums, e.g. bool
    Enumeration(const EnumValues&, integer, integer);     // subrange
    Ordinal* _createSubrange(integer, integer);     // override
public:
    Enumeration();                                  // user-defined enums
    ~Enumeration();
    str definition(const str& ident) const;
    bool canAssignTo(Type*) const;
    void addValue(State*, const str&);
};


// --- Containers ---------------------------------------------------------- //


class Container: public Type
{
    friend class Type;
    friend class QueenBee;
protected:
    Container(Type* i, Type* e);
public:
    Type* const index;
    Type* const elem;
    ~Container();
    str definition(const str& ident) const;
    bool identicalTo(Type*) const;
};


// --- State --------------------------------------------------------------- //


class State: public Type, public Scope
{
    friend class CodeSeg;
    friend class CodeGen;
protected:
    objvec<Type> types;             // owned
    objvec<Definition> defs;        // owned
    objvec<Variable> selfVars;      // owned
    objptr<CodeSeg> code;
    Type* _registerType(Type*);
public:
    State(TypeId, State* parent);
    ~State();
    memint selfVarCount()               { return selfVars.size(); }
    template <class T>
        T* registerType(T* t)           { return (T*)_registerType(t); }
    template <class T>
        T* registerType(objptr<T> t)    { return (T*)_registerType(t); }
    Definition* addDefinition(const str&, Type*, const variant&);
    Definition* addTypeAlias(const str&, Type*);
};


// --- Module -------------------------------------------------------------- //


class Module: public State
{
protected:
    set<str> constStrings; // TODO: find duplicates?
public:
    Module(const str& _name);
    ~Module();
    void registerString(str&); // may return a previously registered string if found
};


// --- QueenBee ------------------------------------------------------------ //


class QueenBee: public Module
{
    friend void initTypeSys();
protected:
    QueenBee();
    ~QueenBee();
public:
    Variant* const defVariant;
    Ordinal* const defInt;
    Ordinal* const defChar;
    Enumeration* const defBool;
    Container* const defStr;
    Container* const defNullCont;
};


// --- Globals ------------------------------------------------------------- //


void initTypeSys();
void doneTypeSys();

extern objptr<TypeReference> defTypeRef;
extern objptr<None> defNone;
extern objptr<QueenBee> queenBee;

#endif // __TYPESYS_H
