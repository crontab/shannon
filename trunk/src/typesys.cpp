
#include "version.h"
#include "typesys.h"
#include "vm.h"
#include "parser.h"


// --- LANGUAGE OBJECT ----------------------------------------------------- //


void* langobj::operator new(size_t, mem datasize)
{
    return new char[sizeof(langobj) + datasize * sizeof(variant)];
}


// Defined in runtime.h
langobj::langobj(State* type)
    : object(type)
#ifdef DEBUG
    , varcount(type == NULL ? 0 : type->thisSize())
#endif
{
//    memset(vars, 0, type->dataSize() * sizeof(variant));
}


bool langobj::empty() const { return false; }


void langobj::_idx_err()
{
    throw emessage("Object index error");
}


langobj::~langobj()
{
    if (get_rt() != NULL)
    {
        mem count = CAST(State*, get_rt())->thisSize();
        while (count--)
            vars[count].~variant();
    }
}


// --- BASE LANGUAGE OBJECTS AND COLLECTIONS ------------------------------- //


EDuplicate::EDuplicate(const str& _ident) throw()
    : exception(), ident(_ident)  { }

EDuplicate::~EDuplicate() throw()  { }


Symbol::Symbol(SymbolId _symbolId, Type* _type, const str& _name)
    : object(NULL), symbolId(_symbolId), type(_type), name(_name)  { }

Symbol::~Symbol()
{
#ifdef DEBUG
    (SymbolId&)symbolId = SymbolId(-1);
#endif
}


void Symbol::dump(fifo_intf& stm) const
    { stm << *type << ' ' << name; }

bool Symbol::isTypeAlias() const
    { return isDefinition() && type->isTypeRef(); }

bool Symbol::isStateAlias() const
    { return isTypeAlias()
        && CAST(Type*, PDef(this)->value._obj())->isState(); }

bool Symbol::isModuleAlias() const
    { return isTypeAlias()
        && CAST(Type*, PDef(this)->value._obj())->isModule(); }


_SymbolTable::_SymbolTable()  { }
_SymbolTable::~_SymbolTable()  { }


void _SymbolTable::addUnique(Symbol* o)
{
    std::pair<Impl::iterator, bool> result = impl.insert(Impl::value_type(o->name, o));
    if (!result.second)
        throw EDuplicate(o->name);
}


Symbol* _SymbolTable::find(const str& name) const
{
    Impl::const_iterator i = impl.find(name);
    if (i == impl.end())
        return NULL;
    return i->second;
}


_PtrList::_PtrList()  { }
_PtrList::~_PtrList()  { }
void _PtrList::clear()  { impl.clear(); }
mem _PtrList::add(void* p)  { impl.push_back(p); return size() - 1; }


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


void _List::dump(fifo_intf& stm) const
{
    for (mem i = 0; i < size(); i++)
    {
        if (i > 0)
            stm << ", ";
        stm << *(*this)[i];
    }
}


// --- Variable ----------------------------------------------------------- //


Variable::Variable(SymbolId _symbolId, Type* _type, const str& _name, mem _id, State* _state, bool _readOnly)
    : Symbol(_symbolId, _type, _name), id(_id), state(_state), readOnly(_readOnly)
{
    assert(isVariable());
    if (_id > 255) fatal(0x3002, "Variable ID too big");
}

Variable::~Variable()  { }


// --- Definition ---------------------------------------------------------- //


Definition::Definition(Type* _type, const str& _name, const variant& _value)
    : Symbol(DEFINITION, _type, _name), value(_value) { }

Definition::Definition(const str& _name, Type* _type)
    : Symbol(DEFINITION, defTypeRef, _name), value(_type) { }

Definition::~Definition()  { }

void Definition::dump(fifo_intf& stm) const
{
    if (isModuleAlias())
        stm << name << " = <used-module>";
    else if (isTypeAlias())
    {
        stm << name << " = ";
        aliasedType()->fullDump(stm);
    }
    else
        stm << *type << ' ' << name << " = " << value;
}


// --- TYPE SYSTEM --------------------------------------------------------- //


void typeMismatch()
        { throw emessage("Type mismatch"); }


// Constructor placeholders for the DERIVEX macro
#define new_Fifo(x)     new Fifo(x)
#define new_Vector(x)   new Container(queenBee->defNone, x)
#define new_Set(x)      new Container(x, queenBee->defNone)
#define new_Range(x)    new Range(x)


#define DERIVEX(d) \
    { if (isNone()) throw emessage("Illegal use of none type"); \
      if (derived##d == NULL) \
        derived##d = owner->registerType(new_##d(this)); \
      return derived##d; }


// --- Type ---------------------------------------------------------------- //


Type::Type(Type* rt, TypeId _t)
  : object(rt), typeId(_t), owner(NULL), derivedFifo(NULL),
    derivedVector(NULL), derivedSet(NULL)
{
    assert(rt != NULL);
}

Type::~Type()
{
#ifdef DEBUG
    (TypeId&)typeId = TypeId(-1);
#endif
}


void Type::dump(fifo_intf& stm) const
{
    if (!name.empty())
        stm << name;
    else
        fullDump(stm);
}

void Type::fullDump(fifo_intf& stm) const
    { stm << "<builtin>"; }

bool Type::isModule() const
    { return typeId == STATE && PState(this)->level == 0; }

bool Type::canBeArrayIndex() const
    { return isOrdinal() && POrdinal(this)->rangeFits(MAX_ARRAY_RANGE); }

bool Type::canBeOrdsetIndex() const
    { return isOrdinal() && POrdinal(this)->rangeFits(charset::BITS); }


Fifo* Type::deriveFifo()        { DERIVEX(Fifo) }
Container* Type::deriveVector() { DERIVEX(Vector) }
Container* Type::deriveSet()    { DERIVEX(Set) }


Container* Type::createContainer(Type* indexType)
{
    if (indexType->isNone())
        return deriveVector();
    if (isNone())
        return deriveSet();
    return owner->registerType(new Container(indexType, this));
}


bool Type::identicalTo(Type* t)  // for comparing container elements, indexes
    { return t->is(typeId); }

bool Type::canAssignTo(Type* t)  // can assign or automatically convert the type without changing the value
    { return identicalTo(t); }

bool Type::isMyType(const variant& v)
    { return v.is_obj() && v._obj() != NULL && v._obj()->get_rt()->canAssignTo(this); }

void Type::runtimeTypecast(variant& v)
    { if (!isMyType(v)) typeMismatch(); }


// --- Scope --------------------------------------------------------------- //


EUnknownIdent::EUnknownIdent(const str& _ident) throw()
    : exception(), ident(_ident)  { }
EUnknownIdent::~EUnknownIdent() throw ()  { }


Scope::Scope(Scope* _outer)
    : outer(_outer)  { }
Scope::~Scope()  { }


Symbol* Scope::findShallow(const str& ident) const
{
    Symbol* s = find(ident);
    if (s != NULL)
        return s;
    throw EUnknownIdent(ident);
}


Symbol* Scope::findDeep(const str& ident) const
{
    Symbol* s = find(ident);
    if (s != NULL)
        return s;
    for (mem i = uses.size(); i--; )
    {
        s = uses[i]->find(ident);
        if (s != NULL)
            return s;
    }
    if (outer != NULL)
        return outer->findDeep(ident);
    throw EUnknownIdent(ident);
}


void Scope::addUses(Module* module)
{
    if (uses.size() >= 255)
        throw emessage("Too many used modules");
    assert(!module->getName().empty());
    objptr<ModuleAlias> alias = new ModuleAlias(module->getName(), module);
    addUnique(alias);
    defs.add(alias);
    uses.add(module);
}


Constant* Scope::addConstant(Type* type, const str& name, const variant& value)
{
    objptr<Constant> c = new Constant(type, name, value);
    addUnique(c); // may throw
    defs.add(c);
    return c;
}


TypeAlias* Scope::addTypeAlias(const str& name, Type* type)
{
    objptr<TypeAlias> c = new TypeAlias(name, type);
    addUnique(c); // may throw
    defs.add(c);
    type->setName(name);
    return c;
}


// --- State --------------------------------------------------------------- //


State::State(Module* _module, State* _parent, Type* resultType)
  : Type(defTypeRef, STATE), Scope(_parent),
    CodeSeg(_module, this), startId(0),
    level(_parent == NULL ? 0 : _parent->level + 1),
    selfPtr(_parent == NULL ? this : _parent->selfPtr)
{
    // TODO: functions returning self
    // TODO: multiple result vars (result1, result2, ... ?)
    setOwner(this);
    if (resultType != NULL)
    {
        resultvar = new ResultVar(Symbol::RESULTVAR, resultType, "result", 0, this, false);
        addUnique(resultvar);
    }
}


State::~State()  { }


void State::fullDump(fifo_intf& stm) const
{
    stm << *resultvar << " *(";
    args.dump(stm);
    stm << ')';
}


void State::listing(fifo_intf& stm) const
{
    stm << ";--------- STATE " << name << endl;
    CodeSeg::listing(stm);
    stm << ";--- Definitions in " << name << endl;
    dumpDefinitions(stm);
}


bool State::identicalTo(Type* t)
    { return t == this; }

bool State::canAssignTo(Type* t)
    { return t == this; } // TODO: implement inheritance


langobj* State::newObject()
{
    mem s = thisSize();
    if (s == 0)
        return nullLangObj;
    return new(s) langobj(this);
}


Variable* State::addThisVar(Type* type, const str& _name, bool readOnly)
{
    mem id = startId + thisvars.size(); // startId will be used with derived classes
    if (id >= 255)
        throw emessage("Maximum number of variables within this object reached");
    objptr<Variable> v = new ThisVar(Symbol::THISVAR, type, _name, id, this, readOnly);
    addUnique(v); // may throw
    thisvars.add(v);
    return v;
}


// --- Module -------------------------------------------------------------- //


Module::Module(const str& _name)
    : State(this, NULL, NULL)
{
    setName(_name);
    if (queenBee != NULL)
        addUses(queenBee);
}


Module::~Module()  { }


void Module::fullDump(fifo_intf& stm) const
{
    stm << "module " << name;
}


void State::dumpDefinitions(fifo_intf& stm) const
{
    for (mem i = 0; i < defs.size(); i++)
        stm << "  def " << *defs[i] << endl;
    for (mem i = 0; i < thisvars.size(); i++)
        stm << "  var " << *thisvars[i] << endl;
}


mem Module::registerFileName(const str& fn)
{
    fileNames.push_back(fn);
    return fileNames.size() - 1;
}


void Module::initialize(varstack& stack)
{
    if (instance == NULL)
    {
        instance = newObject();
        CodeSeg::run(stack, instance, NULL);
    }
}


variant Module::run()
{
    try
    {
        varstack stack;
        for (mem i = 0; i < uses.size(); i++)
            uses[i]->initialize(stack);
        initialize(stack);
    }
    catch (eexit&)
    {
        // exit operator called, we are ok with it
    }
    catch (exception&)
    {
        for (mem i = uses.size(); i--; ) // an example of how unsigned ints suck
            uses[i]->finalize();
        throw;
    }
    variant result = *queenBee->instance->var(queenBee->sresultvar->id);
    finalize();
    for (mem i = uses.size(); i--; )
        uses[i]->finalize();
    return result;
}


// --- None ---------------------------------------------------------------- //


None::None(): Type(defTypeRef, NONE)  { }
bool None::isMyType(const variant& v)  { return v.is_null(); }


// --- Ordinal ------------------------------------------------------------- //


Ordinal::Ordinal(TypeId _type, integer _left, integer _right)
    : Type(defTypeRef, _type), derivedRange(NULL), left(_left), right(_right)
{
    assert(isOrdinal());
}

Ordinal::~Ordinal()  { }


void Ordinal::fullDump(fifo_intf& stm) const
{
    if (isChar())
        stm << mkQuotedPrintable(left) << ".." << mkQuotedPrintable(right);
    else
        stm << left << ".." << right;
}


Range* Ordinal::deriveRange()   { DERIVEX(Range) }


mem Ordinal::rangeSize()
{
    if (left > right)
        return 0;
    integer diff = right - left + 1;
    if (diff <= 0)  // overflow, e.g. default int range
        return INTEGER_MAX;
    return diff;
}


Ordinal* Ordinal::createSubrange(integer _left, integer _right)
{
    if (rangeEq(_left, _right))
        return this;
    if (!isLe(_left, _right))
        throw emessage("Subrange error");
    return owner->registerType(new Ordinal(typeId, _left, _right));
}


bool Ordinal::identicalTo(Type* t)
    { return t->is(typeId) && rangeEq(POrdinal(t)); }


bool Ordinal::canAssignTo(Type* t)
    { return t->is(typeId); }


bool Ordinal::isMyType(const variant& v)
{
    switch (v.type)
    {
    case BOOL: return isBool();
    case CHAR: return isChar();
    case INT: return isInt();
    default: return false;
    }
}


void Ordinal::runtimeTypecast(variant& v)
{
    if (isBool())
        v = v.to_bool();
    else
    {
        if (!v.is_ord())
            typeMismatch();
        if (!isInRange(v._ord()))
            throw emessage("Out of range");
    }
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


Enumeration::~Enumeration()  { }


void Enumeration::fullDump(fifo_intf& stm) const
{
    if (left == 0 && right == integer(values->size() - 1))
        stm << (*values)[0]->name << ".." << (*values)[right]->name;
    else
    {
        stm << "enum(";
        for (mem i = 0; i < values->size() - 1; i++)
        {
            if (i > 0)
                stm << ", ";
            stm << (*values)[i]->name;
        }
        stm << ')';
    }
}


void Enumeration::addValue(const str& _name)
{
    integer n = integer(values->size());
    if (n == 256)
        throw emessage("Maximum number of enum constants reached");
    Constant* c;
    if (isBool())
        c = owner->addConstant(this, _name, bool(n));
    else
        c = owner->addConstant(this, _name, n);
    reassignRight(values->add(c));
    assert(right == n);
}


bool Enumeration::identicalTo(Type* t)
    { return this == t; }

bool Enumeration::canAssignTo(Type* t)
    { return t->isEnum() && values == PEnum(t)->values; }


Ordinal* Enumeration::createSubrange(integer _left, integer _right)
{
    if (rangeEq(_left, _right))
        return this;
    if (isBool())
        throw emessage("Can't derive subrange from bool");
    if (_left < left || _right > right)
        throw emessage("Subrange error");
    return owner->registerType(new Enumeration(values, _left, _right));
}


// --- Range --------------------------------------------------------------- //


Range::Range(Ordinal* _base)
    : Type(defTypeRef, RANGE), base(_base)  { }
Range::~Range()  { }


void Range::fullDump(fifo_intf& stm) const
    { stm << *base << " *[..]"; }

bool Range::identicalTo(Type* t)
    { return t->isRange() && base->identicalTo(PRange(t)->base); }

bool Range::canAssignTo(Type* t)
    { return t->isRange() && base->canAssignTo(PRange(t)->base); }


// --- Container ---------------------------------------------------------- //


Container::Container(Type* _index, Type* _elem)
    : Type(defTypeRef, NONE), index(_index), elem(_elem)
{
    if (index->isNone())
    {
        if (elem->isNone())
            throw emessage("Invalid container type");
        setTypeId(elem->isChar() ? STR : VEC);
    }
    else if (elem->isNone())
        setTypeId(index->canBeOrdsetIndex() ? ORDSET : SET);
    else
        setTypeId(index->canBeArrayIndex() ? ARRAY : DICT);
}


Container::~Container()  { }


void Container::fullDump(fifo_intf& stm) const
{
    stm << *elem << " *[";
    if (!index->isNone())
        stm << *index;
    stm << ']';
}


bool Container::identicalTo(Type* t)
    { return (t == this) || (t->is(typeId) && elem->identicalTo(CAST(Container*, t)->elem)
            && index->identicalTo(CAST(Container*, t)->index)); }


void Container::runtimeTypecast(variant& value)
{
    if (isMyType(value))
        return;
    else if (isString())
    {
        if (!value.is(variant::STR))
            value = value.to_string();
    }
    else
        typeMismatch();
}


mem Container::arrayRangeSize()
{
    assert(isArray() && index->isOrdinal() && POrdinal(index)->rangeFits(MAX_ARRAY_RANGE));
    return POrdinal(index)->rangeSize();
}


// --- Fifo ---------------------------------------------------------------- //


Fifo::Fifo(Type* _elem): Type(defTypeRef, NONE), elem(_elem)
    { setTypeId(elem->isChar() ? CHARFIFO : VARFIFO); }

Fifo::~Fifo()
    { }

void Fifo::fullDump(fifo_intf& stm) const
    { stm << *elem << " *<>"; }

bool Fifo::identicalTo(Type* t)
    { return t->is(typeId) && elem->identicalTo(PFifo(t)->elem); }


// --- NullCompound -------------------------------------------------------- //


NullCompound::NullCompound(): Type(defTypeRef, NULLCOMP)  { }
NullCompound::~NullCompound()  { }


// --- Variant ------------------------------------------------------------- //


Variant::Variant(): Type(defTypeRef, VARIANT)  { }
Variant::~Variant()  { }
bool Variant::isMyType(const variant&)  { return true; }
void Variant::runtimeTypecast(variant&)  { }


// --- TypeReference ------------------------------------------------------- //


TypeReference::TypeReference(): Type(this, TYPEREF)  { }
TypeReference::~TypeReference()  { }


// --- QueenBee ------------------------------------------------------------ //


QueenBee::QueenBee()
  : Module("system")
{
    registerType(defTypeRef);
    defNone = registerType(new None());
    defInt = registerType(new Ordinal(Type::INT, INTEGER_MIN, INTEGER_MAX));
    defBool = registerType(new Enumeration(Type::BOOL));
    defChar = registerType(new Ordinal(Type::CHAR, 0, 255));
    defStr = NULL;
    defVariant = registerType(new Variant());
    defCharFifo = NULL;
    defNullComp = registerType(new NullCompound());
    siovar = NULL;
    serrvar = NULL;
}


void QueenBee::setup()
{
    // This can't be done in the constructor while the global object queenBee
    // is not assigned
    defStr = defChar->deriveVector();
    defCharFifo = defChar->deriveFifo();
    addTypeAlias("typeref", defTypeRef);
    addTypeAlias("none", defNone);
    addConstant(defNone, "null", null);
    addTypeAlias("int", defInt);
    defBool->addValue("false");
    defBool->addValue("true");
    addTypeAlias("bool", defBool);
    addTypeAlias("char", defChar);
    addTypeAlias("str", defStr);
    addTypeAlias("any", defVariant);
    siovar = addThisVar(defCharFifo, "sio");
    serrvar = addThisVar(defCharFifo, "serr");
    sresultvar = addThisVar(defVariant, "sresult");
    addConstant(defInt, "__ver_major", SHANNON_VERSION_MAJOR);
    addConstant(defInt, "__ver_minor", SHANNON_VERSION_MINOR);
}


void QueenBee::initialize(varstack& stack)
{
    if (instance == NULL)
    {
        instance = newObject();
//        CodeSeg::run(stack, instance, NULL);
        ::new(instance->var(siovar->id)) variant(&sio);
        ::new(instance->var(serrvar->id)) variant(&serr);
        ::new(instance->var(sresultvar->id)) variant();
    }
}


TypeReference* defTypeRef;
QueenBee* queenBee;
langobj* nullLangObj;

static objptr<TypeReference> _defTypeRef;
static objptr<QueenBee> _queenBee;
static objptr<langobj> _nullLangObj;


void initTypeSys()
{
    // Because all Type objects are also runtime objects, they all have a
    // runtime type of "type reference". The initial typeref object refers to
    // itself and should be created before anything else in the type system.
    _defTypeRef = defTypeRef = new TypeReference();
    _queenBee = queenBee = new QueenBee();
    _nullLangObj = new(0) langobj(NULL);
    queenBee->setup();
    sio.set_rt(queenBee->defCharFifo);
    serr.set_rt(queenBee->defCharFifo);
}


void doneTypeSys()
{
    sio.clear_rt();
    serr.clear_rt();
    _nullLangObj = nullLangObj = NULL;
    _queenBee = queenBee = NULL;
    _defTypeRef = defTypeRef = NULL;
}

