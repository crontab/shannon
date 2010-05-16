
#include "typesys.h"
#include "vm.h"


// --- Symbols & Scope ----------------------------------------------------- //


Symbol::Symbol(const str& _name, SymbolId _id, Type* _type)
    : symbol(_name), symbolId(_id), type(_type)  { }


Symbol::~Symbol()
    { }


bool Symbol::isTypeAlias() const
    { return isDefinition() && PDefinition(this)->getAliasedType() != NULL; }


// --- //


Definition::Definition(const str& _name, Type* _type, const variant& _value)
    : Symbol(_name, DEFINITION, _type), value(_value) { }


Definition::~Definition()
    { }


Type* Definition::getAliasedType() const
{
    if (type->isTypeRef())
        return cast<Type*>(value._rtobj());
    else
        return NULL;
}


// --- //


Variable::Variable(const str& _name, SymbolId _sid, Type* _type, memint _id, State* _state)
    : Symbol(_name, _sid, _type), id(_id), state(_state)  { }


Variable::~Variable()
    { }


// --- //


EDuplicate::EDuplicate(const str& _ident) throw(): ident(_ident)  { }
EDuplicate::~EDuplicate() throw()  { }
const char* EDuplicate::what() throw()  { return "Duplicate identifier"; }

EUnknownIdent::EUnknownIdent(const str& _ident) throw(): ident(_ident)  { }
EUnknownIdent::~EUnknownIdent() throw()  { }
const char* EUnknownIdent::what() throw()  { return "Unknown identifier"; }


// --- //


Scope::Scope(Scope* _outer)
    : outer(_outer)  { }


Scope::~Scope()
    { }


void Scope::addUnique(Symbol* s)
{
    if (!symbols.add(s))
        throw EDuplicate(s->name);
}


Symbol* Scope::findShallow(const str& ident) const
{
    Symbol* s = find(ident);
    if (s == NULL)
        throw EUnknownIdent(ident);
    return s;
}

/*
Symbol* Scope::findDeep(const str& ident) const
{
    Symbol* s = find(ident);
    if (s != NULL)
        return s;
    if (outer != NULL)
        return outer->findDeep(ident);
    throw EUnknownIdent(ident);
}
*/

// --- //


BlockScope::BlockScope(Scope* _outer, CodeGen* _gen)
    : Scope(_outer), startId(_gen->getLocals()), gen(_gen)  { }


BlockScope::~BlockScope()
    { localVars.release_all(); }


void BlockScope::deinitLocals()
{
    for (memint i = localVars.size(); i--; )
        gen->deinitLocalVar(localVars[i]);
}


Variable* BlockScope::addLocalVar(const str& name, Type* type)
{
    memint id = startId + localVars.size();
    if (id >= 127)
        throw ecmessage("Maximum number of local variables reached");
    objptr<Variable> v = new Variable(name, Symbol::LOCALVAR, type, id, gen->getState());
    addUnique(v);   // may throw
    localVars.push_back(v);
    return v;
}


// --- Type ---------------------------------------------------------------- //


void typeMismatch()
    { throw ecmessage("Type mismatch"); }


Type::Type(Type* t, TypeId id)
    : rtobject(t), refType(NULL), host(NULL), typeId(id)
        { if (id != REF) refType = new Reference(this); }


Type::~Type()
    { }


bool Type::empty() const
    { return false; }


bool Type::isSmallOrd() const
    { return isAnyOrd() && POrdinal(this)->isSmallOrd(); }


bool Type::isBitOrd() const
    { return isAnyOrd() && POrdinal(this)->isBitOrd(); }


bool Type::isOrdVec() const
    { return isVec() && PContainer(this)->hasSmallElem(); }


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


Container* Type::deriveVec()
{
    if (isNone())
        return queenBee->defNullCont;
    else if (isChar())
        return queenBee->defStr;
    else
        return new Container(defNone, this);
}


Container* Type::deriveSet()
{
    if (isNone())
        return queenBee->defNullCont;
    else if (isChar())
        return queenBee->defCharSet;
    else
        return new Container(this, defNone);
}


Container* Type::deriveContainer(Type* idx)
{
    if (isNone())
        return idx->deriveSet();
    else if (idx->isNone())
        return deriveVec();
    else
        return new Container(idx, this);
}


Fifo* Type::deriveFifo()
{
    if (isChar())
        return queenBee->defCharFifo;
    else
        return new Fifo(this);
}


// --- General Types ------------------------------------------------------- //


TypeReference::TypeReference(): Type(this, TYPEREF)  { }
TypeReference::~TypeReference()  { }


None::None(): Type(defTypeRef, NONE)  { }
None::~None()  { }


Variant::Variant(): Type(defTypeRef, VARIANT)  { }
Variant::~Variant()  { }


Reference::Reference(Type* _to)
    : Type(defTypeRef, REF), to(_to)  { }


Reference::~Reference()
    { }


str Reference::definition(const str& ident) const
    { return to->definition(ident) + '^'; }


bool Reference::identicalTo(Type* t) const
    { return this == t || (t->isReference()
        && to->identicalTo(PReference(t)->to)); }


// --- Ordinals ------------------------------------------------------------ //


Ordinal::Ordinal(TypeId id, integer l, integer r)
    : Type(defTypeRef, id), left(l), right(r)
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


str Ordinal::definition(const str&) const
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
    { return this == t || (t->typeId == typeId
        && left == POrdinal(t)->left && right == POrdinal(t)->right); }


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


str Enumeration::definition(const str&) const
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
    : Type(defTypeRef, contType(i, e)), index(i), elem(e)  { }


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
    return this == t || (t->isAnyCont()
        && elem->identicalTo(PContainer(t)->elem)
        && index->identicalTo(PContainer(t)->index));
}


// --- Fifo ---------------------------------------------------------------- //


Fifo::Fifo(Type* e)
    : Type(defTypeRef, FIFO), elem(e)  { }


Fifo::~Fifo()
    { }


bool Fifo::identicalTo(Type* t) const
    { return this == t || (t->isFifo() && elem->identicalTo(PFifo(t)->elem)); }


// TODO: definition()


// --- Prototype ----------------------------------------------------------- //


Prototype::Prototype(Type* r)
    : Type(defTypeRef, PROTOTYPE), returnType(r)  { }


Prototype::~Prototype()
    { args.release_all(); }


bool Prototype::identicalTo(Type* t) const
    { return t->isPrototype() && identicalTo(PPrototype(t)); }


bool Prototype::identicalTo(Prototype* t) const
{
    if (this == t)
        return true;
    if (!returnType->identicalTo(t->returnType)
        || args.size() != t->args.size())
            return false;
    for (memint i = args.size(); i--; )
        if (!args[i]->type->identicalTo(t->args[i]->type))
            return false;
    return true;
}


// TODO: definition()


// --- State --------------------------------------------------------------- //

// State is a type and at the same time is a prototype object. That's why the
// runtime type of a State object is not TypeRef* like all other types, but
// Prototype* (actually it's the State constructor's prototype)

State::State(TypeId _id, Prototype* proto, State* parent, State* self)
    : Type(proto, _id), Scope(parent), selfPtr(self),
      prototype(proto), codeseg(new CodeSeg(this))  { }


State::~State()
{
    selfVars.release_all();
    defs.release_all();
    types.release_all();
}


Type* State::_registerType(Type* t)
    { return _registerType(str(), t); }


Type* State::_registerType(const str& n, Type* t)
{
    if (t->host == NULL)
    {
        types.push_back(t->grab<Type>());
        t->alias = n;
        t->host = this;
    }
    return t;
}


Definition* State::addDefinition(const str& n, Type* t, const variant& v)
{
    if (n.empty())
        fatal(0x3001, "Internal: empty identifier");
    objptr<Definition> d = new Definition(n, t, v);
    addUnique(d); // may throw
    defs.push_back(d->grab<Definition>());
    return d;
}


Definition* State::addTypeAlias(const str& n, Type* t)
    { return addDefinition(n, t->getType(), t); }


Variable* State::addSelfVar(const str& n, Type* t)
{
    if (n.empty())
        fatal(0x3002, "Internal: empty identifier");
    memint id = selfVarCount();
    if (id >= 127)
        throw ecmessage("Too many variables");
    objptr<Variable> v = new Variable(n, Symbol::SELFVAR, t, id, this);
    addUnique(v);
    selfVars.push_back(v->grab<Variable>());
    return v;
}


ModuleVar* State::addModuleVar(const str& n, Module* t)
{
    memint id = selfVarCount();
    if (id >= 127)
        throw ecmessage("Too many variables");
    objptr<ModuleVar> v = new ModuleVar(n, t, id, this);
    selfVars.push_back(v->grab<ModuleVar>());
    return v;
}


stateobj* State::newInstance()
{
    memint varcount = selfVarCount();
    if (varcount == 0)
        return NULL;
    stateobj* obj = new(varcount * sizeof(variant)) stateobj(this);
#ifdef DEBUG
    obj->varcount = varcount;
#endif
    return obj;
}


// TODO: definition()


// --- Module -------------------------------------------------------------- //


Module::Module(const str& name)
    : State(MODULE, defPrototype, NULL, this), complete(false)
        { alias = name; }


Module::~Module()
    { }


void Module::addUses(const str& name, Module* m)
    { uses.push_back(addModuleVar(name, m)); }


void Module::registerString(str& s)
{
    memint i;
    if (constStrings.bsearch(s, i))
        s = constStrings[i];
    else
        constStrings.insert(i, s);
}


// TODO: definition()


// --- //


ModuleVar::ModuleVar(const str& n, Module* m, memint _id, State* s)
    : Variable(n, SELFVAR, m, _id, s)  { }


ModuleVar::~ModuleVar()  { }


// --- //


// --- QueenBee ------------------------------------------------------------ //


QueenBee::QueenBee()
    : Module("system"),
      defVariant(new Variant()),
      defInt(new Ordinal(Type::INT, INTEGER_MIN, INTEGER_MAX)),
      defChar(new Ordinal(Type::CHAR, 0, 255)),
      defBool(new Enumeration(Type::BOOL)),
      defNullCont(new Container(defNone, defNone)),
      defStr(new Container(defNone, defChar)),
      defCharSet(new Container(defChar, defNone)),
      defCharFifo(new Fifo(defChar))
{
    // Fundamentals
    addTypeAlias("type", registerType<Type>("type", defTypeRef));
    addTypeAlias("none", registerType<Type>("none", defNone));
    registerType<Type>(defPrototype);
    addDefinition("null", defNone, variant::null);
    addTypeAlias("any", registerType("any", defVariant));
    addTypeAlias("int", registerType("int", defInt));
    addTypeAlias("char", registerType("char", defChar));
    addTypeAlias("bool", registerType("bool", defBool));
    defBool->addValue(this, "false");
    defBool->addValue(this, "true");
    registerType(defNullCont);
    addTypeAlias("str", registerType("str", defStr));
    addTypeAlias("charset", registerType("charset", defCharSet));
    addTypeAlias("charfifo", registerType("charfifo", defCharFifo));

    // Constants
    addDefinition("__VER_MAJOR", defInt, SHANNON_VERSION_MAJOR);
    addDefinition("__VER_MINOR", defInt, SHANNON_VERSION_MINOR);
    addDefinition("__VER_FIX", defInt, SHANNON_VERSION_FIX);

    // Variables
    resultVar = addSelfVar("__program_result", defVariant);
    sioVar = addSelfVar("sio", defCharFifo);
    serrVar = addSelfVar("serr", defCharFifo);
}


QueenBee::~QueenBee()
    { }


stateobj* QueenBee::newInstance()
{
    stateobj* inst = parent::newInstance();
    sio.setType(defCharFifo);
    serr.setType(defCharFifo);
    inst->var(sioVar->id) = &sio;
    inst->var(serrVar->id) = &serr;
    codeseg->close();
    setComplete();
    return inst;
}


// --- Globals ------------------------------------------------------------- //


objptr<TypeReference> defTypeRef;
objptr<None> defNone;
objptr<Prototype> defPrototype;
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

    // This is a function prototype with no arguments and None return type,
    // used as a prototype for module constructors
    defPrototype = new Prototype(defNone);

    // The "system" module that defines default types; some of them have
    // recursive definitions and other kinds of weirdness, and therefore should
    // be defined in C code rather than in Shannon code
    queenBee = new QueenBee();
}


void doneTypeSys()
{
    queenBee = NULL;
    defPrototype = NULL;
    defNone = NULL;
    defTypeRef = NULL;
}

