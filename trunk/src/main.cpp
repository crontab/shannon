
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


#include "runtime.h"
#include "source.h"
#include "symbols.h"


class Variable;
class Constant;
class State;
class Module;

class Ordinal;
class Range;


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


class Type: public object
{
protected:
    State* owner; // for storing derivators

    enum TypeId { VOID, BOOL, CHAR, INT, ENUM, RANGE,
        VECTOR, ARRAY, DICT, TINYSET, CHARSET, SET, FIFO };

    TypeId const typeId;
    
public:
    Type(TypeId);
    ~Type();
    
    void setOwner(State* _owner)   { assert(owner == NULL); owner = _owner; }

    bool is(TypeId t) const { return typeId == t; }
    bool isVoid() const { return typeId == VOID; }
    bool isBool() const { return typeId == BOOL; }
    bool isChar() const { return typeId == CHAR; }
    bool isInt() const { return typeId == INT; }
    bool isEnum() const { return typeId == ENUM; }
    bool isRange() const { return typeId == RANGE; }
    bool isVector() const { return typeId == VECTOR; }
    bool isArray() const { return typeId == ARRAY; }
    bool isDict() const { return typeId == DICT; }
    bool isTinySet() const { return typeId == TINYSET; }
    bool isCharSet() const { return typeId == CHARSET; }
    bool isSet() const { return typeId == SET; }
    bool isFifo() const { return typeId == FIFO; }

    bool isOrdinal() const { return typeId >= BOOL && typeId <= ENUM; }
    bool isString() const;
    bool isCharFifo() const;
    bool isTinyRange() const;
    bool isByteRange() const;

    virtual bool compatibleWith(Type* t)  { return t == this; } // can assign
    virtual bool canStaticCastTo(Type* t)  { return compatibleWith(t); }
    virtual bool canCompare(Type* t, bool eq)  { return eq; } // eq: only equality or all types of comparisons
};


class Scope: SymbolTable<Base>
{
protected:
    Scope* const outer;
    PtrList<State> uses;
    List<Variable> vars;

public:
    Scope(Scope* _outer);
    ~Scope();
    Base* deepFind(const str&) const;
};


class Variable: public Base
{
public:
    Type* const type;

    Variable(const str& _name, Type*);
    ~Variable();
};


class Constant: public Base
{
public:
    Type* const type;
    variant const value;
    
    Constant(const str& _name, Type*, const variant&);
    ~Constant();
};


class State: public Base, public Scope
{
protected:
    List<Constant> consts;
    List<Type> types;
    List<State> states;

public:
    State* const parent;
    int const level;

    State(const str& _name, State* _parent);
    ~State();

    langobj* newObject();
    Type* addType(Type* t) { t->setOwner(this); types.add(t); return t; }
};


class Module: public State
{
public:
    Module(const str& _name): State(_name, NULL)  { }
};


// --- TYPES ---

class Range;

typedef Ordinal* POrdinal;

class Ordinal: public Type
{
    Range* derivedRange;

public:
    integer const left;
    integer const right;
    
    Ordinal(TypeId, integer, integer);

    Range* deriveRange();
    bool rangeLe(integer _left, integer _right)
        { return left >= _left && right <= _right; }
    bool rangeFits(integer i)
        { return right - left <= i; }

    virtual bool compatibleWith(Type* t)  { return t->is(typeId); }
    virtual bool canStaticCastTo(Type* t)  { return t->isOrdinal(); }
    virtual bool canCompare(Type* t, bool)  { return t->is(typeId); }
};


typedef Range* PRange;

class Range: public Type
{
public:
    Ordinal* base;

    Range(Ordinal*);

    virtual bool compatibleWith(Type* t)
        { return t->isRange() && base->compatibleWith(PRange(t)->base); }
};


// --- BASIC LANGUAGE OBJECTS ---------------------------------------------- //


#define DERIVEX(h,d) \
    d* h::derive##d() { \
        assert(owner != NULL); \
        if (derived##d == NULL) { \
            derived##d = new d(this); \
            owner->addType(derived##d); } \
    return derived##d; }


Base::Base(BaseId _id): Symbol(null_str), baseId(_id)  { }
Base::Base(const str& _name, BaseId _id): Symbol(_name), baseId(_id)  { }


// --- Type ---------------------------------------------------------------- //


Type::Type(TypeId _t): typeId(_t) { }
Type::~Type() { }


bool Type::isTinyRange() const
    { return isInt() && POrdinal(this)->rangeFits(TINYSET_BITS); }

bool Type::isByteRange() const
    { return isInt() && POrdinal(this)->rangeFits(256); }


// --- Scope --------------------------------------------------------------- //


Scope::Scope(Scope* _outer): outer(_outer)  { }
Scope::~Scope()  { }


Base* Scope::deepFind(const str& ident) const
{
    Base* b = find(ident);
    if (b != NULL)
        return b;
    for (int i = uses.size() - 1; i >= 0; i--)
    {
        b = uses[i]->find(ident);
        if (b != NULL)
            return b;
    }
    if (outer != NULL)
        return outer->deepFind(ident);
    return NULL;
}


// --- Variable ----------------------------------------------------------- //


Variable::Variable(const str& _name, Type* _type)
    : Base(_name, VARIABLE), type(_type)  { }

Variable::~Variable()  { }


// --- Constant ----------------------------------------------------------- //


Constant::Constant(const str& _name, Type* _type, const variant& _value)
    : Base(_name, CONSTANT), type(_type), value(_value)  { }

Constant::~Constant()  { }


// --- State --------------------------------------------------------------- //


State::State(const str& _name, State* _parent)
  : Base(_name, STATE), Scope(_parent), parent(_parent),
    level(_parent == NULL ? 0 : _parent->level + 1) { }

State::~State()  { }


// --- Module -------------------------------------------------------------- //


// --- Ordinal ------------------------------------------------------------- //

Ordinal::Ordinal(TypeId _type, integer _left, integer _right)
    : Type(_type), derivedRange(NULL), left(_left), right(_right)  { }


DERIVEX(Ordinal, Range)


// --- Range --------------------------------------------------------------- //


Range::Range(Ordinal* _base): Type(RANGE), base(_base)  { }


// --- tests --------------------------------------------------------------- //


int main()
{
    {
        variant v;
        Parser parser("x", new in_text("x"));
        List<Symbol> list;
        fifo f(true);
        
        Scope s(NULL);

        fout << sizeof(object) << endl;

        fout << "Hello, world" << endl;
    }
#ifdef DEBUG
    assert(object::alloc == 0);
#endif
}

