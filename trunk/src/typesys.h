#ifndef __TYPESYS_H
#define __TYPESYS_H

#include "common.h"
#include "runtime.h"

#include <stdint.h>

#include <map>


class Type;
class Variable;
class Constant;

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

typedef Container Container;
typedef Container Vector;
typedef Container String;
typedef Container Set;
typedef Constant TypeAlias;
typedef TypeAlias StateAlias;
typedef StateAlias ModuleAlias;


// --- CODE SEGMENT ------------------------------------------------------- //

class CodeGen;
class Context;

// Defined here because the State type contains code segments. The
// implementation of CodeSeg is in vm.cpp.

class CodeSeg: noncopyable
{
    friend class CodeGen;
    friend class State;

    // This object can be duplicated if necessary with a different context
    // assigned; the code segment is a refcounted string, so copying would
    // be efficient.
protected:
    str code;
    varlist consts;
    mem stksize;
    // These can't be refcounted as it will introduce circular references. Both
    // can be NULL if this is a const expression executed at compile time.
    State* state;
    Context* context;
#ifdef DEBUG
    bool closed;
#endif

    // Code generation
    void add8(uint8_t i);
    void add16(uint16_t i);
    void addInt(integer i);
    void addPtr(void* p);
    void close(mem _stksize);
    void resize(mem s)
        { code.resize(s); }
    uint8_t operator[] (mem i) const { return uint8_t(code[i]); }

    // Execution
    static void varToVec(Vector* type, const variant& elem, variant* result);
    static void varCat(const variant& elem, variant* vec);
    static void vecCat(const variant& vec2, variant* vec1);

    void run(varstack& stack, langobj* self, variant* result) const; // <-- this is the VM itself

public:
    CodeSeg(State*, Context*);
    ~CodeSeg();

    // For unit tests:
    void clear();
    bool empty() const
        { return code.empty(); }
    mem size() const
        { return code.size(); }
};


// --- BASE LANGUAGE OBJECTS AND COLLECTIONS ------------------------------- //


class Base: public object
{
public:
    enum BaseId { THISVAR, RESULTVAR, LOCALVAR, ARGVAR,
        CONSTANT, TYPEALIAS, STATEALIAS, MODULEALIAS };

    BaseId const baseId;
    Type* const type;
    str const name;
    mem const id;

    Base(BaseId, Type*, const str&, mem);
    ~Base();

    bool isVariable()  { return baseId <= ARGVAR; }
    bool isDefinition()  { return baseId >= CONSTANT; }

    bool isThisVar()  { return baseId == THISVAR; }
    bool isResultVar()  { return baseId == RESULTVAR; }
    bool isLocalVar()  { return baseId == LOCALVAR; }
    bool isArgVar()  { return baseId == ARGVAR; }
    bool isConstant()  { return baseId == CONSTANT; }
    bool isTypeAlias()  { return baseId == TYPEALIAS; }
    bool isStateAlias()  { return baseId == STATEALIAS; }
    bool isModuleAlias()  { return baseId == MODULEALIAS; }
};


typedef Variable* PVar;

class Variable: public Base
{
public:
    bool const readOnly;
    Variable(BaseId, Type*, const str&, mem, bool _readOnly = false);
    ~Variable();
};


typedef Constant* PConst;

class Constant: public Base
{
public:
    variant const value;
    Constant(BaseId, Type*, const str&, mem, const variant&);
    ~Constant();
    Type* getAlias();
};


// --- SYMBOL TABLE, COLLECTIONS ------------------------------------------- //

struct EDuplicate: public emessage
    { EDuplicate(const str& symbol) throw(); };


class _BaseTable: public noncopyable
{
    typedef std::map<str, Base*> Impl;
    Impl impl;
public:
    _BaseTable();
    ~_BaseTable();
    bool empty()             const { return impl.empty(); }
    void addUnique(Base*);
    Base* find(const str&) const;
};


template<class T>
class BaseTable: public _BaseTable
{
public:
    void addUnique(T* o)           { _BaseTable::addUnique(o); }
    T* find(const str& name) const { return (T*)_BaseTable::find(name); }
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


class Scope: public BaseTable<Base>
{
protected:
    Scope* const outer;
    List<Constant> consts;
    PtrList<Module> uses;
    mem startId;
public:
    Scope(Scope* outer, mem startId);
    ~Scope();
    Base* deepFind(const str&) const;
    Constant* addConstant(Type*, const str&, const variant&);
    Constant* addTypeAlias(Type*, const str&);
};


// --- TYPE SYSTEM --------------------------------------------------------- //


void typeMismatch();


typedef Type* PType;

// TODO: SEMFIFO: s fifo with a semaphore

class Type: public object
{
    friend class Scope;

public:
    enum TypeId { NONE, BOOL, CHAR, INT, ENUM, RANGE,
        DICT, ARRAY, VECTOR, SET, ORDSET, FIFO, VARIANT, TYPEREF, STATE };

    enum { MAX_ARRAY_INDEX = 256 }; // trigger Dict if bigger than this

protected:
    str name;       // some types have a name for better diagnostics (int, str, ...)
    TypeId const typeId;

    State* owner;   // derivators are inserted into the owner's repositories
    Fifo* derivedFifo;
    Container* derivedVector;
    Container* derivedSet;
    
    void setName(const str _name)
            { assert(name.empty()); name = _name; }
    void setTypeId(TypeId t)
            { (TypeId&)typeId = t; }

public:
    Type(Type* rt, TypeId);
    ~Type();

    void setOwner(State* _owner)   { assert(owner == NULL); owner = _owner; }

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
    bool isVector()  { return typeId == VECTOR; }
    bool isSet()  { return typeId == SET; }
    bool isOrdset()  { return typeId == ORDSET; }
    bool isCharSet();
    bool isFifo()  { return typeId == FIFO; }
    bool isVariant()  { return typeId == VARIANT; }
    bool isTypeRef()  { return typeId == TYPEREF; }
    bool isState()  { return typeId == STATE; }

    bool isOrdinal()  { return typeId >= BOOL && typeId <= ENUM; }
    bool isContainer()  { return typeId >= DICT && typeId <= SET; }
    bool isModule();
    bool isString();
    bool isCharFifo();
    bool canBeArrayIndex();
    bool canBeOrdsetIndex();

    Fifo* deriveFifo();
    Container* deriveVector();
    Container* deriveSet();

    virtual bool identicalTo(Type*);  // for comparing container elements, indexes
    virtual bool canCastImplTo(Type*);  // can assign or automatically convert the type without changing the value
    virtual bool isMyType(variant&);
    virtual void runtimeTypecast(variant&);
};


inline Type* Constant::getAlias()
            { assert(value.is_object()); return CAST(Type*, value._object()); }


// --- TYPES ---


typedef State* PState;
typedef State* PModule;

class State: public Type, public Scope, public CodeSeg
{
    friend class Context;

protected:
    List<Type> types;
    List<Variable> thisvars;

    CodeSeg final;
    void finalize(varstack& stack, langobj* self)
        { final.run(stack, self, NULL); }

public:
    int const level;

    State(State* _parent, Context*);
    ~State();
    bool identicalTo(Type*);
    bool canCastImplTo(Type*);
    bool isMyType(variant&);
    template<class T>
        T* registerType(T* t)
            { t->setOwner(this); types.add(t); return t; }
    Variable* addThisVar(Type*, const str&, bool readOnly = false);
    langobj* newObject();
    mem thisSize()
            { return thisvars.size(); }
};


class Module: public State
{
    friend class Context;
protected:
    Module(Context* context, mem _id);
public:
    mem const id;
    ~Module();
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
    integer left;
    integer right;
    void reassignRight(integer r) // for enums during their definition
        { assert(r >= left); right = r; }
public:
    Ordinal(TypeId, integer, integer);
    Range* deriveRange();
    bool identicalTo(Type*);
    bool canCastImplTo(Type*);
    bool isMyType(variant&);
    void runtimeTypecast(variant&);
    bool isLe(integer _left, integer _right)
            { return _left >= left && _right <= right; }
    bool rangeFits(integer i)
            { return right - left <= i; }
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
public:
    Enumeration();  // user-defined enums
    Enumeration(EnumValues*, integer _left, integer _right);    // subrange
    void addValue(const str&);
    bool identicalTo(Type*);
    bool canCastImplTo(Type*);
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
    bool canCastImplTo(Type*);
};


typedef Container* PContainer;
typedef Vector* PVector;
typedef String* PString;
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
    friend class Context;
protected:
    Variable* siovar;
    Variable* serrvar;
    Variable* sresultvar;
public:
    None* defNone;
    Ordinal* defInt;
    Enumeration* defBool;
    Ordinal* defChar;
    Container* defStr;
    Variant* defVariant;
    Fifo* defCharFifo;

    QueenBee();
    void setup();
    Type* typeFromValue(const variant&);
    void initialize(langobj*);
};


extern objptr<TypeReference> defTypeRef;
extern objptr<QueenBee> queenBee;


void initTypeSys();
void doneTypeSys();


#endif // __TYPESYS_H
