
#include "typesys.h"
#include "vm.h"


// --- Symbols & Scope ----------------------------------------------------- //


Symbol::Symbol(const str& n, SymbolId id, Type* t, State* h)
    : symbol(n), symbolId(id), type(t), host(h)  { }


Symbol::~Symbol()
    { }


void Symbol::fqName(fifo& stm) const
{
    if (host)
    {
        host->fqName(stm);
        stm << '.';
    }
    stm << name;
}


void Symbol::dump(fifo& stm) const
{
    type->dump(stm);
    stm << ' ' << name;
}


bool Symbol::isTypeAlias() const
    { return isDefinition() && PDefinition(this)->getAliasedType() != NULL; }


// --- //


Definition::Definition(const str& n, Type* t, const variant& v, State* h)
    : Symbol(n, DEFINITION, t, h), value(v) { }


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


Variable::Variable(const str& n, SymbolId sid, Type* t, memint i, State* h)
    : Symbol(n, sid, t, h), id(i)  { }


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
    localVars.push_back(v->grab<Variable>());
    return v;
}


// --- Type ---------------------------------------------------------------- //


// void typeMismatch()
//     { throw ecmessage("Type mismatch"); }


Type::Type(TypeId id)
    : rtobject(id == TYPEREF ? this : defTypeRef), refType(NULL), host(NULL), def(NULL), typeId(id)
        { if (id != REF) refType = new Reference(this); }


Type::~Type()
    { }


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


void Type::_dump(fifo& stm) const
{
    assert(def != NULL);
    stm << def->name;
}


void Type::dump(fifo& stm) const
{
    if (def != NULL)
        stm << def->name;  // TODO: prefixed by scope?
    else
        dumpDefinition(stm);
}


void Type::dumpDefinition(fifo& stm) const
{
    _dump(stm);
    if (!isReference())
        stm << '^';
}


Container* Type::deriveVec()
{
    if (isNone())
        return queenBee->defNullCont;
    else if (isChar())
        return queenBee->defStr;
    else
        return new Container(defVoid, this);
}


Container* Type::deriveSet()
{
    if (isNone())
        return queenBee->defNullCont;
    else if (isChar())
        return queenBee->defCharSet;
    else
        return new Container(this, defVoid);
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


TypeReference::TypeReference(): Type(TYPEREF)  { }
TypeReference::~TypeReference()  { }


Void::Void(): Type(VOID)  { }
Void::~Void()  { }


Variant::Variant(): Type(VARIANT)  { }
Variant::~Variant()  { }


Reference::Reference(Type* _to)
    : Type(REF), to(_to)  { }


Reference::~Reference()
    { }


void Reference::_dump(fifo& stm) const
    { to->_dump(stm); }  // Type::dumpDefinition() takes care of the '^' symbol for non-refs


bool Reference::identicalTo(Type* t) const
    { return this == t || (t->isReference()
        && to->identicalTo(PReference(t)->to)); }


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


void Ordinal::_dump(fifo& stm) const
{
    switch(typeId)
    {
    case INT:
        stm << to_string(left) << ".." << to_string(right);
        break;
    case CHAR:
        stm << to_quoted(uchar(left)) << ".." << to_quoted(uchar(right));
        break;
    default: stm << "<?>"; break;
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
    if (n >= 256)  // TODO: maybe this is not really necessary
        throw emessage("Maximum number of enum constants reached");
    Definition* d = state->addDefinition(ident, this, n);
    values.push_back(d);
    reassignRight(n);
}


void Enumeration::_dump(fifo& stm) const
{
    if (left > 0 || right < values.size() - 1)  // subrange?
        stm << values[0]->name << ".." << values[memint(right)]->name;
    else
    {
        stm << "(enum ";
        for (memint i = 0; i < values.size(); i++)
            stm << (i ? ", " : "") << values[i]->name;
        stm << ')';
    }
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


void Container::_dump(fifo& stm) const
{
    elem->dump(stm);
    stm << '[';
    if (!isVec())
        index->dump(stm);
    stm << ']';
}


bool Container::identicalTo(Type* t) const
{
    return this == t || (t->isAnyCont()
        && elem->identicalTo(PContainer(t)->elem)
        && index->identicalTo(PContainer(t)->index));
}


// --- Fifo ---------------------------------------------------------------- //


Fifo::Fifo(Type* e)
    : Type(FIFO), elem(e)  { }


Fifo::~Fifo()
    { }


void Fifo::_dump(fifo& stm) const
    { elem->dump(stm); stm << "<>"; }


bool Fifo::identicalTo(Type* t) const
    { return this == t || (t->isFifo() && elem->identicalTo(PFifo(t)->elem)); }


// --- Prototype ----------------------------------------------------------- //


Prototype::Prototype(Type* r)
    : Type(PROTOTYPE), returnType(r)  { }


Prototype::~Prototype()
    { args.release_all(); }


void Prototype::_dump(fifo& stm) const
{
    returnType->dump(stm);
    stm << '(';
    for (int i = 0; i < args.size(); i++)
    {
        if (i)
            stm << ", ";
        args[i]->dump(stm);
    }
    stm << ')';
}


bool Prototype::identicalTo(Type* t) const
    { return this == t || (t->isPrototype() && identicalTo(PPrototype(t))); }


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


// --- State --------------------------------------------------------------- //


State::State(TypeId id, Prototype* proto, const str& n, State* par, State* self)
    : Type(id), Scope(parent), name(n), parent(par), selfPtr(self),
      prototype(proto), codeseg(new CodeSeg(this))  { }


State::~State()
{
    selfVars.release_all();
    defs.release_all();
    types.release_all();
}


void State::fqName(fifo& stm) const
{
    if (parent)
    {
        parent->fqName(stm);
        stm << '.';
    }
    if (name.empty())
        stm << '*';
    else
        stm << name;
}


void State::_dump(fifo&) const
{
    // TODO: 
}


Type* State::_registerType(Type* t, Definition* d)
{
    if (t->host == NULL)
    {
        types.push_back(t->grab<Type>());
        t->host = this;
        if (d)
            t->def = d;
        // Also register the bundled reference type, or in case this is a reference,
        // register its bundled value type.
        if (isReference())
            _registerType(t->getValueType(), NULL);
        else
            _registerType(t->getRefType(), NULL);
    }
    return t;
}


Definition* State::addDefinition(const str& n, Type* t, const variant& v)
{
    if (n.empty())
        fatal(0x3001, "Internal: empty identifier");
    objptr<Definition> d = new Definition(n, t, v, this);
    addUnique(d); // may throw
    defs.push_back(d->grab<Definition>());
    if (t->isTypeRef())
    {
        // In case this def is a type definition, also register the type with this state,
        // and bind the type object to this def for better diagnostic output (dump() family).
        assert(v.as_rtobj()->getType()->isTypeRef());
        _registerType(cast<Type*>(v._rtobj()), d);
    }
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


// TODO: identicalTo()


// --- Module -------------------------------------------------------------- //


Module::Module(const str& n)
    : State(MODULE, defPrototype, n, NULL, this), complete(false)  { }


Module::~Module()
    { }


void Module::addUses(Module* m)
    { uses.push_back(addSelfVar(m->name, m)); }


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
      defInt(new Ordinal(Type::INT, INTEGER_MIN, INTEGER_MAX)),
      defChar(new Ordinal(Type::CHAR, 0, 255)),
      defBool(new Enumeration(Type::BOOL)),
      defNullCont(new Container(defVoid, defVoid)),
      defStr(new Container(defVoid, defChar)),
      defCharSet(new Container(defChar, defVoid)),
      defCharFifo(new Fifo(defChar))
{
    // Fundamentals
    addTypeAlias("type", defTypeRef);
    addTypeAlias("void", defVoid);
    registerType<Type>(defPrototype);
    addDefinition("null", defVoid, variant::null);
    addTypeAlias("any", defVariant);
    addTypeAlias("int", defInt);
    addTypeAlias("char", defChar);
    addTypeAlias("bool", defBool);
    defBool->addValue(this, "false");
    defBool->addValue(this, "true");
    registerType(defNullCont);
    addTypeAlias("str", defStr);
    addTypeAlias("charset", defCharSet);
    addTypeAlias("charfifo", defCharFifo);

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
objptr<Void> defVoid;
objptr<Prototype> defPrototype;
objptr<QueenBee> queenBee;


void initTypeSys()
{
    // Because all Type objects are also runtime objects, they all have a
    // runtime type of "type reference". The initial typeref object refers to
    // itself and should be created before anything else in the type system.
    defTypeRef = new TypeReference();

    // Void is used in deriving vectors and sets, so we need it before some of
    // the default types are created in QueenBee
    defVoid = new Void();

    // This is a function prototype with no arguments and Void return type,
    // used as a prototype for module constructors
    defPrototype = new Prototype(defVoid);

    // The "system" module that defines default types; some of them have
    // recursive definitions and other kinds of weirdness, and therefore should
    // be defined in C code rather than in Shannon code
    queenBee = new QueenBee();
}


void doneTypeSys()
{
    queenBee = NULL;
    defPrototype = NULL;
    defVoid = NULL;
    defTypeRef = NULL;
}

