
#include "typesys.h"
#include "vm.h"


static void error(const char* msg)
    { throw emessage(msg); }


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
    type->dumpDef(stm);
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


LocalVar::LocalVar(const str& n, Type* t, memint i, State* h)
    : Variable(n, LOCALVAR, t, i, h)  { }


SelfVar::SelfVar(const str& n, Type* t, memint i, State* h)
    : Variable(n, SELFVAR, t, i, h)  { }


// --- //


EDuplicate::EDuplicate(const str& _ident) throw(): ident(_ident)  { }
EDuplicate::~EDuplicate() throw()  { }
const char* EDuplicate::what() throw()  { return "Duplicate identifier"; }

EUnknownIdent::EUnknownIdent(const str& _ident) throw(): ident(_ident)  { }
EUnknownIdent::~EUnknownIdent() throw()  { }
const char* EUnknownIdent::what() throw()  { return "Unknown identifier"; }


// --- //


template class symtbl<Symbol>;


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


LocalVar* BlockScope::addLocalVar(const str& name, Type* type)
{
    memint id = startId + localVars.size();
    if (id >= 127)
        error("Maximum number of local variables reached");
    objptr<LocalVar> v = new LocalVar(name, type, id, gen->getState());
    addUnique(v);   // may throw
    localVars.push_back(v->grab<LocalVar>());
    return v;
}


// --- Type ---------------------------------------------------------------- //


Type::Type(TypeId id)
    : rtobject(id == TYPEREF ? this : defTypeRef), refType(NULL), // ptrType(NULL),
      host(NULL), defName(), typeId(id)
{
/*
    if (id != VARPTR)
    {
        ptrType = new Pointer(this);
        if (id != REF)
            refType = new Reference(this);
    }
*/
    if (id != REF)
        refType = new Reference(this);
}


Type::~Type()
    { }


bool Type::isByte() const
    { return isAnyOrd() && POrdinal(this)->isByte(); }


bool Type::isBit() const
    { return isAnyOrd() && POrdinal(this)->isBit(); }


bool Type::isFullChar() const
    { return isChar() && POrdinal(this)->isFullChar(); }


bool Type::isByteVec() const
    { return isAnyVec() && PContainer(this)->hasByteElem(); }


bool Type::isByteSet() const
    { return isAnySet() && PContainer(this)->hasByteIndex(); }


bool Type::isByteDict() const
    { return isAnyDict() && PContainer(this)->hasByteIndex(); }


bool Type::isContainer(Type* idx, Type* elem) const
    { return isAnyCont() && elem->identicalTo(PContainer(this)->elem)
         && idx->identicalTo(PContainer(this)->index); }


bool Type::identicalTo(Type* t) const
    { return t == this; }


bool Type::canAssignTo(Type* t) const
    { return identicalTo(t); }


bool Type::empty() const
    { return false; }


void Type::dump(fifo& stm) const
{
    if (defName.empty())
        fatal(0x3003, "Invalid type alias");
    stm << "builtin." << defName;
}


void Type::dumpDef(fifo& stm) const
{
    if (defName.empty())
        dump(stm);
    else
    {
        if (host)
        {
            host->fqName(stm);
            stm << '.';
        }
        stm << defName;
    }
}


Container* Type::deriveVec(State* h)
{
    if (isVoid())
        return queenBee->defNullCont;
    else if (isFullChar())
        return queenBee->defStr;
    else
        return h->getContainerType(defVoid, this);
}


Container* Type::deriveSet(State* h)
{
    if (isReference())
        error("Reference type not allowed in set");
    if (isVoid())
        return queenBee->defNullCont;
    else if (isFullChar())
        return queenBee->defCharSet;
    else
        return h->getContainerType(this, defVoid);
}


Container* Type::deriveContainer(State* h, Type* idx)
{
    if (idx->isReference())
        error("Reference type not allowed in dict/set");
    if (isVoid())
        return idx->deriveSet(h);
    else if (idx->isVoid())
        return deriveVec(h);
    else
        return h->getContainerType(idx, this);
}


Fifo* Type::deriveFifo(State* h)
{
    if (isFullChar())
        return queenBee->defCharFifo;
    else
        // TODO: lookup existing fifo types in h
        return h->registerType(new Fifo(this));
}


// --- Printing ------------------------------------------------------------ //


void Type::dumpValue(fifo& stm, const variant& v) const
{
    // Default is to print raw variant value
    dumpVariant(stm, v, NULL);
}


static void dumpVec(fifo& stm, const varvec& vec, bool curly, Type* elemType = NULL)
{
    stm << (curly ? '{' : '[');
    for (memint i = 0; i < vec.size(); i++)
    {
        if (i) stm << ", ";
        dumpVariant(stm, vec[i], elemType);
    }
    stm << (curly ? '}' : ']');
}


static void dumpOrdVec(fifo& stm, const str& s, Type* elemType = NULL)
{
    stm << '[';
    for (memint i = 0; i < s.size(); i++)
    {
        if (i) stm << ", ";
        dumpVariant(stm, s[i], elemType);
    }
    stm << ']';
}


static void dumpOrdDict(fifo& stm, const varvec& v, Type* keyType = NULL, Type* elemType = NULL)
{
    stm << '{';
    int count = 0;
    for (memint i = 0; i < v.size(); i++)
    {
        if (v[i].is_null()) continue;
        if (count++) stm << ", ";
        dumpVariant(stm, integer(i), keyType);
        stm << " = ";
        dumpVariant(stm, v[i], elemType);
    }
    stm << '}';
}


static void dumpOrdSet(fifo& stm, const ordset& s, Ordinal* elemType = NULL)
{
    stm << '{';
    if (!s.empty())
    {
        int i = elemType ? int(imin<integer>(elemType->left, 0)) : 0;
        int right = elemType ? int(imax<integer>(elemType->right, 255)) : 255;
        int count = 0;
        while (i <= right)
        {
            if (s.find(i))
            {
                if (count++)
                    stm << ", ";
                dumpVariant(stm, i, elemType);
                int l = ++i;
                while (i <= right && s.find(i))
                    i++;
                if (i > l)
                {
                    stm << "..";
                    dumpVariant(stm, i - 1, elemType);
                }
            }
            else
                i++;
        }
    }
    stm << '}';
}


static void dumpDict(fifo& stm, const vardict& d, Type* keyType = NULL, Type* valType = NULL)
{
    stm << '{';
    for (memint i = 0; i < d.size(); i++)
    {
        if (i) stm << ", ";
        dumpVariant(stm, d.key(i), keyType);
        stm << " = ";
        dumpVariant(stm, d.value(i), valType);
    }
    stm << '}';
}


void dumpVariant(fifo& stm, const variant& v, Type* type)
{
    if (type)
        type->dumpValue(stm, v);
    else
    {
        switch (v.getType())
        {
        case variant::VOID:     stm << "null"; break;
        case variant::ORD:      stm << v._int(); break;
        case variant::REAL:     notimpl(); break;
        case variant::VARPTR:   stm << "@@"; if (v._var()) dumpVariant(stm, v._var()); break;
        case variant::STR:      stm << to_quoted(v._str()); break;
        case variant::VEC:      dumpVec(stm, v._vec(), false); break;
        case variant::SET:      dumpVec(stm, v._set(), true); break;
        case variant::ORDSET:   dumpOrdSet(stm, v._ordset()); break;
        case variant::DICT:     dumpDict(stm, v._dict()); break;
        case variant::REF:      stm << '@'; dumpVariant(stm, v._ref()->var); break;
        case variant::RTOBJ:    if (v._rtobj()) v._rtobj()->dump(stm); break;
        }
    }
}


// --- General Types ------------------------------------------------------- //


TypeReference::TypeReference(): Type(TYPEREF)  { }
TypeReference::~TypeReference()  { }

void TypeReference::dumpValue(fifo& stm, const variant& v) const
{
    Type* type = cast<Type*>(v.as_rtobj());
    type->dump(stm);
}


Void::Void(): Type(VOID)  { }
Void::~Void()  { }


Variant::Variant(): Type(VARIANT)  { }
Variant::~Variant()  { }


Reference::Reference(Type* _to)
    : Type(REF), to(_to)  { }


Reference::~Reference()
    { }


void Reference::dump(fifo& stm) const
    { stm << '('; to->dumpDef(stm); stm << "^)"; }


void Reference::dumpValue(fifo& stm, const variant& v) const
    { stm << '@'; dumpVariant(stm, v, to); }


bool Reference::identicalTo(Type* t) const
    { return this == t || (t->isReference()
        && to->identicalTo(PReference(t)->to)); }


bool Reference::canAssignTo(Type* t) const
    { return this == t || (t->isReference()
        && to->canAssignTo(PReference(t)->to)); }


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
        error("Subrange can't be bigger than original");
    return _createSubrange(l, r);
}


void Ordinal::dump(fifo& stm) const
{
    if (isInt())
        stm << '(' << to_string(left) << ".." << to_string(right) << ')';
    else if (isChar())
        stm << '(' << to_quoted(uchar(left)) << ".." << to_quoted(uchar(right)) << ')';
    else
        notimpl();
}


void Ordinal::dumpValue(fifo& stm, const variant& v) const
{
    if (isInt())
        stm << v.as_ord();
    else if (isChar())
        stm << to_quoted(uchar(v.as_ord()));
    else
        notimpl();
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
        error("Maximum number of enum constants reached");
    Definition* d = state->addDefinition(ident, this, n);
    values.push_back(d);
    reassignRight(n);
}


void Enumeration::dump(fifo& stm) const
{
    if (left > 0 || right < values.size() - 1)  // subrange?
        stm << '(' << values[0]->name << ".." << values[memint(right)]->name << ')';
    else
    {
        stm << '(';
        for (memint i = 0; i < values.size(); i++)
            stm << (i ? ", " : "") << values[i]->name;
        stm << ')';
    }
}


void Enumeration::dumpValue(fifo& stm, const variant& v) const
{
    integer i = v.as_ord();
    if (isInRange(i))
        stm << values[memint(i)]->name;
    else
        stm << i;
}


bool Enumeration::identicalTo(Type* t) const
    { return this == t; }


bool Enumeration::canAssignTo(Type* t) const
    { return t->typeId == typeId && values == PEnumeration(t)->values; }


// --- Containers ---------------------------------------------------------- //


Type::TypeId Type::contType(Type* i, Type* e)
{
    if (i->isVoid())
        if (e->isVoid())
            return NULLCONT;
        else
            return VEC;
    else if (e->isVoid())
        return SET;
    else
        return DICT;
}


Container::Container(Type* i, Type* e)
    : Type(contType(i, e)), index(i), elem(e)  { }


Container::~Container()
    { }


void Container::dump(fifo& stm) const
{
    stm << '(';
    if (isAnySet())
    {
        index->dumpDef(stm);
        stm << "[..";
    }
    else
    {
        elem->dumpDef(stm);
        stm << '[';
        if (!isAnyVec())
            index->dumpDef(stm);
    }
    stm << "])";
}


void Container::dumpValue(fifo& stm, const variant& v) const
{
    if (isNullCont())
        stm << "[]";
    else if (isAnyVec())
    {
        if (elem->isChar())
            stm << to_quoted(v.as_str());
        else if (isByteVec())
            dumpOrdVec(stm, v.as_str(), elem);
        else
            dumpVec(stm, v.as_vec(), false, elem);
    }
    else if (isAnySet())
    {
        if (isByteSet())
            dumpOrdSet(stm, v.as_ordset(), POrdinal(index));
        else
            dumpVec(stm, v.as_set(), true, index);
    }
    else if (isAnyDict())
    {
        if (isByteDict())
            dumpOrdDict(stm, v.as_vec(), index, elem);
        else
            dumpDict(stm, v.as_dict(), index, elem);
    }
    else
        notimpl();
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


void Fifo::dump(fifo& stm) const
    { stm << '('; elem->dumpDef(stm); stm << "<>)"; }


bool Fifo::identicalTo(Type* t) const
    { return this == t || (t->isFifo() && elem->identicalTo(PFifo(t)->elem)); }


// --- Prototype ----------------------------------------------------------- //


Prototype::Prototype(Type* r)
    : Type(PROTOTYPE), returnType(r)  { }


Prototype::~Prototype()
    { args.release_all(); }


void Prototype::dump(fifo& stm) const
{
    stm << '(';
    returnType->dumpDef(stm);
    stm << "*(";
    for (int i = 0; i < args.size(); i++)
    {
        if (i)
            stm << ", ";
        args[i]->dump(stm);
    }
    stm << "))";
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


State::State(TypeId id, Prototype* proto, State* par, State* self)
    : Type(id), Scope(par), parent(par), selfPtr(self),
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
    if (defName.empty())
        stm << '*';
    else
        stm << defName;
}


str State::getParentModuleName() const
{
    if (parent == NULL)
    {
        assert(isModule());
        return defName.empty() ? str('*') : defName;
    }
    else
        return parent->getParentModuleName();
}


void State::dump(fifo& stm) const
{
    // TODO: better dump for states?
    stm << "(state ";
    prototype->dump(stm);
    stm << ')';
//    dumpAll(stm);
}


void State::dumpAll(fifo& stm) const
{
    // Print all registered types (except states) in comments
    for (memint i = 0; i < types.size(); i++)
    {
        Type* type = types[i];
        if (type->isAnyState() || type->isReference())
            continue;
        stm << "type ";
        types[i]->dump(stm);
        stm << endl;
    }
    // Print definitions
    for (memint i = 0; i < defs.size(); i++)
    {
        Definition* def = defs[i];
        stm << "def ";
        def->type->dumpDef(stm);
        stm << ' ';
        def->fqName(stm);
        stm << " = ";
        Type* typeDef = def->getAliasedType();
        if (typeDef && (def->name != typeDef->defName || typeDef->host != this))
            typeDef->dumpDef(stm);  // just the name if this is not the definition of this type
        else
            dumpVariant(stm, def->value, def->type);
        stm << endl;
    }
    for (memint i = 0; i < selfVars.size(); i++)
    {
        SelfVar* var = selfVars[i];
        stm << "var ";
        var->type->dumpDef(stm);
        stm << ' ';
        var->fqName(stm);
        stm << endl;
    }
}


Type* State::_registerType(Type* t, Definition* d)
{
    if (t->host == NULL)
    {
        types.push_back(t->grab<Type>());
        t->host = this;
        // Also register the bundled reference type, or in case this is a reference,
        // register its bundled value type.
        if (isReference())
            _registerType(t->getValueType(), NULL);
        else
            _registerType(t->getRefType(), NULL);
    }
    // Also assign the diagnostic type alias, if appropriate
    if (t->host == this && d && t->defName.empty())
        t->defName = d->name;
    return t;
}


Definition* State::addDefinition(const str& n, Type* t, const variant& v)
{
    if (n.empty())
        fatal(0x3001, "Empty identifier");
    objptr<Definition> d = new Definition(n, t, v, this);
    addUnique(d); // may throw
    defs.push_back(d->grab<Definition>());
    if (t->isTypeRef())
    {
        // In case this def is a type definition, also register the type with this state,
        // and bind the type object to this def for better diagnostic output (dump() family).
        _registerType(cast<Type*>(v._rtobj()), d);
    }
    return d;
}


Definition* State::addTypeAlias(const str& n, Type* t)
    { return addDefinition(n, t->getType(), t); }


SelfVar* State::addSelfVar(const str& n, Type* t)
{
    if (n.empty())
        fatal(0x3002, "Empty identifier");
    memint id = selfVarCount();
    if (id >= 127)
        error("Too many variables");
    objptr<SelfVar> v = new SelfVar(n, t, id, this);
    addUnique(v);
    selfVars.push_back(v->grab<SelfVar>());
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


Container* State::getContainerType(Type* idx, Type* elem)
{
    assert(!idx->isReference());
    // TODO: replace linear search with something faster?
    for (memint i = 0; i < types.size(); i++)
    {
        Type* t = types[i];
        if (t->isContainer(idx, elem))
            return PContainer(t);
    }
    return registerType(new Container(idx, elem));
}


// TODO: identicalTo()


// --- Module -------------------------------------------------------------- //


Module::Module(const str& n, const str& f)
    : State(MODULE, defPrototype, NULL, this), complete(false), filePath(f)
        { defName = n; }


Module::~Module()
    { codeSegs.release_all(); }


void Module::dump(fifo& stm) const
{
    stm << endl << "#MODULE_DUMP " << getName() << endl << endl;
    dumpAll(stm);
    for (memint i = 0; i < codeSegs.size(); i++)
    {
        CodeSeg* c = codeSegs[i];
        stm << endl << "#CODE_DUMP ";
        c->getStateType()->fqName(stm);
        stm << endl << endl;
        c->dump(stm);
    }
}


void Module::addUses(Module* m)
    { uses.push_back(addSelfVar(m->getName(), m)); }


void Module::registerString(str& s)
{
    if (s.empty())
        return;
    constStrings.push_back(s);
    // TODO: make the below optional?
/*
    memint i;
    if (constStrings.bsearch(s, i))
        s = constStrings[i];
    else
        constStrings.insert(i, s);
*/
}


void Module::registerCodeSeg(CodeSeg* c)
    { codeSegs.push_back(c->grab<CodeSeg>()); }


// --- QueenBee ------------------------------------------------------------ //


QueenBee::QueenBee()
    : Module("system", "<builtin>"),
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
    addTypeAlias("voidc", defNullCont);
    addTypeAlias("str", defStr);
    addTypeAlias("chars", defCharSet);
    addTypeAlias("charf", registerType(defCharFifo)->getRefType());

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
    *inst->member(sioVar->id) = &sio;
    *inst->member(serrVar->id) = &serr;
    getCodeSeg()->close();
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

