
#include "version.h"
#include "typesys.h"


// --- LANGUAGE OBJECT ----------------------------------------------------- //


void* langobj::operator new(size_t, mem datasize)
{
    return new char[sizeof(langobj) + datasize * sizeof(variant)];
}


// Defined in runtime.h
langobj::langobj(State* type)
    : object(type)
#ifdef DEBUG
    , varcount(type->thisSize())
#endif
{
//    memset(vars, 0, type->dataSize() * sizeof(variant));
}


bool langobj::empty()  { return false; }


void langobj::_idx_err()
{
    throw emessage("Object index error");
}


langobj::~langobj()
{
    mem count = CAST(State*, get_rt())->thisSize();
    while (count--)
        vars[count].~variant();
}


// --- BASE LANGUAGE OBJECTS AND COLLECTIONS ------------------------------- //


Base::Base(BaseId _baseId, Type* _type, const str& _name, mem _id)
    : object(NULL), baseId(_baseId), type(_type), name(_name), id(_id)  { }

Base::~Base()  { }

bool Base::empty()  { return false; }


EDuplicate::EDuplicate(const str& symbol) throw()
    : emessage("Duplicate identifier: " + symbol)  { }


_BaseTable::_BaseTable()  { }
_BaseTable::~_BaseTable()  { }


void _BaseTable::addUnique(Base* o)
{
    std::pair<Impl::iterator, bool> result = impl.insert(Impl::value_type(o->name, o));
    if (!result.second)
        throw EDuplicate(o->name);
}


Base* _BaseTable::find(const str& name) const
{
    Impl::const_iterator i = impl.find(name);
    if (i == impl.end())
        return NULL;
    return i->second;
}


_PtrList::_PtrList()  { }
_PtrList::~_PtrList()  { }
void _PtrList::clear()  { impl.clear(); }


mem _PtrList::add(void* p)
{
    impl.push_back(p);
    return size() - 1;
}


_List::_List()              { }
_List::~_List()             { clear(); }
mem _List::add(object* o)   { return _PtrList::add(grab(o)); }


void _List::clear()
{
    mem i = size();
    while (i--)
        release(operator[](i));
    _PtrList::clear();
}


// --- Variable ----------------------------------------------------------- //


Variable::Variable(BaseId _baseId, Type* _type, const str& _name, mem _id, State* _state, bool _readOnly)
    : Base(_baseId, _type, _name, _id), state(_state), readOnly(_readOnly)
{
    assert(isVariable());
    if (_id > 255) fatal(0x3002, "Variable ID too big");
}

Variable::~Variable()  { }


// --- Constant ----------------------------------------------------------- //


Constant::Constant(BaseId _baseId, Type* _type, const str& _name, mem _id, const variant& _value)
    : Base(_baseId, _type, _name, _id), value(_value)  { }

Constant::~Constant()  { }


// --- TYPE SYSTEM --------------------------------------------------------- //


void typeMismatch()
        { throw emessage("Type mismatch"); }


// Constructor placeholders for the DERIVEX macro
#define new_Fifo(x)     new Fifo(x)
#define new_Vector(x)   new Container(queenBee->defNone, x)
#define new_Set(x)      new Container(x, queenBee->defNone)
#define new_Range(x)    new Range(x)


#define DERIVEX(d) \
    { if (derived##d == NULL) \
        derived##d = owner->registerType(new_##d(this)); \
      return derived##d; }


// --- Type ---------------------------------------------------------------- //


Type::Type(Type* rt, TypeId _t)
  : object(rt), typeId(_t), owner(NULL), derivedFifo(NULL),
    derivedVector(NULL), derivedSet(NULL)
{
    assert(rt != NULL);
}

Type::~Type() { }

bool Type::empty()  { return false; }

bool Type::isModule()
    { return typeId == STATE && PState(this)->level == 0; }

bool Type::canBeArrayIndex()
    { return isOrdinal() && POrdinal(this)->rangeFits(Container::MAX_ARRAY_INDEX); }

bool Type::canBeOrdsetIndex()
    { return isOrdinal() && POrdinal(this)->rangeFits(charset::BITS); }


Fifo* Type::deriveFifo()        { DERIVEX(Fifo) }
Container* Type::deriveVector() { DERIVEX(Vector) }
Container* Type::deriveSet()    { DERIVEX(Set) }


bool Type::identicalTo(Type* t)  // for comparing container elements, indexes
    { return t->is(typeId); }

bool Type::canAssignTo(Type* t)  // can assign or automatically convert the type without changing the value
    { return identicalTo(t); }

bool Type::isMyType(variant& v)
    { return (v.is_object() && identicalTo(v._object()->get_rt())); }

void Type::runtimeTypecast(variant& v)
    { if (!isMyType(v)) typeMismatch(); }


// --- Scope --------------------------------------------------------------- //


Scope::Scope(Scope* _outer)
    : outer(_outer)  { }
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


Constant* Scope::addConstant(Type* type, const str& name, const variant& value)
{
    objptr<Constant> c = new Constant(Base::CONSTANT, type, name, 0, value);
    addUnique(c); // may throw
    consts.add(c);
    return c;
}


Constant* Scope::addTypeAlias(Type* type, const str& name)
{
    objptr<Constant> c = new Constant(Base::TYPEALIAS, defTypeRef, name, 0, type);
    addUnique(c); // may throw
    consts.add(c);
    if (type->name.empty())
        type->name = name;
    return c;
}


void Scope::addUses(ModuleAlias* alias)
{
    addUnique(alias);
    uses.add(alias->getModule());
}


// --- State --------------------------------------------------------------- //


State::State(State* _parent, Context* context, Type* resultType)
  : Type(defTypeRef, STATE), Scope(_parent),
    CodeSeg(this, context), startId(0),
    level(_parent == NULL ? 0 : _parent->level + 1),
    selfPtr(_parent == NULL ? this : _parent->selfPtr)
{
    // TODO: functions returning self
    setOwner(_parent);
    if (resultType != NULL)
    {
        // TODO: multiple result vars (result1, result2, ... ?)
        resultvar = new ResultVar(Base::RESULTVAR, resultType, "result", 0, this, false);
        addUnique(resultvar);
    }
}


State::~State()  { }


bool State::identicalTo(Type* t)
    { return t == this; }

bool State::canAssignTo(Type* t)
    { return t == this; } // TODO: implement inheritance


langobj* State::newObject()
{
    mem s = thisSize();
    if (s == 0)
        return NULL;
    return new(s) langobj(this);
}


bool State::isMyType(variant& v)
    { return (v.is_object() && v._object()->get_rt()->canAssignTo(this)); }


Module::Module(Context* context, mem _id)
    : State(NULL, context, NULL), id(_id)  { }
Module::~Module()  { }


Variable* State::addThisVar(Type* type, const str& name, bool readOnly)
{
    mem id = startId + thisvars.size(); // startId will be used with derived classes
    if (id >= 255)
        throw emessage("Maximum number of variables within this object reached");
    objptr<Variable> v = new ThisVar(Base::THISVAR, type, name, id, this, readOnly);
    addUnique(v); // may throw
    thisvars.add(v);
    return v;
}


// --- None ---------------------------------------------------------------- //


None::None(): Type(defTypeRef, NONE)  { }
bool None::isMyType(variant& v)  { return v.is_null(); }


// --- Ordinal ------------------------------------------------------------- //


Ordinal::Ordinal(TypeId _type, integer _left, integer _right)
    : Type(defTypeRef, _type), derivedRange(NULL), left(_left), right(_right)
{
    assert(isOrdinal());
}


Range* Ordinal::deriveRange()   { DERIVEX(Range) }


bool Ordinal::rangeFits(integer i)
{
    if (left > right)
        return false;
    integer diff = right - left + 1;
    if (diff <= 0)  // overflow, e.g. default int range
        return false;
    return diff <= i;
}


Ordinal* Ordinal::deriveSubrange(integer _left, integer _right)
{
    if (rangeEq(_left, _right))
        return this;
    if (!isLe(_left, _right))
        throw emessage("Subrange error");
    return new Ordinal(typeId, _left, _right);
}


bool Ordinal::identicalTo(Type* t)
    { return t->is(typeId) && rangeEq(POrdinal(t)); }

bool Ordinal::canAssignTo(Type* t)
    { return t->is(typeId); }


bool Ordinal::isMyType(variant& v)
{
    return isBool() || (v.is_ordinal() && isInRange(v.as_ordinal()));
}


void Ordinal::runtimeTypecast(variant& v)
{
    if (isBool())
    {
        v = v.to_bool();
        return;
    }
    if (!v.is_ordinal())
        typeMismatch();
    if (!isInRange(v.as_ordinal()))
        throw emessage("Out of range");
    if (isChar())
        v.type = variant::CHAR;
    else if (isInt() || isEnum())
        v.type = variant::INT;
    else
        notimpl();
}


// --- Enumeration --------------------------------------------------------- //


Enumeration::Enumeration(): Ordinal(ENUM, 0, -1), values(new EnumValues())  { }


Enumeration::Enumeration(TypeId _typeId)
    : Ordinal(_typeId, 0, -1), values(new EnumValues())  { }


Enumeration::Enumeration(EnumValues* _values, integer _left, integer _right)
    : Ordinal(ENUM, _left, _right), values(_values)
{
    assert(_left >= 0 && _left <= _right && mem(_right) < _values->size());
}


void Enumeration::addValue(const str& name)
{
    reassignRight(
        values->add(
            owner->addConstant(this, name, values->size())));
}


bool Enumeration::identicalTo(Type* t)
    { return this == t; }

bool Enumeration::canAssignTo(Type* t)
    { return t->isEnum() && values == PEnum(t)->values; }


Ordinal* Enumeration::deriveSubrange(integer _left, integer _right)
{
    if (_left == left && _right == right)
        return this;
    if (_left < left || _right > right)
        throw emessage("Subrange error");
    return new Enumeration(values, _left, _right);
}


// --- Range --------------------------------------------------------------- //


Range::Range(Ordinal* _base)
    : Type(defTypeRef, RANGE), base(_base)  { }


bool Range::identicalTo(Type* t)
        { return t->isRange() && base->identicalTo(PRange(t)->base); }

bool Range::canAssignTo(Type* t)
        { return t->isRange() && base->canAssignTo(PRange(t)->base); }


// --- Container ---------------------------------------------------------- //


Container::Container(Type* _index, Type* _elem)
    : Type(defTypeRef, NONE), index(_index), elem(_elem)
{
    if (index->isNone())
        setTypeId(elem->isChar() ? STR : VEC);
    else if (elem->isNone())
        setTypeId(index->canBeOrdsetIndex() ? ORDSET : SET);
    else
        setTypeId(index->canBeArrayIndex() ? ARRAY : DICT);
}


bool Container::identicalTo(Type* t)
    { return t->is(typeId) && elem->identicalTo(CAST(Container*, t)->elem)
            && index->identicalTo(CAST(Container*, t)->index); }


// --- Fifo ---------------------------------------------------------------- //


Fifo::Fifo(Type* _elem): Type(defTypeRef, NONE), elem(_elem)
{
    setTypeId(elem->isChar() ? CHARFIFO : VARFIFO);
}


bool Fifo::identicalTo(Type* t)
    { return t->is(typeId) && elem->identicalTo(PFifo(t)->elem); }


// --- Variant ------------------------------------------------------------- //


Variant::Variant(): Type(defTypeRef, VARIANT)  { }
bool Variant::isMyType(variant&)  { return true; }
void Variant::runtimeTypecast(variant&)  { }


// --- TypeReference ------------------------------------------------------- //


TypeReference::TypeReference(): Type(this, TYPEREF)  { }


// --- QueenBee ------------------------------------------------------------ //


QueenBee::QueenBee()
  : Module(NULL, 0)
{
    registerType(defTypeRef);
    defNone = registerType(new None());
    defInt = registerType(new Ordinal(Type::INT, INTEGER_MIN, INTEGER_MAX));
    defBool = registerType(new Enumeration(Type::BOOL));
    defChar = registerType(new Ordinal(Type::CHAR, 0, 255));
    defStr = NULL;
    defVariant = registerType(new Variant());
    defCharFifo = NULL;
    siovar = NULL;
    serrvar = NULL;
}


void QueenBee::setup()
{
    // This can't be done in the constructor while the global object queenBee
    // is not assigned
    defStr = defChar->deriveVector();
    defCharFifo = defChar->deriveFifo();
    addTypeAlias(defTypeRef, "typeref");
    addTypeAlias(defNone, "none");
    addConstant(defNone, "null", null);
    addTypeAlias(defInt, "int");
    defBool->addValue("false");
    defBool->addValue("true");
    addTypeAlias(defBool, "bool");
    addTypeAlias(defStr, "str");
    addTypeAlias(defVariant, "any");
    siovar = addThisVar(defCharFifo, "sio");
    serrvar = addThisVar(defCharFifo, "serr");
    sresultvar = addThisVar(defVariant, "sresult");
    addConstant(defInt, "__ver_major", SHANNON_VERSION_MAJOR);
    addConstant(defInt, "__ver_minor", SHANNON_VERSION_MINOR);
}


Type* QueenBee::typeFromValue(const variant& v)
{
    switch (v.getType())
    {
    case variant::NONE: return defNone;
    case variant::BOOL: return defBool;
    case variant::CHAR: return defChar;
    case variant::INT:  return defInt;
    case variant::REAL: notimpl(); break;
    case variant::STR:  return defStr;
    case variant::OBJECT: return v._object()->get_rt();
    }
    return NULL;
}


void QueenBee::initialize(langobj* self)
{
    ::new(self->var(siovar->id)) variant(&sio);
    ::new(self->var(serrvar->id)) variant(&serr);
    ::new(self->var(sresultvar->id)) variant();
}


TypeReference* defTypeRef;
QueenBee* queenBee;

static objptr<TypeReference> _defTypeRef;
static objptr<QueenBee> _queenBee;


void initTypeSys()
{
    // Because all Type objects are also runtime objects, they all have a
    // runtime type of "type reference". The initial typeref object refers to
    // itself and should be created before anything else in the type system.
    _defTypeRef = defTypeRef = new TypeReference();
    _queenBee = queenBee = new QueenBee();
    queenBee->setup();
    sio.set_rt(queenBee->defCharFifo);
    serr.set_rt(queenBee->defCharFifo);
//    fout->set_rt();
}


void doneTypeSys()
{
    sio.clear_rt();
    serr.clear_rt();
    _queenBee = queenBee = NULL;
    _defTypeRef = defTypeRef = NULL;
}

