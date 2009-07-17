#ifndef __TYPESYS_H
#define __TYPESYS_H

#include "common.h"
#include "runtime.h"
#include "symbols.h"

#include <stdint.h>


class Type;
class Variable;
class Constant;
class StateAlias;
class QueenBee;

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
class StateBody;

typedef Container Container;
typedef Container Vector;
typedef Container String;
typedef Container Set;
typedef StateAlias ModuleAlias;


// --- TYPE SYSTEM --------------------------------------------------------- //


void typeMismatch();


class Base: public Symbol
{
public:
    enum BaseId { VARIABLE, DEFINITION };

    BaseId const baseId;

    Base(Type*, BaseId);
    Base(Type*, const str&, BaseId);

    bool isVariable() const     { return baseId == VARIABLE; }
    bool isDefinition() const   { return baseId == DEFINITION; }
};


typedef Type* PType;

// TODO: SEMFIFO: s fifo with a semaphore

class Type: public object
{
    friend class State;
    friend class Context;

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


typedef Variable* PVar;

class Variable: public Base
{
public:
    Type* const type;
    mem const id;
    bool const readOnly;
    Variable(const str& _name, Type*, mem _id, bool _readOnly = false);
    ~Variable();
};


typedef Constant* PConst;

class Constant: public Base
{
public:
    Type* const type;
    variant const value;

    Constant(const str&, Type*, const variant&);
    ~Constant();
    bool isTypeAlias()
            { return type->isTypeRef(); }
    bool isModuleAlias();
    Type* getAlias();
};


class StateAlias: public Constant
{
public:
    StateAlias(const str&, State*, StateBody*);
    ~StateAlias();
    State* getStateType()
            { return (State*)type; }
    StateBody* getBody()
            { assert(value.as_object()->get_rt() == type);
                return (StateBody*)value._object(); }
};


class Scope: public SymbolTable<Base>
{
protected:
    Scope* const outer;
    PtrList<Module> uses;
    List<Variable> vars;
public:
    Scope(Scope* _outer);
    ~Scope();
    Base* deepFind(const str&) const;
    Variable* addVariable(const str&, Type*);
    mem dataSize()
            { return vars.size(); }
};


// --- TYPES ---


typedef State* PState;
typedef State* PModule;

class State: public Type, public Scope
{
protected:
    List<Constant> consts;
    List<Type> types;
public:
    int const level;

    State(State* _parent);
    ~State();
    bool identicalTo(Type*);
    bool canCastImplTo(Type*);
    bool isMyType(variant&);
    langobj* newObject();
    template<class T>
        T* registerType(T* t)
            { t->setOwner(this); types.add(t); return t; }
    Constant* addConstant(const str& name, Type* type, const variant& value);
    Constant* addTypeAlias(const str& name, Type* type);
};


class Module: public State
{
public:
    mem const id;
    Module(mem _id);
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
protected:
    Variable* siovar;
    Variable* serrvar;
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
    virtual void run(langobj* self, varstack&);
};


extern objptr<TypeReference> defTypeRef;
extern objptr<QueenBee> queenBee;


void initTypeSys();
void doneTypeSys();


#endif // __TYPESYS_H
