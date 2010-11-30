

#include "sysmodule.h"
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
    if (host && host != queenBee)
    {
        host->fqName(stm);
        stm << '.';
    }
    stm << name;
}


void Symbol::dump(fifo& stm) const
{
    if (type)
        type->dumpDef(stm);
    else
        stm << "<var>";
    if (!name.empty())
        stm << ' ' << name;
}


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


StkVar::StkVar(const str& n, Type* t, memint i, State* h)
    : Variable(n, STKVAR, t, i, h)  { assert(i >= 0); }

ArgVar::ArgVar(const str& n, Type* t, memint i, State* h)
    : Variable(n, ARGVAR, t, i, h)  { assert(i >= 0); }

InnerVar::InnerVar(const str& n, Type* t, memint i, State* h)
    : Variable(n, INNERVAR, t, i, h)  { assert(i >= 0); }


FormalArg::FormalArg(const str& n, Type* t)
    : Symbol(n, FORMALARG, t, NULL)  { }


// --- //


Builtin::Builtin(const str& n, CompileFunc f, FuncPtr* p, State* h)
    : Symbol(n, BUILTIN, NULL, h), compileFunc(f), prototype(p)  { }


Builtin::~Builtin()
    { }


void Builtin::dump(fifo& stm) const
{
    if (prototype)
    {
        prototype->dump(stm);
        stm << " {<builtin." << name << ">}";
    }
    else
        stm << "builtin." << name;
}



// --- //


EDuplicate::EDuplicate(const str& _ident) throw(): ident(_ident)  { }
EDuplicate::~EDuplicate() throw()  { }
const char* EDuplicate::what() throw()  { return "Duplicate identifier"; }

EUnknownIdent::EUnknownIdent(const str& _ident) throw(): ident(_ident)  { }
EUnknownIdent::~EUnknownIdent() throw()  { }
const char* EUnknownIdent::what() throw()  { return "Unknown identifier"; }


// --- //


Scope::Scope(bool _local, Scope* _outer)
    : local(_local), outer(_outer)  { }


Scope::~Scope()
    { }


void Scope::addUnique(Symbol* s)
{
    if (!symbols.add(s))
        throw EDuplicate(s->name);
}


void Scope::replaceSymbol(Symbol* s)
{
    if (!symbols.replace(s))
        throw EUnknownIdent(s->name);
}


Symbol* Scope::findShallow(const str& ident) const
{
    Symbol* s = find(ident);
    if (s == NULL)
        throw EUnknownIdent(ident);
    return s;
}



// --- //


BlockScope::BlockScope(Scope* _outer, CodeGen* _gen)
    : Scope(true, _outer), startId(_gen->getLocals()), gen(_gen)  { }


BlockScope::~BlockScope()
    { stkVars.release_all(); }


void BlockScope::deinitLocals()
{
    for (memint i = stkVars.size(); i--; )
        gen->deinitLocalVar(stkVars[i]);
}


StkVar* BlockScope::addStkVar(const str& n, Type* t)
{
    memint varid = startId + stkVars.size();
    if (varid < 0 || varid >= 255)
        error("Maximum number of local variables reached");
    objptr<StkVar> v = new StkVar(n, t, varid, gen->getCodeOwner());
    addUnique(v);   // may throw
    stkVars.push_back(v->grab<StkVar>());
    return v;
}


// --- Type ---------------------------------------------------------------- //


Type::Type(TypeId id)
    : rtobject(id == TYPEREF ? this : defTypeRef), refType(NULL), // ptrType(NULL),
      host(NULL), defName(), typeId(id)
{
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

bool Type::isCharFifo() const
    { return isAnyFifo() && PFifo(this)->elem->isChar(); }

bool Type::isContainer(Type* idx, Type* elem) const
    { return isAnyCont() && elem->identicalTo(PContainer(this)->elem)
         && idx->identicalTo(PContainer(this)->index); }


bool Type::identicalTo(Type* t) const
    { return t == this; }


bool Type::canAssignTo(Type* t) const
    { return identicalTo(t); }


bool Type::isCompatibleWith(const variant& v)
{
    switch (v.getType())
    {
        case variant::VOID:     return isVoid();
        case variant::ORD:      return isAnyOrd();
        case variant::REAL:     notimpl(); return false;
        case variant::VARPTR:   return false;
        case variant::STR:      return isByteVec();
        case variant::RANGE:    return isRange();
        case variant::VEC:      return (isAnyVec() && !isByteVec()) || isByteDict();
        case variant::SET:      return isAnySet() && !isByteSet();
        case variant::ORDSET:   return isByteSet();
        case variant::DICT:     return isAnyDict() && !isByteDict();
        case variant::REF:      return isReference();
        case variant::RTOBJ:
            rtobject* o = v._rtobj();
            return (o == NULL) || o->getType()->canAssignTo(this);
    }
    return false;
}


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
        if (host && host != queenBee)
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
            case variant::VARPTR:   stm << "@@"; if (v._ptr()) dumpVariant(stm, v._ptr()); break;
            case variant::STR:      stm << to_quoted(v._str()); break;
            case variant::RANGE:    stm << v._range().left() << ".." << v._range().right(); break;
            case variant::VEC:      dumpVec(stm, v._vec(), false); break;
            case variant::SET:      dumpVec(stm, v._set(), true); break;
            case variant::ORDSET:   dumpOrdSet(stm, v._ordset()); break;
            case variant::DICT:     dumpDict(stm, v._dict()); break;
            case variant::REF:      stm << '@'; dumpVariant(stm, v._ref()->var); break;
            case variant::RTOBJ:    if (v._rtobj()) v._rtobj()->dump(stm); else stm << "{}"; break;
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
    { stm << '('; to->dumpDef(stm); stm << " *^)"; }


void Reference::dumpValue(fifo& stm, const variant& v) const
    { stm << '@'; dumpVariant(stm, v.as_ref()->var, to); }


bool Reference::identicalTo(Type* t) const
    { return this == t || (t->isReference()
        && to->identicalTo(PReference(t)->to)); }


bool Reference::canAssignTo(Type* t) const
    { return this == t || (t->isReference()
        && to->canAssignTo(PReference(t)->to)); }


// --- Ordinals ------------------------------------------------------------ //


Ordinal::Ordinal(TypeId id, integer l, integer r)
    : Type(id), rangeType(new Range(this)), left(l), right(r)
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


void Enumeration::addValue(State* state, Scope* scope, const str& ident)
{
    integer n = integer(values.size());
    if (n >= 256)  // TODO: maybe this is not really necessary
        error("Maximum number of enum constants reached");
    Definition* d = state->addDefinition(ident, this, n, scope);
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


// --- //


Range::Range(Ordinal* e)
    : Type(RANGE), elem(e)  { }

Range::~Range()
    { }


void Range::dump(fifo& stm) const
{
    stm << '(';
    elem->dumpDef(stm);
    stm << " *[..])";
}


void Range::dumpValue(fifo& stm, const variant& v) const
{
    elem->dumpValue(stm, v.as_range().left());
    stm << "..";
    elem->dumpValue(stm, v.as_range().right());
}


bool Range::identicalTo(Type* t) const
    { return t->isRange() && elem->identicalTo(PRange(t)->elem); }

bool Range::canAssignTo(Type* t) const
    { return t->isRange() && elem->canAssignTo(PRange(t)->elem); }


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
    elem->dumpDef(stm);
    stm << " *[";
    if (!isAnyVec())
        index->dumpDef(stm);
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
    { stm << '('; elem->dumpDef(stm); stm << " *<>)"; }


bool Fifo::identicalTo(Type* t) const
    { return this == t || (t->isAnyFifo() && elem->identicalTo(PFifo(t)->elem)); }


// --- Prototype ----------------------------------------------------------- //


FuncPtr::FuncPtr(Type* r)
    : Type(FUNCPTR), returnType(r)  { }


FuncPtr::~FuncPtr()
    { formalArgs.release_all(); }


void FuncPtr::dump(fifo& stm) const
{
    stm << '(';
    returnType->dumpDef(stm);
    stm << " *(";
    for (int i = 0; i < formalArgs.size(); i++)
    {
        if (i)
            stm << ", ";
        formalArgs[i]->dump(stm);
    }
    stm << ")...)";
}


bool FuncPtr::identicalTo(Type* t) const
    { return this == t || (t->isFuncPtr() && identicalTo(PFuncPtr(t))); }


bool FuncPtr::identicalTo(FuncPtr* t) const
{
    if (this == t)
        return true;
    if (!returnType->identicalTo(t->returnType)
        || formalArgs.size() != t->formalArgs.size())
            return false;
    for (memint i = formalArgs.size(); i--; )
        if (!t->formalArgs[i]->type->identicalTo(formalArgs[i]->type))
            return false;
    return true;
}


bool FuncPtr::canAssignTo(Type* t) const
    { return this == t || (t->isFuncPtr() && canAssignTo(PFuncPtr(t))); }


bool FuncPtr::canAssignTo(FuncPtr* t) const
{
    if (this == t)
        return true;
    if (!returnType->canAssignTo(t->returnType)
        || formalArgs.size() != t->formalArgs.size())
            return false;
    for (memint i = formalArgs.size(); i--; )
        // Note how canAssignTo() check is reversed for arguments
        if (!t->formalArgs[i]->type->canAssignTo(formalArgs[i]->type))
            return false;
    return true;
}


FormalArg* FuncPtr::addFormalArg(const str& n, Type* t)
{
    objptr<FormalArg> arg = new FormalArg(n, t);
    formalArgs.push_back(arg->grab<FormalArg>());
    return arg;
}


// --- SelfStub ------------------------------------------------------------ //


SelfStub::SelfStub()
    : Type(SELFSTUB) { }

SelfStub::~SelfStub()
    { }

bool SelfStub::identicalTo(Type*) const
    { error("'self' incomplete"); return false; }

bool SelfStub::canAssignTo(Type*) const
    { error("'self' incomplete"); return false; }


// --- State --------------------------------------------------------------- //


State::State(State* par, FuncPtr* proto)
    : Type(STATE), Scope(false, par),
      complete(false), innerObjUsed(0), outsideObjectsUsed(0),
      parent(par), parentModule(getParentModule(this)),
      prototype(proto), returnVar(NULL),
      codeseg(new CodeSeg(this)), externFunc(NULL),
      popArgCount(0), returns(false), varCount(0)
{
    // Is this a 'self' state?
    if (prototype->returnType->isSelfStub())
        prototype->resolveSelfType(this);
    // Register all formal args as actual args within the local scope,
    // including the return var
    popArgCount = prototype->formalArgs.size();
    returns = !prototype->isVoidFunc();
    if (!prototype->isVoidFunc())
        returnVar = addArgument("result", prototype->returnType, popArgCount + 1);
    for (memint i = 0; i < popArgCount; i++)
    {
        FormalArg* arg = prototype->formalArgs[i];
        addArgument(arg->name, arg->type, popArgCount - i);
    }
}


State::~State()
{
    args.release_all();
    innerVars.release_all();
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


Module* State::getParentModule(State* m)
{
    while (m->parent)
        m = m->parent;
    return PModule(m);
}


void State::dump(fifo& stm) const
{
    stm << '(';
    prototype->dump(stm);
    stm << " {<code>})";
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
    for (memint i = 0; i < innerVars.size(); i++)
    {
        InnerVar* var = innerVars[i];
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


Definition* State::addDefinition(const str& n, Type* t, const variant& v, Scope* scope)
{
    if (n.empty())
        fatal(0x3001, "Empty identifier");
    objptr<Definition> d = new Definition(n, t, v, this);
    scope->addUnique(d); // may throw
    defs.push_back(d->grab<Definition>());
    if (t->isTypeRef())
    {
        // In case this def is a type definition, also register the type with this state,
        // and bind the type object to this def for better diagnostic output (dump() family).
        _registerType(cast<Type*>(v._rtobj()), d);
    }
    return d;
}


void State::addTypeAlias(const str& n, Type* t)
    { addDefinition(n, t->getType(), t, this); }


ArgVar* State::addArgument(const str& n, Type* t, memint varid)
{
    objptr<ArgVar> arg = new ArgVar(n, t, varid, this);
    if (!n.empty())
        addUnique(arg);
    args.push_back(arg->grab<ArgVar>());
    return arg;
}


InnerVar* State::addInnerVar(InnerVar* var)
{
    if (var->id >= 255)
        error("Too many variables");
    varCount++;
    return innerVars.push_back(var->grab<InnerVar>());
}


InnerVar* State::addInnerVar(const str& n, Type* t)
{
    if (n.empty())
        fatal(0x3002, "Empty identifier");
    objptr<InnerVar> v = new InnerVar(n, t, varCount, this);
    addUnique(v);
    return addInnerVar(v);
}


InnerVar* State::reclaimArg(ArgVar* arg, Type* t)
{
    objptr<InnerVar> v = new InnerVar(arg->name, t, varCount, this);
    replaceSymbol(v);
    return addInnerVar(v);
}


stateobj* State::newInstance()
{
    if (varCount == 0)
        return NULL;
    stateobj* obj = new(varCount) stateobj(this);
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


// --- Module -------------------------------------------------------------- //


Module::Module(const str& n, const str& f)
    : State(NULL, new FuncPtr(this)), filePath(f)
{
    defName = n;
    registerType(prototype);
}


Module::~Module()
{
    builtins.release_all();
    codeSegs.release_all();
}


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


void Module::addUsedModule(Module* m)
    { usedModuleVars.push_back(addInnerVar(m->getName(), m)); }


InnerVar* Module::findUsedModuleVar(Module* m)
{
    for (memint i = usedModuleVars.size(); i--; )
    {
        InnerVar* var = usedModuleVars[i];
        if (var->type == m)
            return var;
    }
    return NULL;
}


void Module::registerString(str& s)
{
    if (s.empty())
        return;
    constStrings.push_back(s);
    // TODO: make finding duplicates a compiler option?
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


Builtin* Module::addBuiltin(Builtin* b)
{
    builtins.push_back(b->grab<Builtin>());
    addUnique(b);
    return b;
}


Builtin* Module::addBuiltin(const str& n, Builtin::CompileFunc f, FuncPtr* p)
{
    return addBuiltin(new Builtin(n, f, p, this));
}


// --- QueenBee ------------------------------------------------------------ //


QueenBee::QueenBee()
    : Module("system", "<builtin>"),
      defVariant(new Variant()),
      defInt(new Ordinal(Type::INT, INTEGER_MIN, INTEGER_MAX)),
      defChar(new Ordinal(Type::CHAR, 0, 255)),
      defByte(new Ordinal(Type::INT, 0, 255)),
      defBool(new Enumeration(Type::BOOL)),
      defNullCont(new Container(defVoid, defVoid)),
      defStr(new Container(defVoid, defChar)),
      defCharSet(new Container(defChar, defVoid)),
      defCharFifo(new Fifo(defChar)),
      defSelfStub(new SelfStub())
{
    // Fundamentals
    addTypeAlias("type", defTypeRef);
    addTypeAlias("void", defVoid);
    addDefinition("null", defVoid, variant::null, this);
    addTypeAlias("any", defVariant);
    addTypeAlias("int", defInt);
    addTypeAlias("char", defChar);
    addTypeAlias("byte", defByte);
    addTypeAlias("bool", defBool);
    defBool->addValue(this, this, "false");
    defBool->addValue(this, this, "true");
    addTypeAlias("voidc", defNullCont);
    addTypeAlias("str", defStr);
    addTypeAlias("chars", defCharSet);
    addTypeAlias("charf", registerType(defCharFifo)->getRefType());
    addTypeAlias("self", defSelfStub);

    // Constants
    addDefinition("__VER_MAJOR", defInt, SHANNON_VERSION_MAJOR, this);
    addDefinition("__VER_MINOR", defInt, SHANNON_VERSION_MINOR, this);
    addDefinition("__VER_FIX", defInt, SHANNON_VERSION_FIX, this);

    // Variables
    resultVar = addInnerVar("__program_result", defVariant);
    sioVar = addInnerVar("sio", defCharFifo);
    serrVar = addInnerVar("serr", defCharFifo);

    // Built-ins:
    // Return type for many builtins is set to void because they don't need
    // an empty return var be pushed onto the stack like for ordinary
    // functions: the builtins usually handle the return value themselves.
    // Also, NULL argument means anything goes, the builtin parser will
    // take care of type checking.
    FuncPtr* proto1 = registerType<FuncPtr>(new FuncPtr(defVoid));
    proto1->addFormalArg("arg", NULL);
    addBuiltin("len", compileLen, proto1);
    addBuiltin("lo", compileLo, proto1);
    addBuiltin("hi", compileHi, proto1);

    getCodeSeg()->close();
    setComplete();
}


QueenBee::~QueenBee()
    { }


stateobj* QueenBee::newInstance()
{
    assert(getCodeSeg()->closed);
    assert(complete);
    stateobj* inst = parent::newInstance();
    sio.setType(defCharFifo);
    serr.setType(defCharFifo);
    *inst->member(sioVar->id) = &sio;
    *inst->member(serrVar->id) = &serr;
    return inst;
}


// --- Globals ------------------------------------------------------------- //


objptr<TypeReference> defTypeRef;
objptr<Void> defVoid;
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

    // The "system" module that defines default types; some of them have
    // recursive definitions and other kinds of weirdness, and therefore should
    // be defined in C code rather than in Shannon code
    queenBee = new QueenBee();
}


void doneTypeSys()
{
    queenBee = NULL;
    defVoid = NULL;
    defTypeRef = NULL;
}

