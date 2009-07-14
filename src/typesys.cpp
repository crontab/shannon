

#include "typesys.h"
#include "vm.h"


void varTypeMismatch()  { throw EVarTypeMismatch(); }


// --- EXECUTION CONTEXT --------------------------------------------------- //


Context::Context()
    : topModule(NULL)  { }


Module* Context::addModule(const str& name)
{
    if (modules.size() == 255)
        throw emessage("Maximum number of modules reached");
    topModule = new Module(name, modules.size(), this);
    modules.add(topModule);
    datasegs.add(new tuple());
    return topModule;
}


void Context::resetDatasegs()
{
    assert(modules.size() == datasegs.size());
    for (mem i = 0; i < modules.size(); i++)
    {
        tuple* d = datasegs[i];
        d->clear();
        d->resize(modules[i]->dataSize());
    }
}



// --- CODE SEGMENT -------------------------------------------------------- //

CodeSeg::CodeSeg(State* _state, Context* _context)
  : stksize(0), returns(0), state(_state), context(_context)
#ifdef DEBUG
    , closed(false)
#endif
    { }

CodeSeg::~CodeSeg()  { }

void CodeSeg::clear()
{
    code.clear();
    consts.clear();
    stksize = 0;
    returns = 0;
#ifdef DEBUG
    closed = false;
#endif
}

int CodeSeg::addOp(unsigned c)
    { code.push_back(c); return code.size() - 1; }

void CodeSeg::add8(uchar i)
    { code.push_back(i); }

void CodeSeg::add16(ushort i)
    { code.append((char*)&i, 2); }

void CodeSeg::addInt(integer i)
    { code.append((char*)&i, sizeof(i)); }

void CodeSeg::addPtr(void* p)
    { code.append((char*)&p, sizeof(p)); }


void CodeSeg::close(mem _stksize, mem _returns)
{
    assert(!closed);
    if (!code.empty())
        addOp(opEnd);
    stksize = _stksize;
    returns = _returns;
#ifdef DEBUG
    closed = true;
#endif
}


// --- TYPE SYSTEM --------------------------------------------------------- //

// Constructor placeholders for the DERIVEX macro
#define new_Fifo(x)     new Fifo(x)
#define new_Vector(x)   new Container(queenBee->defNone, x)
#define new_Set(x)      new Container(x, queenBee->defNone)
#define new_Range(x)    new Range(x)


#define DERIVEX(d) \
    { if (derived##d == NULL) \
        derived##d = owner->registerType(new_##d(this)); \
      return derived##d; }


Base::Base(BaseId _id): Symbol(null_str), baseId(_id)  { }
Base::Base(const str& _name, BaseId _id): Symbol(_name), baseId(_id)  { }


// --- Type ---------------------------------------------------------------- //


Type::Type(TypeId _t)
  : typeId(_t), owner(NULL), derivedFifo(NULL),
    derivedVector(NULL), derivedSet(NULL)  { }

Type::~Type() { }


bool Type::isString()
    { return isVector() && PVector(this)->isString(); }

bool Type::canBeArrayIndex()
    { return isOrdinal() && POrdinal(this)->rangeFits(Container::MAX_ARRAY_INDEX); }

bool Type::canBeSmallSetIndex()
    { return isOrdinal() && POrdinal(this)->rangeFits(charset::BITS); }


Fifo* Type::deriveFifo()        { DERIVEX(Fifo) }
Container* Type::deriveVector() { DERIVEX(Vector) }
Container* Type::deriveSet()    { DERIVEX(Set) }


// --- Variable ----------------------------------------------------------- //


Variable::Variable(const str& _name, Type* _type)
    : Base(_name, VARIABLE), type(_type)  { }

Variable::~Variable()  { }


// --- Constant ----------------------------------------------------------- //


Constant::Constant(const str& _name, Type* _type)
    : Base(_name, CONSTANT), type(queenBee->defTypeRef),
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
    if (vars.size() == 255)
        throw emessage("Maximum number of variables within one scope is reached");
    objptr<Variable> v = new Variable(name, type);
    addUnique(v);   // may throw
    vars.add(v);
    return v;
}


// --- State --------------------------------------------------------------- //


State::State(const str& _name, State* _parent, Context* _context)
  : Type(STATE), Scope(_parent),
    main(this, _context), finalize(this, _context),
    level(_parent == NULL ? 0 : _parent->level + 1)
{
    setName(_name);
    setOwner(_parent);
}


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
        type->setName(name);
    addUnique(c); // may throw
    consts.add(c);
    return c;
}


// --- Module -------------------------------------------------------------- //


// --- None ---------------------------------------------------------------- //


None::None(): Type(NONE)  { }


// --- Ordinal ------------------------------------------------------------- //


DEF_EXCEPTION(ESubrange, "Invalid subrange")


Ordinal::Ordinal(TypeId _type, integer _left, integer _right)
    : Type(_type), derivedRange(NULL), left(_left), right(_right)
{
    assert(isOrdinal());
}


Range* Ordinal::deriveRange()   { DERIVEX(Range) }


Ordinal* Ordinal::deriveSubrange(integer _left, integer _right)
{
    if (rangeEq(_left, _right))
        return this;
    if (!isLe(_left, _right))
        throw ESubrange();
    return new Ordinal(typeId, _left, _right);
}


void Ordinal::runtimeTypecast(variant& v)
{
    if (isBool())
    {
        v = v.to_bool();
        return;
    }
    if (!v.is_ordinal())
        throw EVarTypeMismatch();
    if (!isInRange(v.as_ordinal()))
        throw ERange();
    if (isChar())
        v.type = variant::CHAR;
    else if (isInt() || isEnum())
        v.type = variant::INT;
    else
        fatal(0x3001, "Unknown ordinal type");
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
            owner->addConstant(name, this, variant(values->size()))));
}


Ordinal* Enumeration::deriveSubrange(integer _left, integer _right)
{
    if (_left == left && _right == right)
        return this;
    if (_left < left || _right > right)
        throw ESubrange();
    return new Enumeration(values, _left, _right);
}


// --- Range --------------------------------------------------------------- //


Range::Range(Ordinal* _base): Type(RANGE), base(_base)  { }


// --- Container ---------------------------------------------------------- //


Container::Container(Type* _index, Type* _elem)
    : Type(NONE), index(_index), elem(_elem)
{
    if (index->isNone())
        setTypeId(VECTOR);
    else if (elem->isNone())
        setTypeId(SET);
    else if (index->canBeArrayIndex())
        setTypeId(ARRAY);
    else
        setTypeId(DICT);
}


void Container::runtimeTypecast(variant& v)
{
    if (isString())
    {
        if (!v.is(variant::STR))
            v = v.to_string();
        return;
    }
    if ((elem->isVariant() || elem->isNone())
        && (index->isVariant() || index->isNone()))
            return;
    throw EVarTypeMismatch();
}


// --- Fifo ---------------------------------------------------------------- //


Fifo::Fifo(Type* _elem): Type(FIFO), elem(_elem)  { }


void Fifo::runtimeTypecast(variant& v)
{
    // TODO: Type of an object should be known at run time so that we can do
    // more meaningful typecasts. Same for containers. Meanwhile, only this
    // is possible:
    if (!v.is(variant::FIFO))
        throw EVarTypeMismatch();
    if (isCharFifo() && v.as_fifo()->is_char_fifo())
        return;
    if (isVariantFifo())
        return;
    throw EVarTypeMismatch();
}


// --- Variant ------------------------------------------------------------- //


Variant::Variant(): Type(VARIANT)  { }


// --- TypeReference ------------------------------------------------------- //


TypeReference::TypeReference(): Type(TYPEREF)  { }


// --- QueenBee ------------------------------------------------------------ //


QueenBee::QueenBee()
  : Module("system", mem(-1), NULL),
    defTypeRef(registerType(new TypeReference())),
    defNone(registerType(new None())),
    defInt(registerType(new Ordinal(Type::INT, INTEGER_MIN, INTEGER_MAX))),
    defBool(registerType(new Enumeration(Type::BOOL))),
    defChar(registerType(new Ordinal(Type::CHAR, 0, 255))),
    defStr(NULL),
    defEmptyContainer(NULL),
    defVariant(registerType(new Variant()))
    { }


void QueenBee::setup()
{
    // This can't be done in the constructor while the global object queenBee
    // is not assigned yes, because addTypeAlias() uses defaultTypeRef for all
    // type aliases created.
    defStr = defChar->deriveVector();
    defEmptyContainer = defNone->deriveVector();
    addTypeAlias("typeref", defTypeRef);
    addTypeAlias("none", defNone);
    addConstant("null", defNone, null);
    addTypeAlias("int", defInt);
    defBool->addValue("false");
    defBool->addValue("true");
    addTypeAlias("bool", defBool);
    addTypeAlias("str", defStr);
    addTypeAlias("any", defVariant);
}


objptr<QueenBee> queenBee;

void initTypeSys()
{
    queenBee = new QueenBee();
    queenBee->setup();
}


void doneTypeSys()
{
    queenBee = NULL;
}

