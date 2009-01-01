
#include <stdio.h>
#include <limits.h>

#include "langobj.h"
#include "codegen.h"


static void notImpl()
{
    throw EMessage("Feature not implemented");
}


// --- BASIC LANGUAGE OBJECTS ---------------------------------------------- //


ShBase::ShBase(ShBaseId iBaseId)
    : BaseNamed(), baseId(iBaseId)  { }

ShBase::ShBase(const string& name, ShBaseId iBaseId)
    : BaseNamed(name), baseId(iBaseId)  { }


// --- TYPE --- //


ShType::ShType(ShTypeId iTypeId)
    : ShBase(baseType), owner(NULL),
      derivedVectorType(NULL), derivedSetType(NULL), derivedRefType(NULL)
{
    setTypeId(iTypeId);
}

ShType::ShType(const string& name, ShTypeId iTypeId)
    : ShBase(name, baseType), owner(NULL),
      derivedVectorType(NULL), derivedSetType(NULL), derivedRefType(NULL)
{
    setTypeId(iTypeId);
}

ShType::~ShType()  { }


void ShType::setOwner(ShScope* newOwner)
{
    if (owner != NULL)
        internal(3);
    owner = newOwner;
}

offs memAlign(offs size)
{
    if (size == 0)
        return 0;
    else
        return (((size - 1) / DATA_MEM_ALIGN) + 1) * DATA_MEM_ALIGN;
}

void ShType::setTypeId(ShTypeId iTypeId)
{
    typeId = iTypeId;
    // stoByte, stoInt, stoLarge, stoPtr, stoVec, stoVoid
    static offs stoToSize[_stoMax] = 
        { 1, 4, 8, sizeof(ptr), sizeof(ptr), 0 };
    static StorageModel typeToSto[_typeMax] =
    {
        stoByte, stoInt, stoLarge, stoByte, stoByte, stoByte,
        stoVec, stoVec, stoPtr, stoLarge,
        stoPtr,
        stoVoid, stoVoid, stoVoid,
        stoVoid
    };
    stoModel = typeToSto[iTypeId];
    size = stoToSize[stoModel];
    alignedSize = memAlign(size);
}

bool ShType::isString() const
{
    return typeId == typeVector && PVector(this)->elementType->typeId == typeChar;
}

string ShType::getDefinition(const string& objName) const
{
    if (!name.empty())
        return name + (objName.empty() ? "" : " " + objName);
    else
        return getFullDefinition(objName);
}

string ShType::getDefinition() const
{
    return getDefinition("");
}

string ShType::getDefinitionQ() const
{
    return '\'' + getDefinition() + '\'';
}

ShVector* ShType::deriveVectorType()
{
    if (isVoid())
        internal(11);
    if (derivedVectorType == NULL)
    {
        derivedVectorType = new ShVector(this);
        owner->addAnonType(derivedVectorType);
    }
    return derivedVectorType;
}

ShArray* ShType::deriveArrayType(ShType* indexType)
{
    if (!indexType->canBeArrayIndex())
        internal(7);
    if (isVoid())
        return indexType->deriveSetType((ShVoid*)this);
    else
    {
        ShArray* array = new ShArray(this, indexType);
        owner->addAnonType(array);
        return array;
    }
}

ShSet* ShType::deriveSetType(ShVoid* elementType)
{
    if (derivedSetType == NULL)
    {
        derivedSetType = new ShSet(elementType, this);
        owner->addAnonType(derivedSetType);
    }
    return derivedSetType;
}


ShReference* ShType::deriveRefType()
{
    if (derivedRefType == NULL)
    {
        derivedRefType = new ShReference(this);
        owner->addAnonType(derivedRefType);
    }
    return derivedRefType;
}

// --- TYPE ALIAS --- //

ShTypeAlias::ShTypeAlias(const string& name, ShType* iBase)
    : ShBase(name, baseTypeAlias), base(iBase)  { }


// --- VARIABLE --- //

ShVariable::ShVariable(ShType* iType)
    : ShBase(baseVariable), type(iType), dataOffset(0), local(false)  { }

ShVariable::ShVariable(const string& name, ShType* iType)
    : ShBase(name, baseVariable), type(iType), dataOffset(0), local(false)  { }


// --- SYMBOLS-ONLY SCOPE --- //

ShSymScope::ShSymScope(ShSymScope* iParent)
    : ShType("", typeLocalSymScope), parent(iParent)  { }

void ShSymScope::addSymbol(ShBase* obj)
{
    if (obj->name.empty())
        internal(4);
    symbols.addUnique(obj);
}

void ShSymScope::finalizeVars(VmCodeGen& code)
{
    for (int i = symbols.size() - 1; i >= 0; i--)
    {
        ShBase* obj = symbols[i];
        if (obj->isVariable())
            code.genFinVar((ShVariable*)obj);
    }
}

ShBase* ShSymScope::deepFind(const string& name) const
{
    ShBase* obj = find(name);
    if (obj != NULL)
        return obj;
    for (int i = uses.size() - 1; i >= 0; i--)
    {
        obj = uses[i]->find(name);
        if (obj != NULL)
            return obj;
    }
    if (parent != NULL)
        return parent->deepFind(name);
    return NULL;
}

void ShSymScope::addUses(ShModule* obj)
        { uses.add(obj); addSymbol(obj); }


// --- SCOPE --- //

ShScope::ShScope(const string& name, ShTypeId iTypeId)
        : ShSymScope(name, iTypeId)  { }

ShScope::~ShScope()
{
    // Order is important
    typeAliases.clear();
    consts.clear();
    vars.clear();
    types.clear();
}

void ShScope::addType(ShType* obj, ShSymScope* symScope)
{
    types.add(obj);
    obj->setOwner(this);
    symScope->addSymbol(obj);
}

void ShScope::addAnonType(ShType* obj)
{
    types.add(obj);
    obj->setOwner(this);
}

void ShScope::addTypeAlias(ShTypeAlias* obj, ShSymScope* symScope)
{
    typeAliases.add(obj);
    symScope->addSymbol(obj);
}

void ShScope::addConstant(ShConstant* obj, ShSymScope* symScope)
{
    consts.add(obj);
    symScope->addSymbol(obj);
}

void ShScope::addVariable(ShVariable* obj, ShSymScope* symScope)
{
    vars.add(obj);
    symScope->addSymbol(obj);
}


#ifdef DEBUG
void ShScope::dump(string indent) const
{
    for (int i = 0; i < types.size(); i++)
        printf("%s# def %s\n", indent.c_str(), types[i]->getDefinition("*").c_str());
    for (int i = 0; i < typeAliases.size(); i++)
        printf("%sdef %s\n", indent.c_str(),
            typeAliases[i]->base->getDefinition(typeAliases[i]->name).c_str());
    for (int i = 0; i < consts.size(); i++)
    {
        ShConstant* c = consts[i];
        ShType* t = c->value.type;
        printf("%sconst %s = %s\n", indent.c_str(),
            t->getDefinition(c->name).c_str(),
            t->displayValue(c->value).c_str());
    }
    for (int i = 0; i < vars.size(); i++)
        printf("%svar %s\n", indent.c_str(),
            vars[i]->type->getDefinition(vars[i]->name).c_str());
}
#endif


// --- LANGUAGE TYPES ----------------------------------------------------- //

// TODO: define lo() and hi() for ordinals and also ranges

EInvalidSubrange::EInvalidSubrange(ShOrdinal* type)
    : EMessage("Invalid subrange for " + type->getDefinition())  { }
    

int Range::physicalSize() const
{
    if (min >= 0)
    {
        // only ordinals within 0..255 can be unsigned
        if (max <= UCHAR_MAX)
            return 1;
        if (max <= INT_MAX)
            return 4;
        return 8;
    }
    else
    {
        // signed ordinals are always 4 or 8 bytes
        if (min == LLONG_MIN)
            return 8;
        large t = ~min;
        if (max > t)
            t = max;
        if (t <= INT_MAX)
            return 4;
        return 8;
    }
}


ShOrdinal::ShOrdinal(ShTypeId iTypeId, large min, large max)
    : ShType(iTypeId), derivedRangeType(NULL),
      range(min, max)  { }

ShOrdinal::ShOrdinal(const string& name, ShTypeId iTypeId, large min, large max)
    : ShType(name, iTypeId), derivedRangeType(NULL),
      range(min, max)  { }

ShRange* ShOrdinal::deriveRangeType()
{
    if (!isOrdinal())
        internal(10);
    if (derivedRangeType == NULL)
    {
        derivedRangeType = new ShRange(this);
        owner->addAnonType(derivedRangeType);
    }
    return derivedRangeType;
}

ShOrdinal* ShOrdinal::deriveOrdinalFromRange(const ShValue& value)
{
    large min = value.rangeMin();
    large max = value.rangeMax();
    if (rangeEquals(min, max))
        return this;
    if (min >= max || !rangeIsGreaterOrEqual(min, max))
        throw EInvalidSubrange(this);
    ShOrdinal* derived = cloneWithRange(min, max);
    owner->addAnonType(derived);
    return derived;
}

bool ShOrdinal::contains(const ShValue& v) const
{
    if (v.type->isOrdinal())
    {
        if (POrdinal(v.type)->isLargeInt())
            return v.value.large_ >= range.min && v.value.large_ <= range.max;
        else
            return v.value.int_ >= range.min && v.value.int_ <= range.max;
    }
    else
        internal(5);
    return false;
}


// --- INTEGER TYPE --- //

ShInteger::ShInteger(const string& name, large min, large max)
    : ShOrdinal(name, typeInt32, min, max)
{
    int size = range.physicalSize();
    if (size == 1)
        setTypeId(typeInt8);
    else if (size == 8)
        setTypeId(typeInt64);
}


string ShInteger::getFullDefinition(const string& objName) const
{
    return itostring(range.min) + ".." + itostring(range.max)
        + ' ' + objName;
}

string ShInteger::displayValue(const ShValue& v) const
{
    if (isLargeInt())
        return itostring(v.value.large_) + 'L';
    else
        return itostring(v.value.int_);
}

ShOrdinal* ShInteger::cloneWithRange(large min, large max)
    { return new ShInteger(emptystr, min, max); }


// --- CHAR TYPE --- //

ShChar::ShChar(const string& name, int min, int max)
    : ShOrdinal(name, typeChar, min, max)  { }

string ShChar::getFullDefinition(const string& objName) const
{
    return "'" + mkPrintable(range.min)  + "'..'" + mkPrintable(range.max)
        + "' " + objName;
}

string ShChar::displayValue(const ShValue& v) const
    { return "'" + mkPrintable(v.value.int_) + "'"; }

ShOrdinal* ShChar::cloneWithRange(large min, large max)
    { return new ShChar(emptystr, min, max); }


// --- ENUM TYPE --- //

ShEnum::ShEnum()
    : ShOrdinal(typeEnum, 0, 0)  { }

ShEnum::ShEnum(const BaseTable<ShConstant>& t, int min, int max)
    : ShOrdinal(typeEnum, min, max), values(t)  { }

void ShEnum::finish()
{
    int max = values.size() - 1;
    if (max >= 256)
        internal(15);
    reassignMax(max);
}

// TODO: better printing maybe
string ShEnum::getFullDefinition(const string& objName) const
{
    return values[range.min]->name + ".." + values[range.max]->name
        + ' ' + objName;
}

ShOrdinal* ShEnum::cloneWithRange(large min, large max)
{
    return new ShEnum(values, min, max);
}

string ShEnum::displayValue(const ShValue& v) const
{
    int i = v.value.int_;
    if (i >= 0 && i < values.size())
        return values[i]->name;
    else if (!name.empty())
        return name + "(" + itostring(i) + ")";
    else
        return itostring(i);
}


// --- BOOL TYPE --- //

ShBool::ShBool(const string& name)
    : ShOrdinal(name, typeBool, 0, 1)  { }

string ShBool::getFullDefinition(const string& objName) const
    { return "false..true " + objName; }

string ShBool::displayValue(const ShValue& v) const
    { return v.value.int_ ? "true" : "false"; }

ShOrdinal* ShBool::cloneWithRange(large min, large max)
    { throw EInvalidSubrange(this); }



// --- VOID TYPE --- //

ShVoid::ShVoid(const string& name)
    : ShType(name, typeVoid)  { }

string ShVoid::getFullDefinition(const string& objName) const
    { return "void"; }

string ShVoid::displayValue(const ShValue& v) const
    { return "null"; }


// --- TYPEREF TYPE --- //

ShTypeRef::ShTypeRef(const string& name)
    : ShType(name, typeTypeRef)  { }

string ShTypeRef::getFullDefinition(const string& objName) const
    { return "typeref"; }

string ShTypeRef::displayValue(const ShValue& v) const
    { return "typeof(" + ((ShType*)(v.value.ptr_))->getDefinition() + ")"; }


// --- RANGE TYPE --- //

ShRange::ShRange(ShOrdinal* iBase)
    : ShType(typeRange), base(iBase)  { }

ShRange::ShRange(const string& name, ShOrdinal* iBase)
    : ShType(name, typeTypeRef), base(iBase)  { }

string ShRange::getFullDefinition(const string& objName) const
    { return base->getDefinition(objName) + "[..]"; }

string ShRange::displayValue(const ShValue& v) const
{
    ShValue left(base, int(v.value.large_));
    ShValue right(base, int(v.value.large_ >> 32));
    return base->displayValue(left) + ".." + base->displayValue(right);
}


// --- VECTOR TYPE --- //

ShVector::ShVector(ShType* iElementType)
        : ShType(typeVector), elementType(iElementType)  { }

ShVector::ShVector(const string& name, ShType* iElementType)
        : ShType(name, typeVector), elementType(iElementType)  { }

string ShVector::getFullDefinition(const string& objName) const
    { return elementType->getDefinition(objName) + "[]"; }

string ShVector::displayValue(const ShValue& v) const
{
    if (isString())
        return "'" + mkPrintable(PTR_TO_STRING(v.value.ptr_)) + "'";
    else
    {
        string s;
        char* p = pchar(v.value.ptr_);
        int elemSize = elementType->staticSize();
        if (elemSize == 0)
            return "[]";
        int count = PTR_TO_STRING(p).size() / elemSize;
        for (; count > 0 ; count--, p += elemSize)
        {
            if (!s.empty() > 0)
                s += ", ";
            ShValue v;
            v.assignFromBuf(elementType, p);
            s += elementType->displayValue(v);
        }
        return '[' + s + ']';
    }
}


// --- ARRAY TYPE --- //

ShArray::ShArray(ShType* iElementType, ShType* iIndexType)
        : ShVector(iElementType), indexType(iIndexType)  { }

string ShArray::getFullDefinition(const string& objName) const
{
    return elementType->getDefinition(objName) + "[" + indexType->getDefinition() + "]";
}

string ShArray::displayValue(const ShValue& v) const
    { notImpl(); return "null"; }



// --- SET TYPE --- //

ShSet::ShSet(ShVoid* iElementType, ShType* iIndexType)
        : ShArray(iElementType, iIndexType)  { }

string ShSet::displayValue(const ShValue& v) const
    { notImpl(); return "null"; }


// --- REFERENCE TYPE --- //

ShReference::ShReference(ShType* iBase)
    : ShType(typeReference), base(iBase)  { }


string ShReference::getFullDefinition(const string& objName) const
    { return base->getDefinition(objName) + '^'; }


string ShReference::displayValue(const ShValue& v) const
    { return base->displayValue(v); }


// --- STATE --- //

/*
void ShState::addState(ShState* obj)
        { states.add(obj); addSymbol(obj); }

void ShState::addArgument(ShArgument* obj)
        { args.add(obj); addSymbol(obj); }

string ShState::getArgsDefinition() const
{
    string result = '(';
    if (!args.empty())
    {
        result += args[0]->type->getDefinition(args[0]->name);
        for (int i = 1; i < args.size(); i++)
            result += ", " + args[i]->type->getDefinition(args[0]->name);
    }
    result += ')';
    return result;
}

string ShState::getFullDefinition(const string& objName) const
{
    return "state " + objName + getArgsDefinition();
}
*/


// --- LITERAL VALUE --- //

ShValue::ShValue(const ShValue& v)
    : type(v.type)
{
    if (type != NULL && type->isVector())
        value.ptr_ = PTR_TO_STRING(v.value.ptr_)._initialize();
    else
        value = v.value;
}

void ShValue::_finalize()
{
    if (type != NULL && type->isVector())
        // TODO: finalize non-POD vectors
        string::_finalize(value.ptr_);
}

void ShValue::assignInt(ShType* iType, int i)
        { _finalize(); type = iType; value.int_ = i; }
void ShValue::assignLarge(ShType* iType, large l)
        { _finalize(); type = iType; value.large_ = l; }
void ShValue::assignPtr(ShType* iType, ptr p)
        { _finalize(); type = iType; value.ptr_ = p; }
void ShValue::assignVec(ShType* iType, const string& s)
        { _finalize(); type = iType; value.ptr_ = s._initialize(); }
void ShValue::assignVoid(ShType* iType)
        { _finalize(); type = iType; value.ptr_ = 0; }

void ShValue::assignFromBuf(ShType* newType, const ptr p)
{
    switch (newType->storageModel())
    {
        case stoByte: assignInt(newType, int(*puchar(p))); break;
        case stoInt: assignInt(newType, *pint(p)); break;
        case stoLarge: assignLarge(newType, *plarge(p)); break;
        case stoPtr: assignPtr(newType, *pptr(p)); break;
        case stoVec: assignVec(newType, PTR_TO_STRING(*pptr(p))); break;
        default: internal(12);
    };
}

void ShValue::assignToBuf(ptr p)
{
    switch (type->storageModel())
    {
        case stoByte: *puchar(p) = value.int_; break;
        case stoInt: *pint(p) = value.int_; break;
        case stoLarge: *plarge(p) = value.large_; break;
        case stoPtr: *pptr(p) = p; break;
        case stoVec: *pptr(p) = string::_initialize(value.ptr_); break;
        default: internal(13);
    }
}


// --- CONSTANT --- //

ShConstant::ShConstant(const string& name, const ShValue& iValue)
    : ShBase(name, baseConstant), value(iValue)  { }

ShConstant::ShConstant(const string& name, ShEnum* type, int value)
    : ShBase(name, baseConstant), value(type, value)  { }


// ------------------------------------------------------------------------ //


// --- LOCAL SCOPE --- //

ShLocalScope::ShLocalScope(VmCodeGen* iCodeGen)
    : ShScope("", typeLocalScope), codegen(iCodeGen)  { }

string ShLocalScope::getFullDefinition(const string& objName) const
    { return "@localscope"; }

void ShLocalScope::addVariable(ShVariable* var, ShSymScope* symScope)
{
    var->dataOffset = codegen->genReserveLocalVar(var->type);
    var->local = true;
    ShScope::addVariable(var, symScope);
}


// --- MODULE --- //


ShModule::ShModule(const string& iFileName)
    : ShScope(extractFileName(iFileName), typeModule), fileName(iFileName),
      parser(iFileName), codegen(new VmCodeGen), modLocalScope(codegen),
      symbolScope(this), varScope(this), compiled(false),
      // runtime
      dataSize(0), dataSegment(NULL)
{
    if (queenBee != NULL)
        addUses(queenBee);
    registerString(fileName); // file name can be used in 'assert' statements
}

ShModule::~ShModule()
{
    delete codegen;
    memfree(dataSegment);
}

void ShModule::error(EDuplicate& e)
{
    parser.error("'" + e.getEntry() + "' is already defined within this scope");
}

void ShModule::addVariable(ShVariable* obj, ShSymScope* symScope)
{
    obj->dataOffset = dataSize;
    dataSize += obj->type->staticSizeAligned();
    ShScope::addVariable(obj, symScope);
}

string ShModule::getFullDefinition(const string& objName) const
    { return name; }

#ifdef DEBUG
void ShModule::dump(string indent) const
{
    printf("\n%smodule %s\n", indent.c_str(), name.c_str());
    ShScope::dump(indent);
    modLocalScope.dump("");
}
#endif


void ShModule::setupRuntime()
{
    if (dataSize > 0)
        dataSegment = pchar(memalloc(dataSize));
    mainCode = codegen->getCodeSeg();
    compiled = true;
}


void ShModule::execute()
{
    if (!compiled)
        internal(14);
    mainCode.execute(dataSegment, NULL);
}


// --- SYSTEM MODULE --- //

ShQueenBee::ShQueenBee()
    : ShModule("System"),
      defaultInt(new ShInteger("int", INT_MIN, INT_MAX)),
      defaultLarge(new ShInteger("large", LLONG_MIN, LLONG_MAX)),
      defaultChar(new ShChar("char", 0, 255)),
      defaultStr(new ShVector("str", defaultChar)),
      defaultBool(new ShBool("bool")),
      defaultVoid(new ShVoid("void")),
      defaultTypeRef(new ShTypeRef("typeref")),
      defaultEmptyVec(new ShVector(defaultVoid))
{
    addType(defaultInt, this);
    addType(defaultLarge, this);
    addType(defaultChar, this);
    addType(defaultStr, this);
    defaultChar->setDerivedVectorTypePleaseThisIsCheatingIKnow(defaultStr);
    addType(defaultBool, this);
    addType(defaultVoid, this);
    addType(defaultTypeRef, this);
    addAnonType(defaultEmptyVec);

    setupRuntime();
}


// ------------------------------------------------------------------------ //


ShQueenBee* queenBee;
BaseTable<ShModule> moduleTable;
BaseList<ShModule> moduleList;


void initLangObjs()
{
    registerModule(queenBee = new ShQueenBee());
}


void doneLangObjs()
{
    moduleTable.clear();
    moduleList.clear();
    queenBee = NULL;
}


ShModule* findModule(const string& name)
{
    return moduleTable.find(name);
}


void registerModule(ShModule* module)
{
    moduleList.add(module);
    moduleTable.addUnique(module);
}


