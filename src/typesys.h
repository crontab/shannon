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
class CodeGen; // defined in vm.h
class Context; // defined in vm.h


// --- Code segment -------------------------------------------------------- //

// Belongs to vm.h/vm.cpp but defined here because CodeSeg is part of State


#define DEFAULT_STACK_SIZE 8192


class CodeSeg: public rtobject
{
    friend class CodeGen;
    typedef rtobject parent;

    str code;

#ifdef DEBUG
    bool closed;
#endif

protected:
    memint stackSize;

    // Code gen helpers
    template <class T>
        void append(const T& t)     { code.append((const char*)&t, sizeof(T)); }
    void append(const str& s)       { code.append(s); }
    void resize(memint s)           { code.resize(s); }
    str  cutTail(memint start)
        { str t = code.substr(start); resize(start); return t; }
    template<class T>
        const T& at(memint i) const { return *(T*)code.data(i); }
    template<class T>
        T& atw(memint i)            { return *(T*)code.atw(i); }
    char operator[] (memint i) const { return code[i]; }

public:
    CodeSeg(State*);
    ~CodeSeg();

    State* getType() const          { return cast<State*>(parent::getType()); }
    memint size() const             { return code.size(); }
    bool empty() const              { return code.empty(); }
    void close();

    const char* getCode() const     { assert(closed); return code.data(); }
};


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

    bool isSelfVar() const          { return symbolId == SELFVAR; }
    bool isLocalVar() const         { return symbolId == LOCALVAR; }
    bool isVariable() const         { return symbolId <= SELFVAR; }
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
public:
    memint const id;
    Variable(const str&, SymbolId, Type*, memint, State*);
    ~Variable();
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


class Scope
{
    friend void test_typesys();
protected:
    symtbl<Symbol> symbols;         // symbol table for search
    void addUnique(Symbol* s);
public:
    Scope* const outer;
    Scope(Scope* _outer);
    virtual ~Scope();
    Symbol* find(const str& ident) const            // returns NULL or Symbol
        { return symbols.find(ident); }
    Symbol* findShallow(const str& _name) const;    // throws EUnknown
//    Symbol* findDeep(const str&) const;             // throws EUnknown
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
    friend class State;
    friend class Reference; // for access to _dump()
public:
    enum TypeId {
        TYPEREF, VOID, VARIANT, REF,
        BOOL, CHAR, INT, ENUM,
        NULLCONT, VEC, SET, DICT,
        FIFO, PROTOTYPE, FUNC, CLASS, MODULE };

protected:
    objptr<Reference> refType;
    State* host;      // State that "owns" a given type
    Definition* def;  // for more readable diagnostics output, but not really needed

    Type(TypeId);
    virtual void _dump(fifo&) const = 0;
    static TypeId contType(Type* i, Type* e);

public:
    TypeId const typeId;

    ~Type();

    bool isTypeRef() const      { return typeId == TYPEREF; }
    bool isNone() const         { return typeId == VOID; }
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
    bool isOrdVec() const;

    bool isFifo() const         { return typeId == FIFO; }

    bool isPrototype() const    { return typeId == PROTOTYPE; }
    bool isFunction() const     { return typeId == FUNC; }
    bool isClass() const        { return typeId == CLASS; }
    bool isModule() const       { return typeId == MODULE; }
    bool isAnyState() const     { return typeId >= FUNC && typeId <= MODULE; }

    void dump(fifo&) const;
    void dumpDefinition(fifo&) const;
    virtual bool identicalTo(Type*) const;
    virtual bool canAssignTo(Type*) const;

    Reference* getRefType()     { return isReference() ? PReference(this) : refType.get(); }
    Type* getValueType();
    Container* deriveVec();
    Container* deriveSet();
    Container* deriveContainer(Type* idxType);
    Fifo* deriveFifo();
};


// void typeMismatch();


// --- General Types ------------------------------------------------------- //


class TypeReference: public Type
{
    friend void initTypeSys();
    friend class QueenBee;
protected:
    TypeReference();
    ~TypeReference();
    void _dump(fifo& stm) const { Type::dump(stm); }
};


class Void: public Type
{
    friend void initTypeSys();
    friend class QueenBee;
protected:
    Void();
    ~Void();
    void _dump(fifo& stm) const { Type::dump(stm); }
};


class Variant: public Type
{
    friend class QueenBee;
protected:
    Variant();
    ~Variant();
    void _dump(fifo& stm) const { Type::dump(stm); }
};


class Reference: public Type
{
    friend class Type;
protected:
    Reference(Type* _to);
    void _dump(fifo&) const;
    Type* const to;
public:
    ~Reference();
    bool identicalTo(Type* t) const;
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
    void _dump(fifo&) const;
    void reassignRight(integer r)
        { assert(r == right + 1); (integer&)right = r; }
    virtual Ordinal* _createSubrange(integer, integer);
public:
    integer const left;
    integer const right;
    bool identicalTo(Type* t) const;
    bool canAssignTo(Type*) const;
    bool isInRange(integer v)
        { return v >= left && v <= right; }
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
    Enumeration(TypeId _typeId);            // built-in enums, e.g. bool
    Enumeration(const EnumValues&, integer, integer);     // subrange
    void _dump(fifo&) const;
    Ordinal* _createSubrange(integer, integer);     // override
public:
    Enumeration();                          // user-defined enums
    ~Enumeration();
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
    Container(Type* i, Type* e);
    void _dump(fifo&) const;
public:
    Type* const index;
    Type* const elem;
    ~Container();
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
    Fifo(Type*);
    void _dump(fifo&) const;
public:
    Type* const elem;
    ~Fifo();
    bool identicalTo(Type*) const;
};


// --- Prototype ----------------------------------------------------------- //


class Prototype: public Type
{
protected:
    Type* returnType;
    objvec<Variable> args;          // owned
    void _dump(fifo&) const;
public:
    Prototype(Type* retType);
    ~Prototype();
    memint argCount()                   { return args.size(); }
    memint retVarId()                   { return - argCount() - 1; }
    bool identicalTo(Type*) const; // override
    bool identicalTo(Prototype* t) const;
};


// --- State --------------------------------------------------------------- //


class State: public Type, public Scope
{
protected:
    objvec<Type> types;             // owned
    objvec<Definition> defs;        // owned
    objvec<Variable> selfVars;      // owned
    // Local vars are stored in Scope::localVars; arguments are in prototype->args

    void _dump(fifo&) const;
    Type* _registerType(Type*, Definition* = NULL);

public:
    str const name;
    State* const parent;
    State* const selfPtr;
    Prototype* const prototype;
    objptr<CodeSeg> codeseg;

    State(TypeId, Prototype* proto, const str&, State* parent, State* self);
    ~State();
    void fqName(fifo&) const;
    memint selfVarCount()               { return selfVars.size(); } // TODO: plus inherited
    // TODO: bool identicalTo(Type*) const;
    Definition* addDefinition(const str&, Type*, const variant&);
    Definition* addTypeAlias(const str&, Type*);
    Variable* addSelfVar(const str&, Type*);
    virtual stateobj* newInstance();
    template <class T>
        T* registerType(T* t) { return cast<T*>(_registerType(t)); }
};


// --- Module -------------------------------------------------------------- //


class Module: public State
{
protected:
    strvec constStrings;
    bool complete;
public:
    objvec<Variable> uses; // used module instances are stored in static vars
    Module(const str& name);
    ~Module();
    bool isComplete() const     { return complete; }
    void setComplete()          { complete = true; }
    void addUses(Module*);
    void registerString(str&); // returns a previously registered string if found
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
