
#include "typesys.h"


// --- Symbols & Scope ----------------------------------------------------- //


Symbol::Symbol(const str& _name, SymbolId _id, Type* _type)
    : symbol(_name), symbolId(_id), type(_type)  { }

Symbol::~Symbol()
    { }

bool Symbol::isTypeAlias() const
    { return isDefinition() && PDefinition(this)->aliasedType() != NULL; }


Definition::Definition(const str& _name, Type* _type, const variant& _value)
    : Symbol(_name, DEFINITION, _type), value(_value) { }

Definition::~Definition()  { }

Type* Definition::aliasedType() const
{
    if (value.is(variant::RTOBJ) && value._rtobj()->type->isTypeRef())
        return cast<Type*>(value._rtobj());
    else
        return NULL;
}


EDuplicate::EDuplicate(const str& _ident): ident(_ident)  { }
EDuplicate::~EDuplicate()  { }
const char* EDuplicate::what() const  { return "Duplicate identifier"; }

EUnknownIdent::EUnknownIdent(const str& _ident): ident(_ident)  { }
EUnknownIdent::~EUnknownIdent()  { }
const char* EUnknownIdent::what() const  { return "Unknown identifier"; }


Scope::Scope(Scope* _outer)
    : outer(_outer)  { }


Scope::~Scope()
{
    defs.release_all();
}


Symbol* Scope::find(const str& ident) const
{
    memint i;
    if (symbols.bsearch(ident, i))
        return symbols[i];
    else
        return NULL;
}


void Scope::addUnique(Symbol* s)
{
    memint i;
    if (symbols.bsearch(s->name, i))
        throw EDuplicate(s->name);
    else
        symbols.insert(i, s);
}


Symbol* Scope::findShallow(const str& ident) const
{
    Symbol* s = find(ident);
    if (s == NULL)
        throw EUnknownIdent(ident);
    return s;
}


Symbol* Scope::findDeep(const str& ident) const
{
    Symbol* s = find(ident);
    if (s != NULL)
        return s;
    for (memint i = uses.size(); i--; )
    {
        s = uses[i]->find(ident);
        if (s != NULL)
            return s;
    }
    if (outer != NULL)
        return outer->findDeep(ident);
    throw EUnknownIdent(ident);
}


Definition* Scope::addDefinition(const str& name, Type* type, const variant& value)
{
    objptr<Definition> d = new Definition(name, type, value);
    addUnique(d); // may throw
    defs.push_back(d->ref<Definition>());
    return d;
}


Definition* Scope::addTypeAlias(const str& name, Type* type)
{
    return addDefinition(name, defTypeRef, type);
}


// --- Type ---------------------------------------------------------------- //


void typeMismatch()
        { throw ecmessage("Type mismatch"); }


template class vector<objptr<Type> >;


Type::Type(TypeId id)
    : rtobject(id == TYPEREF ? this : defTypeRef.get()), typeId(id),
      host(NULL), derivedVec(NULL), derivedSet(NULL)  { }

Type::~Type()
    { }

bool Type::empty() const
    { return false; }

bool Type::isSmallOrd() const
    { return isAnyOrd() && POrdinal(this)->isSmallOrd(); }

bool Type::isBitOrd() const
    { return isAnyOrd() && POrdinal(this)->isBitOrd(); }

bool Type::isString() const
    { return isVec() && PContainer(this)->elem->isChar(); }

bool Type::identicalTo(Type* t) const
    { return t == this; }

bool Type::canConvertTo(Type* t) const
    { return identicalTo(t); }


#define new_Vector(x) (new Container(defNone, x))
#define new_Set(x) (new Container(x, defNone))


Container* Type::deriveVec()
{
    if (isNone())
        throw ecmessage("Invalid vector element type");
    if (derivedVec == NULL)
        derivedVec = host->registerType(new_Vector(this));
    return derivedVec;
}


Container* Type::deriveSet()
{
    if (isNone())
        throw ecmessage("Invalid set element type");
    if (derivedSet == NULL)
        derivedSet = host->registerType(new_Set(this));
    return derivedSet;
}


// --- General Types ------------------------------------------------------- //


TypeReference::TypeReference(): Type(TYPEREF)  { }
TypeReference::~TypeReference()  { }
str TypeReference::definition() const  { return "typeref"; }


None::None(): Type(NONE)  { }
None::~None()  { }
str None::definition() const  { return "none"; }


Variant::Variant(): Type(VARIANT)  { }
Variant::~Variant()  { }
str Variant::definition() const  { return "any"; }


Reference::Reference(Type* _to)
    : Type(REF), to(_to)  { }

Reference::~Reference()
    { }

str Reference::definition() const
    { return to->definition() + "*^"; }

bool Reference::identicalTo(Type* t) const
    { return t->isReference() && to->identicalTo(PReference(t)->to); }

bool Reference::canConvertTo(Type* t) const
    { return t->isReference() && to->canConvertTo(PReference(t)->to); }


// --- Ordinals ------------------------------------------------------------ //


Ordinal::Ordinal(TypeId id, integer l, integer r)
    : Type(id), left(l), right(r)
        { assert(isAnyOrd()); }

Ordinal::~Ordinal()
    { }

Ordinal* Ordinal::_createSubrange(integer l, integer r)
    { return new Ordinal(typeId, l, r); }


Ordinal* Ordinal::createSubrange(integer l, integer r)
{
    if (l == left && r == right)
        return this;
    if (l < left || r > right)
        throw ecmessage("Subrange can't be bigger than original");
    return _createSubrange(l, r);
}


str Ordinal::definition() const
{
    switch(typeId)
    {
    case INT:
        if (left == INTEGER_MIN && right == INTEGER_MAX)
            return "int";
        else
            return to_string(left) + ".." + to_string(right);
    case CHAR:
        if (left == 0 && right == 255)
            return "char";
        else
            return to_quoted(uchar(left)) + ".." + to_quoted(uchar(right));
    default:
        assert(false);
        return "?";
    }
}

bool Ordinal::canConvertTo(Type* t) const
    { return t->is(typeId); }


// --- //


Enumeration::Enumeration(TypeId id)
    : Ordinal(id, 0, -1)  { }

Enumeration::Enumeration(const EnumValues& v, integer l, integer r)
    : Ordinal(ENUM, l, r), values(v)  { }

Enumeration::Enumeration()
    : Ordinal(ENUM, 0, -1)  { }

Enumeration::~Enumeration()
    { }

Ordinal* Enumeration::_createSubrange(integer l, integer r)
    { return new Enumeration(values, l, r); }


void Enumeration::addValue(Scope* scope, const str& ident)
{
    integer n = integer(values.size());
    if (n >= 256)
        throw emessage("Maximum number of enum constants reached");
    Definition* d = scope->addDefinition(ident, this, n);
    values.push_back(d);
    reassignRight(n);
}


str Enumeration::definition() const
{
    if (left > 0 || right < values.size() - 1)
        return values[0]->name + ".." + values[right]->name;
    else
    {
        str result = "enum(";
        for (memint i = 0; i < values.size(); i++)
            result += (i ? ", " : "") + values[i]->name;
        result += ')';
        return result;
    }
}


bool Enumeration::canConvertTo(Type* t) const
    { return t->is(typeId) && values == PEnumeration(t)->values; }


// --- Containers ---------------------------------------------------------- //


Type::TypeId Type::contType(Type* i, Type* e)
{
    if (i->isNone())
        if (e->isNone())
            return NULLCONT;
        else
            return VEC;
    else if (e->isNone())
        return SET;
    else
        return DICT;
}


Container::Container(Type* i, Type* e)
    : Type(contType(i, e)), index(i), elem(e)  { }

Container::~Container()
    { }

str Container::definition() const
{
    if (isSet())
        return index->definition() + "<>";
    else
    {
        str result = elem->definition() + '[';
        if (!isVec())
            result += index->definition();
        result += ']';
        return result;
    }
}


// --- State --------------------------------------------------------------- //


State::State(TypeId _id, State* parent)
    : Type(_id), Scope(parent)  { }

State::~State()
    { selfVars.release_all(); }


// --- Module -------------------------------------------------------------- //


Module::Module(const str& _name)
    : State(MODULE, NULL), name(_name)  { }

Module::~Module()
    { }

str Module::definition() const
    { return name; }

Type* Module::_registerType(Type* t)
    { t->setHost(this); types.push_back(t); return t; }


// --- QueenBee ------------------------------------------------------------ //


QueenBee::QueenBee()
    : Module("system"),
      defVariant(new Variant()),
      defInt(new Ordinal(INT, INTEGER_MIN, INTEGER_MAX)),
      defChar(new Ordinal(CHAR, 0, 255)),
      defBool(new Enumeration(BOOL)),
      defStr(new_Vector(defChar)),
      defNullCont(new Container(defNone, defNone))
{
    addTypeAlias("typeref", registerType(defTypeRef));
    addTypeAlias("none", registerType(defNone));
    addTypeAlias("any", registerType(defVariant));
    addTypeAlias("int", registerType(defInt));
    addTypeAlias("char", registerType(defChar));
    addTypeAlias("bool", registerType(defBool));
    defBool->addValue(this, "false");
    defBool->addValue(this, "true");
    addTypeAlias("str", defChar->derivedVec = registerType(defStr));
    registerType(defNullCont);
}


QueenBee::~QueenBee()
    { }


// --- Globals ------------------------------------------------------------- //


objptr<TypeReference> defTypeRef;
objptr<None> defNone;
objptr<QueenBee> queenBee;


void initTypeSys()
{
    // Because all Type objects are also runtime objects, they all have a
    // runtime type of "type reference". The initial typeref object refers to
    // itself and should be created before anything else in the type system.
    defTypeRef = new TypeReference();
    
    // None is used in deriving vectors and sets, so we need it before some of
    // the default types are created in QueenBee
    defNone = new None();

    // The "system" module that defines default types; some of them have
    // recursion and other kinds of weirdness, and therefore should be defined
    // in C code rather than in Shannon code
    queenBee = new QueenBee();
}


void doneTypeSys()
{
    queenBee.clear();
    defNone.clear();
    defTypeRef.clear();
}

