#ifndef __TYPESYS_H
#define __TYPESYS_H

#include "common.h"
#include "runtime.h"
#include "symbols.h"


class Type;
class Variable;
class Constant;
class State;
class Module;
class QueenBee;

class Void;
class Ordinal;
class Enumeration;
class Range;
class Container;
class Fifo;
class Variant;
class TypeReference;


// --- VIRTUAL MACHINE (partial) ---

// Some bits of the virtual machine are needed here because each State class
// holds its own code segment.

class Context: noncopyable
{
    friend class CodeSeg;
protected:
    List<Module> modules;
    List<tuple> datasegs;
    Module* topModule;
    void resetDatasegs();
public:
    Context();
    Module* addModule(const str& name);
    void run(varstack&); // TODO: execute all init blocks in modules
};


class CodeSeg: noncopyable
{
    friend class CodeGen;
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

    int addOp(unsigned c);
    void add8(uchar i);
    void add16(unsigned short i);
    void addInt(integer i);
    void addPtr(void* p);
    void close(mem _stksize, mem _returns);
    bool empty() const
        { return code.empty(); }
    void doRun(variant*, const uchar* ip);
public:
    CodeSeg(State*, Context*);
    ~CodeSeg();
    void clear(); // for unit tests
    void run(varstack&);
};


class ConstCode: public CodeSeg
{
public:
    ConstCode(): CodeSeg(NULL, NULL) { }
    void run(variant&);
};


// --- BASIC LANGUAGE OBJECTS ---

class Base: public Symbol
{
public:
    enum BaseId { VARIABLE, CONSTANT, STATE };

    BaseId const baseId;

    Base(BaseId);
    Base(const str&, BaseId);

    bool isVariable() const   { return baseId == VARIABLE; }
    bool isConstant() const   { return baseId == CONSTANT; }
    bool isState() const      { return baseId == STATE; }
};


typedef Type* PType;

// TODO: SEMFIFO: s fifo with a semaphore

class Type: public object
{
    friend class State;

public:
    enum TypeId { VOID, BOOL, CHAR, INT, ENUM, RANGE,
        DICT, ARRAY, VECTOR, SET, FIFO, VARIANT, TYPEREF };

protected:
    str name;       // some types have a name for better diagnostics (int, str, ...)

    TypeId const typeId;
    State* owner;   // used when producing various derivators
    
    Fifo* derivedFifo;
    Container* derivedVector;
    Container* derivedSet;
    
    void setTypeId(TypeId t)
        { (TypeId&)typeId = t; }

public:
    Type(TypeId);
    ~Type();

    void setOwner(State* _owner)   { assert(owner == NULL); owner = _owner; }

    bool is(TypeId t)  { return typeId == t; }
    bool isVoid()  { return typeId == VOID; }
    bool isBool()  { return typeId == BOOL; }
    bool isChar()  { return typeId == CHAR; }
    bool isInt()  { return typeId == INT; }
    bool isEnum()  { return typeId == ENUM || isBool(); }
    bool isRange()  { return typeId == RANGE; }
    bool isDict()  { return typeId == DICT; }
    bool isArray()  { return typeId == ARRAY; }
    bool isVector()  { return typeId == VECTOR; }
    bool isSet()  { return typeId == SET; }
    bool isCharSet();
    bool isFifo()  { return typeId == FIFO; }
    bool isVariant()  { return typeId == VARIANT; }
    bool isTypeRef()  { return typeId == TYPEREF; }

    bool isOrdinal()  { return typeId >= BOOL && typeId <= ENUM; }
    bool isContainer()  { return typeId >= DICT && typeId <= SET; }
    bool canBeArrayIndex();
    bool canBeSmallSetIndex();

    Fifo* deriveFifo();
    Container* deriveVector();
    Container* deriveSet();

    virtual bool identicalTo(Type* t)  { return t == this; } // for comparing container elements, indexes
    virtual bool canCastImplTo(Type* t)  { return identicalTo(t); } // can assign, implicit cast
};


typedef Variable* PVar;

class Variable: public Base
{
    friend class Scope;
protected:
    Type* const type;
    Variable(const str& _name, Type*);
    ~Variable();
};


typedef Constant* PConst;

class Constant: public Base
{
    friend class State;
public:
    Type* const type;
    variant const value;
    Constant(const str&, Type*);                    // type alias
    Constant(const str&, Type*, const variant&);    // ordinary constant
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
    int dataSize()
            { return vars.size(); }
};


typedef State* PState;

class State: public Base, public Scope
{
protected:
    List<Constant> consts;
    List<Type> types;
    List<State> states;

    CodeSeg main;
    CodeSeg finalize;

public:
    State* const parent;
    int const level;
    State(const str& _name, State* _parent, Context*);
    ~State();
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
    Module(const str& _name, mem _id, Context* _context)
        : State(_name, NULL, _context), id(_id)  { }
};


// --- TYPES ---


class Void: public Type
{
public:
    Void();
    virtual bool identicalTo(Type* t)  { return t->isVoid(); }
    virtual bool canCastImplTo(Type* t)  { return t->isVoid() || t->isVariant(); }
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
    bool rangeLe(integer _left, integer _right)
            { return left >= _left && right <= _right; }
    bool rangeFits(integer i)
            { return right - left <= i; }
    bool rangeEq(integer l, integer r)
            { return left == l && right == r; }
    bool rangeEq(Ordinal* t)
            { return rangeEq(t->left, t->right); }
    bool identicalTo(Type* t)
            { return t->is(typeId) && rangeEq(POrdinal(t)); }
    bool canCastImplTo(Type* t)
            { return t->is(typeId); }
    virtual Ordinal* deriveSubrange(integer _left, integer _right);
};


typedef Enumeration* PEnum;

class Enumeration: public Ordinal
{
    friend class QueenBee;
protected:
    // Shared between enums: actually the main enum and its subranges; owned
    // by Enumeration objects, refcounted.
    class EnumValues: public object, public PtrList<Constant>  { };
    objptr<EnumValues> values;
    Enumeration(TypeId _typeId); // built-in enums, e.g. bool
public:
    Enumeration();  // user-defined enums
    Enumeration(EnumValues*, integer _left, integer _right);    // subrange
    void addValue(const str&);
    bool identicalTo(Type* t)
            { return this == t; }
    bool canCastImplTo(Type* t)
            { return t->isEnum() && values == PEnum(t)->values; }
    Ordinal* deriveSubrange(integer _left, integer _right);
};


typedef Range* PRange;

class Range: public Type
{
protected:
    Ordinal* base;
public:
    Range(Ordinal*);
    bool identicalTo(Type* t)
            { return t->isRange() && base->identicalTo(PRange(t)->base); }
};


typedef Container* PContainer;

// Depending on the index and element types, can be one of:
//   DICT:      any, any
//   ARRAY:     ord(256), any
//   VECTOR:    void, any
//   SET:       any, void
//   EMPTYCONT: void, void
class Container: public Type
{
protected:
    Type* index;
    Type* elem;
public:
    enum { MAX_ARRAY_INDEX = 256 };
    Container(Type* _index, Type* _elem);
    bool identicalTo(Type* t)
            { return t->is(typeId) && elem->identicalTo(PContainer(t)->elem)
                && index->identicalTo(PContainer(t)->index); }
    bool canCastImplTo(Type* t)
            { return isEmptyCont() || identicalTo(t); }
    bool isString()
            { return isVector() && elem->isChar(); }
    bool isSmallSet()
            { return isSet() && index->canBeSmallSetIndex(); }
    bool isEmptyCont()
            { return elem->isVoid() && index->isVoid(); }
};


typedef Fifo* PFifo;

class Fifo: public Type
{
protected:
    Type* elem;
public:
    Fifo(Type*);
    bool identicalTo(Type* t)
            { return t->is(typeId) && elem->identicalTo(PFifo(t)->elem); }
    bool isCharFifo()
            { return elem->isChar(); }
};


class Variant: public Type
{
public:
    Variant();
    bool identicalTo(Type* t)  { return t->isVariant(); }
    bool canCastImplTo(Type* t)  { return true; }
};


class TypeReference: public Type
{
public:
    TypeReference();
    bool identicalTo(Type* t)  { return t->isTypeRef(); }
};


// --- QUEEN BEE ---


class QueenBee: public Module
{
public:
    TypeReference* defaultTypeRef;
    Void* defaultVoid;
    Ordinal* defaultInt;
    Enumeration* defaultBool;
    Ordinal* defaultChar;
    Container* defaultStr;
    Container* defaultEmptyContainer;
    
    QueenBee();
    void setup();
};


extern objptr<QueenBee> queenBee;

void initTypeSys();
void doneTypeSys();


#endif // __TYPESYS_H
