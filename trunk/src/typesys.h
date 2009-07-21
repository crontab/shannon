#ifndef __TYPESYS_H
#define __TYPESYS_H

#include "common.h"
#include "runtime.h"

#include <stdint.h>

#include <map>


class Type;
class Variable;
class Definition;
class Constant;
class TypeAlias;
class StateAlias;
class ModuleAlias;

class None;
class Ordinal;
class Enumeration;
class Range;
class Container;
class Fifo;
class Variant;
class TypeReference;
class State;
class Module;
class QueenBee;

typedef Variable ThisVar;
typedef Variable ResultVar;
typedef Variable LocalVar;
typedef Container Container;
typedef Container Vec;
typedef Container Array;
typedef Container Str;
typedef Container Dict;
typedef Container Ordset;
typedef Container Set;


// --- CODE SEGMENT ------------------------------------------------------- //

class CodeGen;

// Defined here because the State type contains code segments. The
// implementation of CodeSeg is in vm.cpp.

class CodeSeg: noncopyable
{
    friend class CodeGen;

protected:
    str code;

    varlist consts;
    mem stksize;
    Module* module; // for references to used modules and their datasegs
    State* state;
    int closed;     // for debugging mainly

    void push_back(uchar u)
            { code.push_back(u); }
    void append(void* p, mem count)
            { code.append((char*)p, count); }
    void resize(mem s)
            { code.resize(s); }
    void close(mem _stksize)
            { assert(++closed == 1); stksize = _stksize; }
    uchar operator[] (mem i) const
            { return uchar(code[i]); }
    void putJumpOffs(mem i, joffs_t o)
            { *(joffs_t*)(code.data() + i) = o; }

    // Execution
    static void vecCat(const variant& vec2, variant* vec1);
    void failAssertion(unsigned file, unsigned linenum) const;
    static void echo(const variant&);

    void run(varstack& stack, langobj* self, variant* result) const; // <-- this is the VM itself

public:
    CodeSeg(Module*, State*);
    ~CodeSeg();

    // For unit tests:
    void clear();
    bool empty() const
        { return code.empty(); }
    mem size() const
        { return code.size(); }
};


// --- BASE LANGUAGE OBJECTS AND COLLECTIONS ------------------------------- //


class Symbol: public object
{
public:
    enum SymbolId { RESULTVAR, LOCALVAR, THISVAR, ARGVAR, // in sync with loaders and storers
                    CONSTANT, TYPEALIAS, STATEALIAS, MODULEALIAS,
                    FIRSTVAR = RESULTVAR };

    SymbolId const symbolId;
    Type* const type;
    str const name;

    Symbol(SymbolId, Type*, const str&);
    ~Symbol();
    bool empty(); // override

    bool isVariable()  { return symbolId <= ARGVAR; }
    bool isDefinition()  { return symbolId >= CONSTANT; }

    bool isThisVar()  { return symbolId == THISVAR; }
    bool isResultVar()  { return symbolId == RESULTVAR; }
    bool isLocalVar()  { return symbolId == LOCALVAR; }
    bool isArgVar()  { return symbolId == ARGVAR; }

    bool isConstant()  { return symbolId == CONSTANT; }
    bool isTypeAlias()  { return symbolId == TYPEALIAS; }
    bool isStateAlias()  { return symbolId == STATEALIAS; }
    bool isModuleAlias()  { return symbolId == MODULEALIAS; }
};


typedef Variable* PVar;

class Variable: public Symbol
{
public:
    mem const id;
    State* const state;
    bool const readOnly;
    Variable(SymbolId, Type*, const str&, mem, State*, bool);
    ~Variable();
};


typedef Definition* PDef;

class Definition: public Symbol
{
public:
    variant const value;
    Definition(SymbolId, Type*, const str&, const variant&);
    ~Definition();
};


class Constant: public Definition
{
public:
    Constant(Type*, const str&, const variant&);
    ~Constant();
};

typedef TypeAlias* PTypeAlias;

class TypeAlias: public Definition
{
public:
    Type* const aliasedType;
    TypeAlias(const str&, Type*);
    ~TypeAlias();
};


class StateAlias: public Definition
{
public:
    State* const aliasedState;
    StateAlias(const str&, State*);
    ~StateAlias();
};


class ModuleAlias: public Definition
{
public:
    Module* const aliasedModule;
    ModuleAlias(const str&, Module*);
    ~ModuleAlias();
};


// --- SYMBOL TABLE, COLLECTIONS ------------------------------------------- //

struct EDuplicate: public emessage
    { EDuplicate(const str& symbol) throw(); };


class _SymbolTable: public noncopyable
{
    typedef std::map<str, Symbol*> Impl;
    Impl impl;
public:
    _SymbolTable();
    ~_SymbolTable();
    bool empty() const  { return impl.empty(); }
    void addUnique(Symbol*);
    Symbol* find(const str&) const;
};


template<class T>
class SymbolTable: public _SymbolTable
{
public:
    void addUnique(T* o)           { _SymbolTable::addUnique(o); }
    T* find(const str& name) const { return (T*)_SymbolTable::find(name); }
};


class _PtrList: public noncopyable
{
    typedef std::vector<void*> Impl;
    Impl impl;
public:
    _PtrList();
    ~_PtrList();
    mem add(void*);
    bool empty()             const { return impl.empty(); }
    mem size()               const { return impl.size(); }
    void clear();
    void* operator[] (mem i) const { return impl[i]; }
};


template<class T>
class PtrList: public _PtrList
{
public:
    mem add(T* p)               { return _PtrList::add(p); }
    T* operator[] (mem i) const { return (T*)_PtrList::operator[](i); }
};


class _List: public _PtrList  // responsible for freeing the objects
{
public:
    _List();
    ~_List();
    void clear();
    mem add(object* o);
    object* operator[] (mem i) const { return (object*)_PtrList::operator[](i); }
};


template<class T>
class List: public _List
{
public:
    mem add(T* p)               { return _List::add(p); }
    T* operator[] (mem i) const { return (T*)_List::operator[](i); }
};


class Scope: public SymbolTable<Symbol>
{
protected:
    Scope* const outer;
    List<Definition> defs;
public:
    Scope(Scope* outer);
    ~Scope();
    virtual Symbol* deepFind(const str&) const; // overridden in Module to look in linked modules
    Constant* addConstant(Type*, const str&, const variant&);
    TypeAlias* addTypeAlias(const str&, Type*);
};


// --- TYPE SYSTEM --------------------------------------------------------- //


void typeMismatch();


typedef Type* PType;

class Type: public object
{
public:
    enum TypeId { NONE, BOOL, CHAR, INT, ENUM, RANGE,
        DICT, VEC, STR, ARRAY, ORDSET, SET, VARFIFO, CHARFIFO, VARIANT, TYPEREF, STATE };

    enum { MAX_ARRAY_INDEX = 256 }; // trigger Dict if bigger than this

protected:
    str name;       // some types have a name for better diagnostics (int, str, ...)
    TypeId const typeId;

    State* owner;   // derivators are inserted into the owner's repositories
    Fifo* derivedFifo;
    Container* derivedVector;
    Container* derivedSet;
    
    void setTypeId(TypeId t)
            { (TypeId&)typeId = t; }

public:
    Type(Type* rt, TypeId);
    ~Type();
    
    bool empty(); // override

    void setOwner(State* _owner)    { assert(owner == NULL); owner = _owner; }
    str  getName()                  { return name; }
    void setName(const str _name)   { if (name.empty()) name = _name; }

    bool is(TypeId t)  { return typeId == t; }
    TypeId getTypeId()  { return typeId; }
    bool isNone()  { return typeId == NONE; }
    bool isBool()  { return typeId == BOOL; }
    bool isChar()  { return typeId == CHAR; }
    bool isInt()  { return typeId == INT; }
    bool isEnum()  { return typeId == ENUM || isBool(); }
    bool isRange()  { return typeId == RANGE; }
    bool isDict()  { return typeId == DICT; }
    bool isArray()  { return typeId == ARRAY; }
    bool isStr()  { return typeId == STR; }
    bool isVec()  { return typeId == VEC; }
    bool isSet()  { return typeId == SET; }
    bool isOrdset()  { return typeId == ORDSET; }
    bool isCharSet();
    bool isVarFifo()  { return typeId == VARFIFO; }
    bool isCharFifo()  { return typeId == CHARFIFO; }
    bool isVariant()  { return typeId == VARIANT; }
    bool isTypeRef()  { return typeId == TYPEREF; }
    bool isState()  { return typeId == STATE; }

    bool isOrdinal()  { return typeId >= BOOL && typeId <= ENUM; }
    bool isContainer()  { return typeId >= DICT && typeId <= SET; }
    bool isModule();
    bool canBeArrayIndex();
    bool canBeOrdsetIndex();

    Fifo* deriveFifo();
    Container* deriveVector();
    Container* deriveSet();

    virtual bool identicalTo(Type*);  // for comparing container elements, indexes
    virtual bool canAssignTo(Type*);  // can assign or automatically convert the type without changing the value
    virtual bool isMyType(variant&);
    virtual void runtimeTypecast(variant&);
};


// --- TYPES ---


typedef State* PState;

class State: public Type, public Scope, public CodeSeg
{
protected:
    List<Type> types;
    List<Variable> thisvars;
    objptr<Variable> resultvar;
    mem startId;

public:
    int const level;
    State* const selfPtr;

    State(Module*, State* parent, Type* resultType);
    ~State();
    bool identicalTo(Type*);
    bool canAssignTo(Type*);
    bool isMyType(variant&);
    template<class T>
        T* registerType(T* t)
            { t->setOwner(this); types.add(t); return t; }
    Variable* addThisVar(Type*, const str&, bool readOnly = false);
    langobj* newObject();
    mem thisSize()
            { return thisvars.size(); }
};


typedef Module* PModule;

class Module: public State
{
protected:
    PtrList<Module> uses;
public:
    Module(const str& name);
    ~Module();
    virtual Symbol* deepFind(const str&) const; // override
    void addUses(Module*); // comes from the global module cache
};


class None: public Type
{
public:
    None();
    bool isMyType(variant&);
};


typedef Ordinal* POrdinal;

class Ordinal: public Type
{
    Range* derivedRange;
protected:
    void reassignRight(integer r) // for enums during their definition
        { assert(r >= left); (integer&)right = r; }
public:
    integer const left;
    integer const right;

    Ordinal(TypeId, integer, integer);
    Range* deriveRange();
    bool identicalTo(Type*);
    bool canAssignTo(Type*);
    bool isMyType(variant&);
    void runtimeTypecast(variant&);
    bool isLe(integer _left, integer _right)
            { return _left >= left && _right <= right; }
    bool rangeFits(integer i);
    bool rangeEq(integer l, integer r)
            { return left == l && right == r; }
    bool rangeEq(Ordinal* t)
            { return rangeEq(t->left, t->right); }
    bool isInRange(integer v)
            { return v >= left && v <= right; }
    virtual Ordinal* deriveSubrange(integer _left, integer _right);
};


typedef Enumeration* PEnum;

class Enumeration: public Ordinal
{
    friend class QueenBee;
protected:
    // Shared between enums: actually the main enum and its subranges; owned
    // by Enumeration objects, refcounted.
    struct EnumValues: public object, public PtrList<Constant>
    {
        EnumValues(): object(NULL) { }
        bool empty() { return false; }
    };
    objptr<EnumValues> values;
    Enumeration(TypeId _typeId); // built-in enums, e.g. bool
public:
    Enumeration();  // user-defined enums
    Enumeration(EnumValues*, integer _left, integer _right);    // subrange
    void addValue(const str&);
    bool identicalTo(Type*);
    bool canAssignTo(Type*);
    Ordinal* deriveSubrange(integer _left, integer _right);
};


typedef Range* PRange;

class Range: public Type
{
    // TODO: implicit conversion to set
public:
    Ordinal* const base;
    Range(Ordinal*);
    bool identicalTo(Type*);
    bool canAssignTo(Type*);
};


// typedef Container* PContainer;
typedef Vec* PVec;
typedef Dict* PDict;
typedef Str* PStr;
typedef Set* PSet;

// Depending on the index and element types, can be one of:
//   DICT:      any, any
//   ARRAY:     ord(256), any
//   VECTOR:    void, any
//   SET:       any, void
//   EMPTYCONT: void, void
class Container: public Type
{
public:
    Type* const index;
    Type* const elem;
    Container(Type* _index, Type* _elem);
    bool identicalTo(Type*);
    mem arrayIndexShift()
        { return CAST(Ordinal*, index)->left; }
    mem ordsetIndexShift()
        { return CAST(Ordinal*, index)->left; }
};


typedef Fifo* PFifo;

class Fifo: public Type
{
protected:
    Type* elem;
public:
    Fifo(Type*);
    bool identicalTo(Type*);
};


class Variant: public Type
{
public:
    Variant();
    bool isMyType(variant&);
    void runtimeTypecast(variant&);
};


typedef TypeReference* PTypeRef;

class TypeReference: public Type
{
public:
    TypeReference();
};


// --- QUEEN BEE ---


class QueenBee: public Module
{
public:
    None* defNone;
    Ordinal* defInt;
    Enumeration* defBool;
    Ordinal* defChar;
    Container* defStr;
    Variant* defVariant;
    Fifo* defCharFifo;
    Variable* siovar;
    Variable* serrvar;
    Variable* sresultvar;

    QueenBee();
    void setup();
    Type* typeFromValue(const variant&);
    void initialize(langobj*);
};


extern TypeReference* defTypeRef;
extern QueenBee* queenBee;


void initTypeSys();
void doneTypeSys();


#endif // __TYPESYS_H
