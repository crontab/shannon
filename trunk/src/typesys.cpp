
#include "typesys.h"
#include "vm.h"


// --- Symbols & Scope ----------------------------------------------------- //


Symbol::Symbol(const str& _name, SymbolId _id, Type* _type) throw()
    : symbol(_name), symbolId(_id), type(_type)  { }


Symbol::~Symbol() throw()
    { }


bool Symbol::isTypeAlias() const
    { return isDefinition() && PDefinition(this)->getAliasedType() != NULL; }


// --- //


Definition::Definition(const str& _name, Type* _type, const variant& _value) throw()
    : Symbol(_name, DEFINITION, _type), value(_value) { }


Definition::~Definition() throw()
    { }


Type* Definition::getAliasedType() const
{
    if (value.is(variant::RTOBJ) && value._rtobj()->getType()->isTypeRef())
        return cast<Type*>(value._rtobj());
    else
        return NULL;
}


// --- //


Variable::Variable(const str& _name, SymbolId _sid, Type* _type, memint _id, State* _state) throw()
    : Symbol(_name, _sid, _type), id(_id), state(_state)  { }


Variable::~Variable() throw()
    { }


// --- //


EDuplicate::EDuplicate(const str& _ident) throw(): ident(_ident)  { }
EDuplicate::~EDuplicate() throw()  { }
const char* EDuplicate::what() const throw()  { return "Duplicate identifier"; }

EUnknownIdent::EUnknownIdent(const str& _ident) throw(): ident(_ident)  { }
EUnknownIdent::~EUnknownIdent() throw()  { }
const char* EUnknownIdent::what() const throw()  { return "Unknown identifier"; }


// --- //


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
    if (outer != NULL)
        return outer->findDeep(ident);
    throw EUnknownIdent(ident);
}


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


Type::Type(TypeId id) throw()
    : rtobject(id == TYPEREF ? this : defTypeRef.get()), refType(NULL), typeId(id)
        { if (id != REF) refType = new Reference(this); }


Type::~Type() throw()
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


Container* Type::deriveDict(Type* elem)
{
    if (isNone())
        return elem->deriveVec();
    else if (elem->isNone())
        return deriveSet();
    else
        return new Container(this, elem);
}


// --- General Types ------------------------------------------------------- //


TypeReference::TypeReference() throw(): Type(TYPEREF)  { }
TypeReference::~TypeReference() throw()  { }


None::None() throw(): Type(NONE)  { }
None::~None() throw()  { }


Variant::Variant() throw(): Type(VARIANT)  { }
Variant::~Variant() throw()  { }


Reference::Reference(Type* _to) throw()
    : Type(REF), to(_to)  { }


Reference::~Reference() throw()
    { }


str Reference::definition(const str& ident) const
    { return to->definition(ident) + '^'; }


bool Reference::identicalTo(Type* t) const
    { return this == t || (t->isReference()
        && to->identicalTo(PReference(t)->to)); }


// --- Ordinals ------------------------------------------------------------ //


Ordinal::Ordinal(TypeId id, integer l, integer r) throw()
    : Type(id), left(l), right(r)
        { assert(isAnyOrd()); }


Ordinal::~Ordinal() throw()
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
    { return this == t || (t->typeId == typeId
        && left == POrdinal(t)->left && right == POrdinal(t)->right); }


bool Ordinal::canAssignTo(Type* t) const
    { return t->typeId == typeId; }


// --- //


Enumeration::Enumeration(TypeId id) throw()
    : Ordinal(id, 0, -1)  { }


Enumeration::Enumeration(const EnumValues& v, integer l, integer r) throw()
    : Ordinal(ENUM, l, r), values(v)  { }


Enumeration::Enumeration() throw()
    : Ordinal(ENUM, 0, -1)  { }


Enumeration::~Enumeration() throw()
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


Container::Container(Type* i, Type* e) throw()
    : Type(contType(i, e)), index(i), elem(e)  { }


Container::~Container() throw()
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


Fifo::Fifo(Type* e) throw()
    : Type(FIFO), elem(e)  { }


Fifo::~Fifo() throw()
    { }


bool Fifo::identicalTo(Type* t) const
    { return this == t || (t->isFifo() && elem->identicalTo(PFifo(t)->elem)); }


// --- State --------------------------------------------------------------- //


State::State(TypeId _id, State* parent) throw()
    : Type(_id), Scope(parent)  { }


State::~State() throw()
{
    selfVars.release_all();
    defs.release_all();
    types.release_all();
}


Type* State::_registerType(Type* t)
    { types.push_back(t->ref<Type>()); return t; }


Definition* State::addDefinition(const str& n, Type* t, const variant& v)
{
    if (n.empty())
        fatal(0x3001, "Internal: empty identifier");
    objptr<Definition> d = new Definition(n, t, v);
    addUnique(d); // may throw
    defs.push_back(d->ref<Definition>());
    return d;
}


Definition* State::addTypeAlias(const str& n, Type* t)
{
    if (t->getAlias().empty())
        t->setAlias(n);
    return addDefinition(n, defTypeRef, t);
}


Variable* State::addSelfVar(const str& n, Type* t)
{
    if (n.empty())
        fatal(0x3002, "Internal: empty identifier");
    memint id = selfVarCount();
    if (id >= 127)
        throw ecmessage("Too many variables");
    objptr<Variable> v = new Variable(n, Symbol::SELFVAR, t, id, this);
    selfVars.push_back(v->ref<Variable>());
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


// --- //


StateDef::StateDef(State* s, CodeSeg* c) throw()
    : Definition(s->getAlias(), s, cast<rtobject*>(c))  { }


StateDef::~StateDef() throw()
    { }


CodeSeg* StateDef::getCodeSeg() const
    { return cast<CodeSeg*>(value._rtobj()); }


// --- Module -------------------------------------------------------------- //


Module::Module(const str& _name) throw()
    : State(MODULE, NULL) { setAlias(_name); }


Module::~Module() throw()
    { }


Symbol* Module::findDeep(const str& ident) const
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
    throw EUnknownIdent(ident);
}


void Module::addUses(Module* module, CodeSeg* codeseg)
{
    if (uses.size() >= 255)
        throw ecmessage("Too many used modules");
    addDefinition(module->getAlias(), module, cast<rtobject*>(codeseg));
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


// --- //


ModuleDef::ModuleDef(Module* m, CodeSeg* s) throw()
    : StateDef(m, s), module(m), instance(m->newInstance())  { }


ModuleDef::ModuleDef(const str& n) throw()
    : StateDef(new Module(n), new CodeSeg(getStateType())),
      module(cast<Module*>(getStateType())), instance(module->newInstance())  { }


ModuleDef::~ModuleDef() throw()
    { }


// --- QueenBee ------------------------------------------------------------ //


QueenBee::QueenBee() throw()
    : Module("system"),
      defVariant(new Variant()),
      defInt(new Ordinal(INT, INTEGER_MIN, INTEGER_MAX)),
      defChar(new Ordinal(CHAR, 0, 255)),
      defBool(new Enumeration(BOOL)),
      defNullCont(new Container(defNone, defNone)),
      defStr(new Container(defNone, defChar)),
      defCharSet(new Container(defChar, defNone)),
      defCharFifo(new Fifo(defChar))
{
    // Fundamentals
    addTypeAlias("typeref", registerType(defTypeRef));
    addTypeAlias("none", registerType(defNone));
    addDefinition("null", defNone, variant::null);
    addTypeAlias("any", registerType(defVariant));
    addTypeAlias("int", registerType(defInt));
    addTypeAlias("char", registerType(defChar));
    addTypeAlias("bool", registerType(defBool));
    defBool->addValue(this, "false");
    defBool->addValue(this, "true");
    registerType(defNullCont);
    addTypeAlias("str", registerType(defStr));
    addTypeAlias("charset", registerType(defCharSet));
    addTypeAlias("charfifo", registerType(defCharFifo));

    // Constants
    addDefinition("__version_major", defInt, SHANNON_VERSION_MAJOR);
    addDefinition("__version_minor", defInt, SHANNON_VERSION_MINOR);
    addDefinition("__version_fix", defInt, SHANNON_VERSION_FIX);

    // Variables
    addSelfVar("__program_result", defVariant);
}


QueenBee::~QueenBee() throw()
    { }


// --- Globals ------------------------------------------------------------- //


objptr<TypeReference> defTypeRef;
objptr<None> defNone;
objptr<QueenBee> queenBee;
objptr<ModuleDef> queenBeeDef;


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
    // recursive definitions and other kinds of weirdness, and therefore should
    // be defined in C code rather than in Shannon code
    queenBee = new QueenBee();
    queenBeeDef = new ModuleDef(queenBee, new CodeSeg(queenBee));

    sio._type = queenBee->defCharFifo;
    serr._type = queenBee->defCharFifo;
}


void doneTypeSys()
{
    queenBeeDef.clear();
    queenBee.clear();
    defNone.clear();
    defTypeRef.clear();
}

