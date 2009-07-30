#ifndef __TYPESYS_H
#define __TYPESYS_H

#include "common.h"
#include "runtime.h"

#include <stdint.h>

#include <vector>
#include <map>


class Symbol;
class Type;
class Variable;
class Definition;

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

typedef Definition Constant;
typedef Definition TypeAlias;
typedef Definition StateAlias;
typedef Definition ModuleAlias;
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
    Module* hostModule; // for references to used modules and their datasegs
    State* ownState;
    int closed;     // for debugging mainly
    mem fileId;
    mem lineNum;

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
    void failAssertion();

    void run(varstack& stack, langobj* self, variant* result); // <-- this is the VM itself

public:
    CodeSeg(Module*, State*);
    ~CodeSeg();

    // For unit tests:
    void clear();
    bool empty() const
        { return code.empty(); }
    mem size() const
        { return code.size(); }
    void listing(fifo_intf&) const;
};


// --- SYMBOL TABLE, COLLECTIONS ------------------------------------------- //

struct EDuplicate: public exception
{
    str ident;
    EDuplicate(const str& _ident) throw();
    ~EDuplicate() throw();
};


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
    mem size() const            { return _PtrList::size(); }
};


class _List: public _PtrList  // responsible for freeing the objects
{
public:
    _List();
    ~_List();
    void clear();
    mem add(object* o);
    object* operator[] (mem i) const { return (object*)_PtrList::operator[](i); }
    void dump(fifo_intf&) const;
};


template<class T>
class List: public _List
{
public:
    mem add(T* p)               { return _List::add(p); }
    T* operator[] (mem i) const { return (T*)_List::operator[](i); }
};


struct EUnknownIdent: public exception
{
    str const ident;
    EUnknownIdent(const str& _ident) throw();
    ~EUnknownIdent() throw();
};


class Scope: protected SymbolTable<Symbol>
{
protected:
    Scope* const outer;
    List<Definition> defs;
    PtrList<Module> uses;
public:
    Scope(Scope* outer);
    ~Scope();
    Symbol* findShallow(const str& _name) const;
    Symbol* findDeep(const str&) const;
    Constant* addConstant(Type*, const str&, const variant&);
    TypeAlias* addTypeAlias(const str&, Type*);
    void addUses(Module*); // comes from the global module cache
};


// --- BASE LANGUAGE OBJECTS AND COLLECTIONS ------------------------------- //


class Symbol: public object
{
public:
    enum SymbolId { RESULTVAR, LOCALVAR, THISVAR, ARGVAR, // in sync with loaders and storers
                    DEFINITION,
                    FIRSTVAR = RESULTVAR };

    SymbolId const symbolId;
    Type* const type;
    str const name;

    Symbol(SymbolId, Type*, const str&);
    ~Symbol();
    void dump(fifo_intf&) const; // override

    bool isVariable() const  { return symbolId <= ARGVAR; }
    bool isDefinition() const  { return symbolId == DEFINITION; }

    bool isThisVar() const  { return symbolId == THISVAR; }
    bool isResultVar() const  { return symbolId == RESULTVAR; }
    bool isLocalVar() const  { return symbolId == LOCALVAR; }
    bool isArgVar() const  { return symbolId == ARGVAR; }

    bool isTypeAlias() const;
    bool isStateAlias() const;
    bool isModuleAlias() const;
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
    Definition(Type*, const str&, const variant&);
    Definition(const str&, Type*);
    ~Definition();
    void dump(fifo_intf&) const; // override
    Type* aliasedType() const;
    State* aliasedState() const;
    Module* aliasedModule() const;
};


// --- TYPE SYSTEM --------------------------------------------------------- //


void typeMismatch();


typedef Type* PType;

class Type: public object
{
public:
    enum TypeId { NONE, BOOL, CHAR, INT, ENUM, RANGE,
        DICT, VEC, STR, ARRAY, ORDSET, SET, VARFIFO, CHARFIFO, VARIANT,
        TYPEREF, STATE };

    enum { MAX_ARRAY_INDEX = 256 }; // trigger Dict if bigger than this

protected:
    Type(Type* rt, TypeId);

    str name;       // some types have a name for better diagnostics (int, str, ...)
    TypeId const typeId;

    State* owner;   // derivators are inserted into the owner's repositories
    Fifo* derivedFifo;
    Container* derivedVector;
    Container* derivedSet;
    
    void setTypeId(TypeId t)
            { (TypeId&)typeId = t; }

public:
    ~Type();
    
    void dump(fifo_intf&) const; //override
    virtual void fullDump(fifo_intf&) const;

    void setOwner(State* _owner)    { assert(owner == NULL); owner = _owner; }
    str  getName()                  { return name; }
    void setName(const str _name)   { if (name.empty()) name = _name; }

    bool is(TypeId t) const  { return typeId == t; }
    TypeId getTypeId() const  { return typeId; }
    bool isNone() const  { return typeId == NONE; }
    bool isBool() const  { return typeId == BOOL; }
    bool isChar() const  { return typeId == CHAR; }
    bool isInt() const  { return typeId == INT; }
    bool isEnum() const  { return typeId == ENUM || isBool(); }
    bool isRange() const  { return typeId == RANGE; }
    bool isDict() const  { return typeId == DICT; }
    bool isArray() const  { return typeId == ARRAY; }
    bool isString() const  { return typeId == STR; }
    bool isVector() const  { return typeId == VEC || typeId == STR; }
    bool isSet() const  { return typeId == SET; }
    bool isOrdset() const  { return typeId == ORDSET; }
    bool isCharSet() const;
    bool isNullContainer() const;
    bool isVarFifo() const  { return typeId == VARFIFO; }
    bool isCharFifo() const  { return typeId == CHARFIFO; }
    bool isVariant() const  { return typeId == VARIANT; }
    bool isTypeRef() const  { return typeId == TYPEREF; }
    bool isState() const  { return typeId == STATE; }

    bool isOrdinal() const  { return typeId >= BOOL && typeId <= ENUM; }
    bool isContainer() const  { return typeId >= DICT && typeId <= SET; }
    bool isModule() const;
    bool canBeArrayIndex() const;
    bool canBeOrdsetIndex() const;

    Fifo* deriveFifo();
    Container* deriveVector();
    Container* deriveSet();
    Container* createContainer(Type* indexType);

    virtual bool identicalTo(Type*);  // for comparing container elements, indexes
    virtual bool canAssignTo(Type*);  // can assign or automatically convert the type without changing the value
    virtual bool isMyType(const variant&);
    virtual void runtimeTypecast(variant&);
};


// --- TYPES ---


typedef State* PState;

class State: public Type, public Scope, public CodeSeg
{
protected:
    List<Type> types;
    objptr<Variable> resultvar;
    List<Variable> args;
    List<Variable> thisvars;
    mem startId;

    void dumpDefinitions(fifo_intf&) const;

public:
    int const level;
    State* const selfPtr;

    State(Module*, State* parent, Type* resultType);
    ~State();
    void fullDump(fifo_intf&) const; //override
    void listing(fifo_intf&) const;
    bool identicalTo(Type*);
    bool canAssignTo(Type*);
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
    friend class CodeSeg;
    friend class Compiler;
protected:
    objptr<langobj> instance;
    std::vector<str> fileNames;
    
    virtual void initialize(varstack&); // create instance and run the main code or skip if created already
    virtual void finalize()             // destroy instance
            { instance.clear(); }
public:
    Module(const str& name);
    ~Module();
    void fullDump(fifo_intf&) const; //override
    mem registerFileName(const str&);
    // Run as main and return the result value (system.sresult)
    variant run();  // <-- execution of the whole thing starts here
};


class None: public Type
{
public:
    None();
    bool isMyType(const variant&);
};


typedef Ordinal* POrdinal;

class Ordinal: public Type
{
    friend class QueenBee;
    Range* derivedRange;
protected:
    Ordinal(TypeId, integer, integer);
    void reassignRight(integer r) // for enums during their definition
        { assert(r >= left); (integer&)right = r; }
public:
    integer const left;
    integer const right;

    ~Ordinal();
    void fullDump(fifo_intf&) const;
    Range* deriveRange();
    bool identicalTo(Type*);
    bool canAssignTo(Type*);
    bool isMyType(const variant&);
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
            { EnumValues(): object(NULL) { } };
    objptr<EnumValues> values;
    Enumeration(TypeId _typeId); // built-in enums, e.g. bool
    Enumeration(EnumValues*, integer _left, integer _right);    // subrange
public:
    Enumeration();  // user-defined enums
    ~Enumeration();
    void fullDump(fifo_intf&) const;
    void addValue(const str&);
    bool identicalTo(Type*);
    bool canAssignTo(Type*);
    Ordinal* deriveSubrange(integer _left, integer _right);
};


typedef Range* PRange;

class Range: public Type
{
    friend class Ordinal;
    // TODO: implicit conversion to set
protected:
    Range(Ordinal*);
public:
    Ordinal* const base;
    ~Range();
    void fullDump(fifo_intf&) const; //override
    bool identicalTo(Type*);
    bool canAssignTo(Type*);
};


// typedef Container* PContainer;
typedef Container* PContainer;
typedef Vec* PVec;
typedef Dict* PDict;
typedef Array* PArray;
typedef Str* PStr;
typedef Set* PSet;
typedef Ordset* POrdset;

// Depending on the index and element types, can be one of:
//   DICT:      any, any
//   ARRAY:     ord(256), any
//   VECTOR:    none, any
//   SET:       any, none
class Container: public Type
{
    friend class Type;
    friend class QueenBee;
protected:
    Container(Type* _index, Type* _elem);
public:
    Type* const index;
    Type* const elem;
    ~Container();
    void fullDump(fifo_intf&) const;
    bool identicalTo(Type*);
    mem arrayIndexShift()
        { return CAST(Ordinal*, index)->left; }
    mem ordsetIndexShift()
        { return CAST(Ordinal*, index)->left; }
    void runtimeTypecast(variant&);
};


typedef Fifo* PFifo;

class Fifo: public Type
{
    friend class Type;
protected:
    Type* elem;
    Fifo(Type*);
public:
    ~Fifo();
    void fullDump(fifo_intf&) const; //override
    bool identicalTo(Type*);
};


class Variant: public Type
{
    friend class QueenBee;
protected:
    Variant();
public:
    ~Variant();
    bool isMyType(const variant&);
    void runtimeTypecast(variant&);
};


typedef TypeReference* PTypeRef;

class TypeReference: public Type
{
    friend void initTypeSys();
protected:
    TypeReference();
public:
    ~TypeReference();
};


// --- QUEEN BEE ---


class QueenBee: public Module
{
    friend void initTypeSys();
public:
    None* defNone;
    Ordinal* defInt;
    Enumeration* defBool;
    Ordinal* defChar;
    Container* defStr;
    Variant* defVariant;
    Fifo* defCharFifo;
    Container* defNullContainer;
    Variable* siovar;
    Variable* serrvar;
    Variable* sresultvar;

    Type* typeFromValue(const variant&);

protected:
    QueenBee();
    void setup();
    void initialize(varstack&); // override
};


extern TypeReference* defTypeRef;
extern QueenBee* queenBee;
extern langobj* nullLangObj;    // modules with empty static datasegs can use this


void initTypeSys();
void doneTypeSys();

// ------------------------------------------------------------------------- //


inline Type* Definition::aliasedType() const
    { return CAST(Type*, value._obj()); }

inline State* Definition::aliasedState() const
    { return CAST(State*, value._obj()); }

inline Module* Definition::aliasedModule() const
    { return CAST(Module*, value._obj()); }

inline bool Type::isNullContainer() const
    { return this == queenBee->defNullContainer; }


#endif // __TYPESYS_H
