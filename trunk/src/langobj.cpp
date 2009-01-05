
#include <stdio.h>
#include <limits.h>

#include "langobj.h"
#include "codegen.h"


static void notImpl()
{
    throw Exception("Feature not implemented");
}


// --- BASIC LANGUAGE OBJECTS ---------------------------------------------- //


ShBase::ShBase(ShBaseId iBaseId)
    : BaseNamed(), baseId(iBaseId)  { }

ShBase::ShBase(const string& iName, ShBaseId iBaseId)
    : BaseNamed(iName), baseId(iBaseId)  { }


// --- TYPE --- //


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

offs memAlign(offs size)
{
    if (size == 0)
        return 0;
    else
        return (((size - 1) / DATA_MEM_ALIGN) + 1) * DATA_MEM_ALIGN;
}


ShType::ShType(ShTypeId iTypeId)
    : ShBase(baseType), typeId(iTypeId), owner(NULL),
      derivedVectorType(NULL), derivedSetType(NULL), derivedRefType(NULL),
      storageModel(typeToSto[iTypeId]), staticSize(stoToSize[storageModel]),
      staticSizeAligned(memAlign(staticSize))  { }

ShType::ShType(const string& iName, ShTypeId iTypeId)
    : ShBase(iName, baseType), typeId(iTypeId), owner(NULL),
      derivedVectorType(NULL), derivedSetType(NULL), derivedRefType(NULL),
      storageModel(typeToSto[iTypeId]), staticSize(stoToSize[storageModel]),
      staticSizeAligned(memAlign(staticSize))  { }

void ShType::setTypeId(ShTypeId iTypeId)
{
    typeId = iTypeId;
    // mute 'const'. sorry, sorry, sorry.
    *(StorageModel*)&storageModel = typeToSto[iTypeId];
    *(offs*)&staticSize = stoToSize[storageModel];
    *(offs*)&staticSizeAligned = memAlign(staticSize);
}

ShType::~ShType()  { }


void ShType::setOwner(ShScope* newOwner)
{
    if (owner != NULL)
        internal(3);
    owner = newOwner;
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

// --- SYMBOLS-ONLY SCOPE --- //

ShSymScope::ShSymScope(const string& iName, ShTypeId iTypeId, ShSymScope* iParent)
        : ShType(iName, iTypeId), parent(iParent)  { }

void ShSymScope::addSymbol(ShBase* obj)
{
    if (obj->name.empty())
        internal(4);
    symbols.addUnique(obj);
}

void ShSymScope::finalizeVars(VmCodeGen* codegen)
{
    for (int i = symbols.size() - 1; i >= 0; i--)
    {
        ShBase* obj = symbols[i];
        if (obj->isVariable())
            codegen->genFinVar((ShVariable*)obj);
    }
}

ShBase* ShSymScope::deepFind(const string& ident) const
{
    ShBase* obj = find(ident);
    if (obj != NULL)
        return obj;
    for (int i = uses.size() - 1; i >= 0; i--)
    {
        obj = uses[i]->find(ident);
        if (obj != NULL)
            return obj;
    }
    if (parent != NULL)
        return parent->deepFind(ident);
    return NULL;
}

void ShSymScope::addUses(ShModule* obj)
        { uses.add(obj); addSymbol(obj); }


// --- SCOPE --- //

ShScope::ShScope(const string& iName, ShTypeId iTypeId, ShSymScope* iParent)
        : ShSymScope(iName, iTypeId, iParent)  { }

ShScope::~ShScope()
{
    // Order is important
    consts.clear();
    vars.clear();
    types.clear();
}

void ShScope::addAnonType(ShType* obj)
{
    types.add(obj);
    obj->setOwner(this);
}

void ShScope::addTypeAlias(const string& ident, ShType* type, ShSymScope* symScope)
{
    ShConstant* obj = new ShConstant(ident, queenBee->defaultTypeRef, type);
    consts.add(obj);
    symScope->addSymbol(obj);
}

void ShScope::addConstant(ShConstant* obj, ShSymScope* symScope)
{
    consts.add(obj);
    symScope->addSymbol(obj);
}

#ifdef DEBUG
void ShScope::dump(string indent) const
{
    for (int i = 0; i < types.size(); i++)
        printf("%s# def %s\n", indent.c_str(), types[i]->getDefinition("*").c_str());
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


// --- VARIABLE --- //

ShVariable::ShVariable(const string& iName, ShType* iType, ShScope* iOwnerScope, offs iOffs)
    : ShBase(iName, baseVariable), type(iType), ownerScope(iOwnerScope),
      dataOffset(iOffs)  { }


// --- LANGUAGE TYPES ----------------------------------------------------- //

EInvalidSubrange::EInvalidSubrange(ShOrdinal* type)
    : Exception("Invalid subrange for " + type->getDefinition())  { }
    

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

ShOrdinal::ShOrdinal(const string& iName, ShTypeId iTypeId, large min, large max)
    : ShType(iName, iTypeId), derivedRangeType(NULL), range(min, max)  { }

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
        if (v.type->isLargeInt())
            return v.value.large_ >= range.min && v.value.large_ <= range.max;
        else
            return v.value.int_ >= range.min && v.value.int_ <= range.max;
    }
    else
        internal(5);
    return false;
}


// --- INTEGER TYPE --- //

ShInteger::ShInteger(const string& iName, large min, large max)
    : ShOrdinal(iName, typeInt32, min, max)
{
    int size = range.physicalSize();
    if (size == 1)
        setTypeId(typeInt8);
    else if (size == 8)
        setTypeId(typeInt64);
}


string ShInteger::getFullDefinition(const string& objName) const
{
    string s = itostring(range.min) + ".." + itostring(range.max);
    if (!objName.empty()) s += ' ' + objName;
    return s;
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

ShChar::ShChar(const string& iName, int min, int max)
    : ShOrdinal(iName, typeChar, min, max)  { }

string ShChar::getFullDefinition(const string& objName) const
{
    string s = "'" + mkPrintable(range.min)  + "'..'" + mkPrintable(range.max) + "'";
    if (!objName.empty()) s += ' ' + objName;
    return s;
}

string ShChar::displayValue(const ShValue& v) const
    { return "'" + mkPrintable(v.value.int_) + "'"; }

ShOrdinal* ShChar::cloneWithRange(large min, large max)
    { return new ShChar(emptystr, min, max); }


// --- ENUM TYPE --- //

ShEnum::ShEnum(const string& iName)
    : ShOrdinal(iName, typeEnum, 0, 0)  { }

ShEnum::ShEnum(const BaseTable<ShConstant>& t, int min, int max)
    : ShOrdinal(typeEnum, min, max), values(t)  { }

void ShEnum::finish()
{
    int max = values.size() - 1;
    if (max >= 256)
        internal(15);
    reassignMax(max);
}

string ShEnum::getFullDefinition(const string& objName) const
{
    string s = values[range.min]->name + ".." + values[range.max]->name;
    if (!objName.empty()) s += ' ' + objName;
    return s;
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

ShBool::ShBool(const string& iName)
    : ShOrdinal(iName, typeBool, 0, 1)  { }

string ShBool::getFullDefinition(const string& objName) const
{
    string s = "false..true";
    if (!objName.empty()) s += ' ' + objName;
    return s;
}

string ShBool::displayValue(const ShValue& v) const
    { return v.value.int_ ? "true" : "false"; }

ShOrdinal* ShBool::cloneWithRange(large min, large max)
    { throw EInvalidSubrange(this); }



// --- VOID TYPE --- //

ShVoid::ShVoid(const string& iName)
    : ShType(iName, typeVoid)  { }

string ShVoid::getFullDefinition(const string& objName) const
    { return "void"; }

string ShVoid::displayValue(const ShValue& v) const
    { return "null"; }


// --- TYPEREF TYPE --- //

ShTypeRef::ShTypeRef(const string& iName)
    : ShType(iName, typeTypeRef)  { }

string ShTypeRef::getFullDefinition(const string& objName) const
    { return "typeref"; }

string ShTypeRef::displayValue(const ShValue& v) const
{
    ShType* type = PType(v.value.ptr_);
    // TODO: display subranges with typeof(...)
    return type->getDefinition();
}


// --- RANGE TYPE --- //

ShRange::ShRange(ShOrdinal* iBase)
    : ShType(typeRange), base(iBase)  { }

ShRange::ShRange(const string& iName, ShOrdinal* iBase)
    : ShType(iName, typeTypeRef), base(iBase)  { }

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

ShVector::ShVector(const string& iName, ShType* iElementType)
        : ShType(iName, typeVector), elementType(iElementType)  { }

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
        int elemSize = elementType->staticSize;
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
        : ShType(typeArray), elementType(iElementType), indexType(iIndexType)  { }

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


// --- LITERAL VALUE --- //

ShValue::ShValue(ShType* iType, const podvalue& iValue)
    : type(iType), value(iValue)
{
    if (type != NULL && type->isVector())
        PTR_TO_STRING(value.ptr_)._initialize();
}

void ShValue::_finalize()
{
    if (type != NULL && type->isVector())
        if (PVector(type)->isPodVector())
            string::_finalize(value.ptr_);
        else
            finalize(type, &value.ptr_);
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

void ShValue::assignFromBuf(ShType* newType, ptr p)
{
    switch (newType->storageModel)
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
    switch (type->storageModel)
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

ShConstant::ShConstant(const string& iName, const ShValue& iValue)
    : ShBase(iName, baseConstant), value(iValue.type, iValue.value)  { }

ShConstant::ShConstant(const string& iName, ShEnum* type, int iValue)
    : ShBase(iName, baseConstant), value(type, iValue)  { }

ShConstant::ShConstant(const string& iName, ShTypeRef* typeref, ShType* iValue)
    : ShBase(iName, baseConstant), value(typeref, iValue)  { }


// ------------------------------------------------------------------------ //


// --- LOCAL SCOPE --- //

ShLocalScope::ShLocalScope(const string& iName, ShSymScope* iParent)
    : ShScope(iName, typeLocalScope, iParent)  { }

string ShLocalScope::getFullDefinition(const string& objName) const
    { return "@localscope"; }

ShVariable* ShLocalScope::addVariable(const string& ident, ShType* type,
    ShSymScope* symScope, VmCodeGen* codegen)
{
    offs offset = 0;
    if (codegen != NULL)
        offset = codegen->genReserveLocalVar(type);
    ShVariable* var = new ShVariable(ident, type, this, offset);
    vars.add(var);
    symScope->addSymbol(var);
    return var;
}


// --- STATE BASE --- //

ShStateBase::ShStateBase(const string& iName, ShTypeId iTypeId, ShSymScope* iParent)
    : ShScope(iName, iTypeId, iParent), localScope("", this)  { }


// --- FUNCTION --- //

ShFunction::ShFunction(ShType* iReturnType, ShSymScope* iParent)
    : ShStateBase("", typeFunction, iParent), 
      // why symbol scope for the return var is "this": because we don't want
      // it to be finalized upon function return
      returnVar(localScope.addVariable("result", iReturnType, this, NULL))  { }

ShVariable* ShFunction::addVariable(const string& ident, ShType* type,
        ShSymScope* symScope, VmCodeGen* codegen)
{
    return localScope.addVariable(ident, type, symScope, codegen);
}

void ShFunction::addArgument(const string& ident, ShType* type)
{
    ShVariable* arg = new ShVariable(ident, type, this, 0);
    args.add(arg);
    localScope.addSymbol(arg);
}

void ShFunction::finishArguments()
{
    offs offset = 0;
    for (int i = args.size() - 1; i >= 0; i--)
    {
        ShVariable* arg = args[i];
        offset -= arg->type->staticSizeAligned;
        arg->dataOffset = offset;
    }
    offset -= returnVar->type->staticSizeAligned;
    returnVar->dataOffset = offset;
}

string ShFunction::getArgDefs() const
{
    string result = '(';
    if (!args.empty())
    {
        result += args[0]->type->getDefinition();
        for (int i = 1; i < args.size(); i++)
            result += ", " + args[i]->type->getDefinition();
    }
    result += ')';
    return result;
}

string ShFunction::getFullDefinition(const string& objName) const
{
    string o = objName.empty() ? "*" : objName;
    return returnVar->type->getDefinition() + ' ' + o
        + getArgDefs();
}


// --- MODULE --- //


ShModule::ShModule(const string& iName)
    : ShStateBase(iName, typeModule, NULL), dataSize(0)
{
    if (queenBee != NULL)
        addUses(queenBee);
}

ShModule::~ShModule()
{
}

ShVariable* ShModule::addVariable(const string& ident, ShType* type,
    ShSymScope* symScope, VmCodeGen* codegen)
{
    ShVariable* var = new ShVariable(ident, type, this, dataSize);
    vars.add(var);
    symScope->addSymbol(var);
    dataSize += var->type->staticSizeAligned;
    return var;
}


string ShModule::getFullDefinition(const string& objName) const
    { return name; }


#ifdef DEBUG
void ShModule::dump(string indent) const
{
    printf("\n%smodule %s\n", indent.c_str(), name.c_str());
    ShScope::dump(indent);
}
#endif


void ShModule::execute()
{
    pchar dataSegment = NULL;
    if (dataSize > 0)
        dataSegment = pchar(memalloc(dataSize));
    try
    {
        codeseg.execute(dataSegment, NULL);
    }
    catch (...)
    {
        memfree(dataSegment);
        throw;
    }
}


// --- SYSTEM MODULE --- //

ShQueenBee::ShQueenBee()
    : ShModule("System"),
      defaultTypeRef(new ShTypeRef("typeref")),
      defaultInt(new ShInteger("int", INT_MIN, INT_MAX)),
      defaultLarge(new ShInteger("large", LLONG_MIN, LLONG_MAX)),
      defaultChar(new ShChar("char", 0, 255)),
      defaultStr(new ShVector("str", defaultChar)),
      defaultBool(new ShBool("bool")),
      defaultVoid(new ShVoid("void")),
      defaultEmptyVec(new ShVector(defaultVoid))
{
    addAnonType(defaultInt);
    addAnonType(defaultLarge);
    addAnonType(defaultChar);
    addAnonType(defaultStr);
    defaultChar->setDerivedVectorTypePleaseThisIsCheatingIKnow(defaultStr);
    addAnonType(defaultBool);
    addAnonType(defaultVoid);
    addAnonType(defaultTypeRef);
    addAnonType(defaultEmptyVec);
}


void ShQueenBee::setup()
{
    addTypeAlias(defaultTypeRef->name, defaultTypeRef, this);
    addTypeAlias(defaultInt->name, defaultInt, this);
    addTypeAlias(defaultLarge->name, defaultLarge, this);
    addTypeAlias(defaultChar->name, defaultChar, this);
    addTypeAlias(defaultStr->name, defaultStr, this);
    addTypeAlias(defaultBool->name, defaultBool, this);
    addTypeAlias(defaultVoid->name, defaultVoid, this);
}


// ------------------------------------------------------------------------ //


ShQueenBee* queenBee;
BaseTable<ShModule> moduleTable;
BaseList<ShModule> moduleList;


void initLangObjs()
{
    registerModule(queenBee = new ShQueenBee());
    queenBee->setup();
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


