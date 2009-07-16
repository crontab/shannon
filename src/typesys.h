#ifndef __TYPESYS_H
#define __TYPESYS_H

#include "common.h"
#include "runtime.h"
#include "symbols.h"

#include <stdint.h>


class Type;
class Variable;
class Constant;
class State;
class Module;
class QueenBee;

class None;
class Ordinal;
class Enumeration;
class Range;
class Container;
class Fifo;
class Variant;
class TypeReference;

typedef Container Container;
typedef Container Vector;
typedef Container String;
typedef Container Set;


// --- VIRTUAL MACHINE (partial) ---

// Some bits of the virtual machine are needed here because each State class
// holds its own code segment.

class Context: noncopyable
{
protected:
    List<Module> modules;
    List<langobj> datasegs;
    Module* topModule;
public:
    Context();
    Module* registerModule(Module*);   // for built-in modules
    Module* addModule(const str& name);
    void run(varstack&); // <--- this is where execution starts
};


class CodeSeg: noncopyable
{
    friend class CodeGen;
    friend class State;
    
protected:
    str code;
    varlist consts;
    mem stksize;
    mem returns;
    // These can't be refcounted as it will introduce circular references. Both
    // can be NULL if this is a const expression executed at compile time.
    State* state;
    Context* context;
#ifdef DEBUG
    bool closed;
#endif

    // Code generation
    int addOp(unsigned c);
    void add8(uint8_t i);
    void add16(uint16_t i);
    void addInt(integer i);
    void addPtr(void* p);
    void close(mem _stksize, mem _returns);

    // Execution
    static void varToVec(Vector* type, const variant& elem, variant* result);
    static void varCat(Vector* type, const variant& elem, variant* vec);
    static void vecCat(const variant& vec2, variant* vec1);

    void run(langobj* self, varstack&) const;

public:
    CodeSeg(State*, Context*);
    ~CodeSeg();
    bool empty() const
        { return code.empty(); }
    void clear(); // for unit tests
};


class ConstCode: public CodeSeg
{
public:
    ConstCode(): CodeSeg(NULL, NULL) { }
    void run(variant&) const;
};


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

public:
    enum TypeId { NONE, BOOL, CHAR, INT, ENUM, RANGE,
        DICT, ARRAY, VECTOR, SET, ORDSET, FIFO, VARIANT, TYPEREF, STATE };

    enum { MAX_ARRAY_INDEX = 256 }; // trigger Dict if bigger than this

protected:
    str name;       // some types have a name for better diagnostics (int, str, ...)

    TypeId const typeId;
    State* owner;   // used when producing various derivators
    
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
    friend class Scope;
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
    friend class State;
public:
    Type* const type;
    variant const value;
    Constant(const str&, Type*, const variant&);
    ~Constant();
    bool isTypeAlias()
            { return type->isTypeRef(); }
    Type* getAlias();
};


class Scope: public SymbolTable<Base>
{
protected:
    Scope* const outer;
    PtrList<State> uses;
    List<Variable> vars;
public:
    Scope(Scope* _outer);
    ~Scope();
    Base* deepFind(const str&) const;
    Variable* addVariable(const str&, Type*);
    mem dataSize()
            { return vars.size(); }
};


typedef State* PState;

class State: public Type, public Scope
{
    friend class Context;
protected:
    List<Constant> consts;
    List<Type> types;
    List<State> states;

    CodeSeg main;
    CodeSeg final;
    int const level;

    virtual void run(langobj* self, varstack&);
    virtual void finalize(langobj* self, varstack&);

public:
    State(const str& _name, State* _parent, Context*);
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
    const mem id;
    Module(const str& _name, mem _id, Context* _context);
};


// --- TYPES ---


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
