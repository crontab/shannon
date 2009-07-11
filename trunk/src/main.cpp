
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


#include "runtime.h"
#include "source.h"
#include "symbols.h"


class Type;
class Variable;
class Constant;
class State;
class Module;
class QueenBee;

class Ordinal;
class Enumeration;
class Range;
class Dictionary;
class Array;
class Vector;
class Set;
class Variant;
class TypeReference;


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
        DICT, ARRAY, VECTOR, SET, CHARSET, FIFO, VARIANT, TYPEREF };

protected:
    str name;       // some types have a name for better diagnostics (int, str, ...)
    State* owner;   // used when producing various derivators

    TypeId const typeId;

public:
    Type(TypeId);
    ~Type();

    void setOwner(State* _owner)   { assert(owner == NULL); owner = _owner; }

    bool is(TypeId t)  { return typeId == t; }
    bool isVoid()  { return typeId == VOID; }
    bool isBool()  { return typeId == BOOL; }
    bool isChar()  { return typeId == CHAR; }
    bool isInt()  { return typeId == INT; }
    bool isEnum()  { return typeId == ENUM; }
    bool isRange()  { return typeId == RANGE; }
    bool isDict()  { return typeId == DICT; }
    bool isArray()  { return typeId == ARRAY; }
    bool isVector()  { return typeId == VECTOR; }
    bool isSet()  { return typeId == SET; }
    bool isCharSet()  { return typeId == CHARSET; }
    bool isFifo()  { return typeId == FIFO; }
    bool isVariant()  { return typeId == VARIANT; }
    bool isTypeRef()  { return typeId == TYPEREF; }

    bool isOrdinal()  { return typeId >= BOOL && typeId <= ENUM; }
    bool canBeArrayIndex();
    bool isString();
    bool isCharFifo();

    virtual bool identicalTo(Type* t)  { return t == this; } // for comparing container elements, indexes
    virtual bool canCastImplTo(Type* t)  { return identicalTo(t); } // can assign, implicit cast
    virtual bool canCastExplTo(Type* t)  { return canCastImplTo(t); } // explicit cast
    virtual bool canCompare(Type* t, bool eq)  { return eq && identicalTo(t); } // eq: only equality or all types of comparisons?
};


class Variable: public Base
{
    friend class Scope;
protected:
    Type* const type;
    Variable(const str& _name, Type*);
    ~Variable();
};


class Constant: public Base
{
    friend class State;
protected:
    Type* type;
    variant value;
protected:
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
    Type* addType(Type* t)
            { t->setOwner(this); types.add(t); return t; }
    template<class T>
    T* registerType(T* t)
            { types.add(t); return t; }
    Constant* addConstant(const str& name, Type* type, const variant& value);
    Constant* addTypeAlias(const str& name, Type* type);
};


class Module: public State
{
public:
    Module(const str& _name): State(_name, NULL)  { }
};


class QueenBee: public Module
{
public:
    TypeReference* const defaultTypeRef;
    Ordinal* const defaultInt;
    
    QueenBee();
};


extern objptr<QueenBee> queenBee;

void initTypeSys();
void doneTypeSys();


// --- TYPES ---

typedef Ordinal* POrdinal;

class Ordinal: public Type
{
    Range* derivedRange;
protected:
    integer left;
    integer right;
    void reassignRight(integer r) // for enums during their definition
        { right = r; }
public:
    Ordinal(TypeId, integer, integer);
    Range* deriveRange();
    bool rangeLe(integer _left, integer _right)
            { return left >= _left && right <= _right; }
    bool rangeFits(integer i)
            { return right - left <= i; }
    bool rangeEq(Ordinal* t)
            { return left == t->left && right == t->right; }
    bool identicalTo(Type* t)
            { return t->is(typeId) && rangeEq(POrdinal(t)); }
    bool canCastImplTo(Type* t)
            { return t->is(typeId); }
    bool canCastExplTo(Type* t)
            { return t->isOrdinal(); }
    bool canCompare(Type* t, bool)
            { return t->is(typeId); }
    virtual Ordinal* deriveSubrange(integer _left, integer _right);
};


typedef Enumeration* PEnum;

class Enumeration: public Ordinal
{
protected:
    // Shared between enums: actually the main enum and its subranges; owned
    // by Enumeration objects, refcounted.
    class EnumValues: public object, public PtrList<Constant>  { };
    objptr<EnumValues> values;
public:
    Enumeration();
    Enumeration(EnumValues*, integer _left, integer _right);    // subrange
    void addValue(const str&);
    bool identicalTo(Type* t)
            { return this == t; }
    bool canCastImplTo(Type* t)
            { return t->isEnum() && values == PEnum(t)->values; }
    bool canCompare(Type* t, bool)
            { return canCastImplTo(t); }
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
    // TODO: Range comparisons maybe? < <= > >=
};


typedef Dictionary* PDict;

class Dictionary: public Type
{
protected:
    Type* elem;
    Type* index;
    Dictionary(TypeId _typeId, Type* _elem, Type* _index);
public:
    Dictionary(Type* _elem, Type* _index);
    bool identicalTo(Type* t)
            { return t->is(typeId) && elem->identicalTo(PDict(t)->elem)
                && index->identicalTo(PDict(t)->index); }
};


typedef Array* PArray;

class Array: public Dictionary
{
    // Array is just an specialization of dictionary with a small ordinal
    // index type.
public:
    enum { MAX_INDEX_RANGE = 256 };
    Array(Type* _elem, Ordinal* _index); // only byte-range ordinals are allowed
};


typedef Vector* PVector;

class Vector: public Dictionary
{
public:
    Vector(Type* _elem);
    bool isString()
            { return elem->isChar(); }
};


class Variant: public Type
{
public:
    Variant();
    bool identicalTo(Type* t)  { return t->isVariant(); }
    bool canCastImplTo(Type* t)  { return true; }
    bool canCastExplTo(Type* t)  { return true; }
    bool canCompare(Type* t, bool eq)  { return true; }
};


class TypeReference: public Type
{
public:
    TypeReference();
    bool identicalTo(Type* t)  { return t->isTypeRef(); }
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


bool Type::canBeArrayIndex()
    { return isOrdinal() && POrdinal(this)->rangeFits(Array::MAX_INDEX_RANGE); }

bool Type::isString()
    { return isVector() && PVector(this)->isString(); }


// --- Variable ----------------------------------------------------------- //


Variable::Variable(const str& _name, Type* _type)
    : Base(_name, VARIABLE), type(_type)  { }

Variable::~Variable()  { }


// --- Constant ----------------------------------------------------------- //


Constant::Constant(const str& _name, Type* _type)
    : Base(_name, CONSTANT), type(queenBee->defaultTypeRef),
      value((object*)_type)  { }


Constant::Constant(const str& _name, Type* _type, const variant& _value)
    : Base(_name, CONSTANT), type(_type), value(_value)  { }


Constant::~Constant()  { }


Type* Constant::getAlias()
{
    assert(isTypeAlias());
    return PType(value.as_object());
}


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


Variable* Scope::addVariable(const str& name, Type* type)
{
    objptr<Variable> v = new Variable(name, type);
    addUnique(v);   // may throw
    vars.add(v);
    return v;
}


// --- State --------------------------------------------------------------- //


State::State(const str& _name, State* _parent)
  : Base(_name, STATE), Scope(_parent), parent(_parent),
    level(_parent == NULL ? 0 : _parent->level + 1) { }

State::~State()  { }


Constant* State::addConstant(const str& name, Type* type, const variant& value)
{
    objptr<Constant> c = new Constant(name, type, value);
    addUnique(c); // may throw
    consts.add(c);
    return c;
}


Constant* State::addTypeAlias(const str& name, Type* type)
{
    objptr<Constant> c = new Constant(name, type);
    if (type->name.empty())
        type->name = name;
    addUnique(c); // may throw
    consts.add(c);
    return c;
}


// --- Module -------------------------------------------------------------- //


// --- Ordinal ------------------------------------------------------------- //


DEF_EXCEPTION(ERange, "Invalid subrange")


Ordinal::Ordinal(TypeId _type, integer _left, integer _right)
    : Type(_type), derivedRange(NULL), left(_left), right(_right)
{
    assert(isBool() || isChar() || isInt() || isEnum());
}


DERIVEX(Ordinal, Range)


Ordinal* Ordinal::deriveSubrange(integer _left, integer _right)
{
    if (_left == left && _right == right)
        return this;
    if (_left < left || _right > right)
        throw ERange();
    return new Ordinal(typeId, _left, _right);
}


// --- Enumeration --------------------------------------------------------- //


Enumeration::Enumeration(): Ordinal(ENUM, 0, -1), values(new EnumValues())  { }


Enumeration::Enumeration(EnumValues* _values, integer _left, integer _right)
    : Ordinal(ENUM, _left, _right), values(_values)
{
    assert(_left >= 0 && _left <= _right && mem(_right) < _values->size());
}


void Enumeration::addValue(const str& name)
{
    reassignRight(
        values->add(
            owner->addConstant(name, this, variant(values->size()))));
}


Ordinal* Enumeration::deriveSubrange(integer _left, integer _right)
{
    if (_left == left && _right == right)
        return this;
    if (_left < left || _right > right)
        throw ERange();
    return new Enumeration(values, _left, _right);
}


// --- Range --------------------------------------------------------------- //


Range::Range(Ordinal* _base): Type(RANGE), base(_base)  { }


// --- Dictionary ---------------------------------------------------------- //


Dictionary::Dictionary(TypeId _typeId, Type* _elem, Type* _index)
    : Type(_typeId), elem(_elem), index(_index)  { }


Dictionary::Dictionary(Type* _elem, Type* _index)
    : Type(DICT), elem(_elem), index(_index)  { }


// --- Array --------------------------------------------------------------- //


Array::Array(Type* _elem, Ordinal* _index)
    : Dictionary(ARRAY, _elem, _index)
{
    assert(_index->rangeFits(MAX_INDEX_RANGE));
}


// --- Vector -------------------------------------------------------------- //


// Vector::Vector(Type* _elem): Dictionary(VECTOR, _elem, )  { }


// --- Variant ------------------------------------------------------------- //


Variant::Variant(): Type(VARIANT)  { }


// --- TypeReference ------------------------------------------------------- //


TypeReference::TypeReference(): Type(TYPEREF)  { }


// --- QueenBee ------------------------------------------------------------ //


QueenBee::QueenBee()
  : Module("system"),
    defaultTypeRef(registerType(new TypeReference())),
    defaultInt(registerType(new Ordinal(Type::INT, INTEGER_MIN, INTEGER_MAX)))
{
    addTypeAlias("typeref", defaultTypeRef);
    addTypeAlias("int", defaultInt);
}


objptr<QueenBee> queenBee;

void initTypeSys()
{
    queenBee = new QueenBee();
}


void doneTypeSys()
{
    queenBee = NULL;
}


// --- tests --------------------------------------------------------------- //


int main()
{
    initTypeSys();
    try
    {
        Parser parser("x", new in_text("x"));
        
        State state("s", NULL);
        {
            objptr<Enumeration> ep = new Enumeration();
            ep->setOwner(&state);
            ep->addValue("a");
            try
                { ep->addValue("a"); }
            catch (EDuplicate& e)
                { fout << e.what() << endl; }
            ep->addValue("b");
            ep->addValue("c");
            state.addVariable("v", ep);
            Ordinal* op = state.registerType(new Ordinal(Type::INT, 0, 10));
            Range* rp = state.registerType(new Range(op));
//            state.registerType(new Vector(op));
            state.registerType(new Variant());
            state.registerType(new Array(rp, op));
        }
    }
    catch (std::exception& e)
    {
        ferr << "Exception: " << e.what() << endl;
    }
    doneTypeSys();
#ifdef DEBUG
    assert(object::alloc == 0);
#endif
}

