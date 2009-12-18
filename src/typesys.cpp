
#include "typesys.h"
#include "vm.h"


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
    if (value.is(variant::RTOBJ) && value._rtobj()->getType()->isTypeRef())
        return cast<Type*>(value._rtobj());
    else
        return NULL;
}


Variable::Variable(const str& _name, SymbolId _sid, Type* _type, memint _id, State* _state)
    : Symbol(_name, _sid, _type), id(_id), state(_state)  { }

Variable::~Variable()
    { }


EDuplicate::EDuplicate(const str& _ident): ident(_ident)  { }
EDuplicate::~EDuplicate()  { }
const char* EDuplicate::what() const  { return "Duplicate identifier"; }

EUnknownIdent::EUnknownIdent(const str& _ident): ident(_ident)  { }
EUnknownIdent::~EUnknownIdent()  { }
const char* EUnknownIdent::what() const  { return "Unknown identifier"; }


Scope::Scope(Scope* _outer)
    : outer(_outer)  { }


Scope::~Scope()
    { }


Symbol* Scope::find(const str& ident) const
{
    memint i;
    if (symbols.bsearch(ident, i))
        return cast<Symbol*>(symbols[i]);
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


// --- Type ---------------------------------------------------------------- //


void typeMismatch()
        { throw ecmessage("Type mismatch"); }


Type::Type(TypeId id)
    : rtobject(id == TYPEREF ? this : defTypeRef.get()), refType(NULL), typeId(id)
{
    if (id != REF)
        refType = new Reference(this);
}

Type::~Type()
    { }

bool Type::empty() const
    { return false; }

bool Type::isSmallOrd() const
    { return isAnyOrd() && POrdinal(this)->isSmallOrd(); }

bool Type::isBitOrd() const
    { return isAnyOrd() && POrdinal(this)->isBitOrd(); }

bool Type::identicalTo(Type* t) const
    { return t == this; }

bool Type::canAssignTo(Type* t) const
    { return identicalTo(t); }


str Type::definition(const str& ident) const
{
    assert(!alias.empty());
    str result = alias;
    if (!ident.empty())
        result += ' ' + ident;
    return result;
}


#define new_Vector(x) (new Container(defNone, x))
#define new_Set(x) (new Container(x, defNone))


// --- General Types ------------------------------------------------------- //


TypeReference::TypeReference(): Type(TYPEREF)  { }
TypeReference::~TypeReference()  { }


None::None(): Type(NONE)  { }
None::~None()  { }


Variant::Variant(): Type(VARIANT)  { }
Variant::~Variant()  { }


Reference::Reference(Type* _to)
    : Type(REF), to(_to)  { }

Reference::~Reference()
    { }

str Reference::definition(const str& ident) const
    { return to->definition(ident) + '^'; }

bool Reference::identicalTo(Type* t) const
    { return t->isReference() && to->identicalTo(PReference(t)->to); }


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


str Ordinal::definition(const str& ident) const
{
    switch(typeId)
    {
    case INT:
        return to_string(left) + ".." + to_string(right);
    case CHAR:
        return to_quoted(uchar(left)) + ".." + to_quoted(uchar(right));
        break;
    default: return "?"; break;
    }
}


bool Ordinal::identicalTo(Type* t) const
    { return t->typeId == typeId
        && left == POrdinal(t)->left && right == POrdinal(t)->right; }


bool Ordinal::canAssignTo(Type* t) const
    { return t->typeId == typeId; }


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


void Enumeration::addValue(State* state, const str& ident)
{
    integer n = integer(values.size());
    if (n >= 256)
        throw emessage("Maximum number of enum constants reached");
    Definition* d = state->addDefinition(ident, this, n);
    values.push_back(d);
    reassignRight(n);
}


str Enumeration::definition(const str& ident) const
{
    str result;
    if (left > 0 || right < values.size() - 1)
        result = values[0]->name + ".." + values[memint(right)]->name;
    else
    {
        result = "enum(";
        for (memint i = 0; i < values.size(); i++)
            result += (i ? ", " : "") + values[i]->name;
        result += ')';
    }
    return result;
}


bool Enumeration::identicalTo(Type* t) const
    { return this == t; }


bool Enumeration::canAssignTo(Type* t) const
    { return t->typeId == typeId && values == PEnumeration(t)->values; }


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

str Container::definition(const str& ident) const
{
    str result = elem->definition(ident) + '[';
    if (!isVec())
        result += index->definition("");
    result += ']';
    return result;
}

bool Container::identicalTo(Type* t) const
{
    return t->isAnyCont() && elem->identicalTo(PContainer(t)->elem)
        && index->identicalTo(PContainer(t)->index);
}


// --- State --------------------------------------------------------------- //


State::State(TypeId _id, State* parent)
    : Type(_id), Scope(parent)  { }


State::~State()
{
    selfVars.release_all();
    defs.release_all();
    types.release_all();
}


Type* State::_registerType(Type* t)
    { types.push_back(t->ref<Type>()); return t; }


Container* State::registerContainer(Type* e, Type* i)
    { return registerType(new Container(e, i)); }


Definition* State::addDefinition(const str& n, Type* t, const variant& v)
{
    objptr<Definition> d = new Definition(n, t, v);
    addUnique(d); // may throw
    defs.push_back(d->ref<Definition>());
    return d;
}


Definition* State::addTypeAlias(const str& n, Type* t)
{
    if (n.empty())
        fatal(0x3001, "Internal: empty identifier");
    if (t->getAlias().empty())
        t->setAlias(n);
    return addDefinition(n, defTypeRef, t);
}


// --- Module -------------------------------------------------------------- //


Module::Module(const str& _name)
    : State(MODULE, NULL)
{
    setAlias(_name);
    if (queenBee != NULL)
        addUses(queenBee);
}

Module::~Module()
    { }


void Module::addUses(Module* module)
{
    if (uses.size() >= 255)
        throw ecmessage("Too many used modules");
    addTypeAlias(module->getAlias(), module);
    uses.push_back(module);
}


void Module::registerString(str& s)
{
    memint i;
    if (constStrings.bsearch(s, i))
        s = constStrings[i];
    else
        constStrings.insert(i, s);
}


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
    addDefinition("null", defNone, variant::null);
    addTypeAlias("any", registerType(defVariant));
    addTypeAlias("int", registerType(defInt));
    addTypeAlias("char", registerType(defChar));
    addTypeAlias("bool", registerType(defBool));
    defBool->addValue(this, "false");
    defBool->addValue(this, "true");
    addTypeAlias("str", registerType(defStr));
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

