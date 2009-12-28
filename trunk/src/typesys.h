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
class StateDef;
class ModuleInst;
class ModuleVar;

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
        void append(const T& t)     { code.push_back<T>(t); }
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
    CodeSeg(State*) throw();
    ~CodeSeg() throw();

    State* getType() const          { return cast<State*>(parent::getType()); }
    memint size() const             { return code.size(); }
    memint getStackSize() const     { return stackSize; }
    bool empty() const;
    void close();

    // Return a NULL-terminated string ready to be run: NULL char is an opcode
    // to exit the function
    const char* getCode() const     { assert(closed); return code.data(); }
};


// --- Symbols & Scope ----------------------------------------------------- //


class Symbol: public symbol
{
public:
    enum SymbolId { LOCALVAR, SELFVAR, DEFINITION, MODULEINST };

    SymbolId const symbolId;
    Type* const type;

    Symbol(const str&, SymbolId, Type*) throw();
    ~Symbol() throw();

    bool isDefinition() const   { return symbolId == DEFINITION; }
    bool isTypeAlias() const;
    bool isSelfVar() const      { return symbolId == SELFVAR; }
    bool isLocalVar() const     { return symbolId == LOCALVAR; }
    bool isVariable() const     { return symbolId <= SELFVAR; }
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
    symtbl symbols;         // symbol table for search
    void addUnique(Symbol* s);
public:
    Scope* const outer;
    Scope(Scope* _outer);
    virtual ~Scope();
    Symbol* find(const str&) const;                 // returns NULL or Symbol
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
public:
    enum TypeId {
        TYPEREF, NONE, VARIANT, REF,
        BOOL, CHAR, INT, ENUM,
        NULLCONT, VEC, SET, DICT,
        FIFO, PROTOTYPE, FUNC, CLASS, MODULE };

protected:
    objptr<Reference> refType;
    str alias;      // for more readable diagnostics output, but not really needed
    State* host;

    Type(Type*, TypeId) throw();
//    Type(TypeId) throw();
    bool empty() const;
    static TypeId contType(Type* i, Type* e);

public:
    TypeId const typeId;

    ~Type() throw();

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

    bool isPrototype() const    { return typeId == PROTOTYPE; }
    bool isFunction() const     { return typeId == FUNC; }
    bool isClass() const        { return typeId == CLASS; }
    bool isModule() const       { return typeId == MODULE; }
    bool isAnyState() const     { return typeId >= FUNC && typeId <= MODULE; }

    virtual str definition(const str& ident) const;
    virtual bool identicalTo(Type*) const;
    virtual bool canAssignTo(Type*) const;

    Reference* deriveRefType()     { return refType; }
    Container* deriveVec();
    Container* deriveSet();
    Container* deriveContainer(Type* idxType);
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


// --- Prototype ----------------------------------------------------------- //


class Prototype: public Type
{
protected:
    Type* returnType;
    objvec<Variable> args;          // owned
public:
    Prototype(Type* retType) throw();
    ~Prototype() throw();
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

    Type* _registerType(Type*);
    Type* _registerType(const str&, Type*);

public:
    State* const selfPtr;
    Prototype* const prototype;
    objptr<CodeSeg> codeseg;

    State(TypeId, Prototype* proto, State* parent, State* self) throw();
    ~State() throw();
    memint selfVarCount()               { return selfVars.size(); } // TODO: plus inherited
    // TODO: bool identicalTo(Type*) const;
    Definition* addDefinition(const str&, Type*, const variant&);
    Definition* addTypeAlias(const str&, Type*);
    Variable* addSelfVar(const str&, Type*);
    ModuleVar* addModuleVar(const str&, Module*);
    virtual stateobj* newInstance();
    template <class T>
        T* registerType(const str& n, T* t) { return (T*)_registerType(n, t); }
    template <class T>
        T* registerType(T* t) { return (T*)_registerType(t); }
};


// --- Module -------------------------------------------------------------- //


class Module: public State
{
    friend class ModuleInst;
protected:
    vector<str> constStrings;
    bool complete;
public:
    objvec<ModuleVar> uses; // used module instances are stored in static vars
    Module() throw();
    ~Module() throw();
    bool isComplete() const     { return complete; }
    void setComplete()          { complete = true; }
    void addUses(const str&, Module*);
    void registerString(str&); // returns a previously registered string if found
};


class ModuleVar: public Variable
{
public:
    ModuleVar(const str& n, Module* m, memint _id, State* s) throw();
    ~ModuleVar() throw();
    Module* getModuleType()     { return cast<Module*>(type); }
};


class ModuleInst: public Symbol
{
public:
    objptr<Module> module;
    objptr<stateobj> instance;

    ModuleInst(const str&, Module*) throw();         // for the system module
    ModuleInst(const str&) throw();
    ~ModuleInst() throw();
    bool isComplete() const     { return module->isComplete(); }
    void setComplete()          { module->setComplete(); }
    void initialize(Context*, rtstack&);
    void finalize();
};


// --- QueenBee (system module) -------------------------------------------- //


class QueenBee: public Module
{
    typedef Module parent;
    friend void initTypeSys();
protected:
    QueenBee() throw();
    ~QueenBee() throw();
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
extern objptr<None> defNone;
extern objptr<Prototype> defPrototype;
extern objptr<QueenBee> queenBee;

#endif // __TYPESYS_H
