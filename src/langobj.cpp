
#include <stdio.h>
#include <limits.h>

#include "langobj.h"


// inline int align(int size)
//         { return ((size / memAlign) + 1) * memAlign; }

static void notImpl()
{
    throw EMessage("Feature not implemented");
}


// --- BASIC LANGUAGE OBJECTS ---------------------------------------------- //


ShBase::ShBase(ShBaseId iBaseId)
    : BaseNamed(), baseId(iBaseId), owner(NULL)  { }

ShBase::ShBase(const string& name, ShBaseId iBaseId)
    : BaseNamed(name), baseId(iBaseId), owner(NULL)  { }


// --- TYPE --- //


ShType::ShType(ShTypeId iTypeId)
    : ShBase(baseType), typeId(iTypeId),
      derivedVectorType(NULL), derivedSetType(NULL)  { }

ShType::ShType(const string& name, ShTypeId iTypeId)
    : ShBase(name, baseType), typeId(iTypeId), 
      derivedVectorType(NULL), derivedSetType(NULL)  { }

ShType::~ShType()  { }

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
        internal(10);
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


// --- TYPE ALIAS --- //

ShTypeAlias::ShTypeAlias(const string& name, ShType* iBase)
    : ShBase(name, baseTypeAlias), base(iBase)  { }


// --- VARIABLE --- //

ShVariable::ShVariable(ShType* iType)
    : ShBase(baseVariable), type(iType), scopeIndex(0)  { }

ShVariable::ShVariable(const string& name, ShType* iType)
    : ShBase(name, baseVariable), type(iType), scopeIndex(0)  { }

ShArgument::ShArgument(const string& name, ShType* iType)
    : ShVariable(name, type)  { }


// --- SCOPE --- //

ShScope::ShScope(const string& name, ShTypeId iTypeId)
        : ShType(name, iTypeId)  { }

ShScope::~ShScope()
{
    // Order is important
    typeAliases.clear();
    consts.clear();
    vars.clear();
    types.clear();
}

ShBase* ShScope::own(ShBase* obj)
{
    if (obj->owner != NULL)
        internal(3);
    obj->owner = this;
    return obj;
}

void ShScope::addSymbol(ShBase* obj)
{
    own(obj);
    if (obj->name.empty())
        internal(4);
    symbols.addUnique(obj);
}

void ShScope::addUses(ShModule* obj)
        { addSymbol(obj); uses.add(obj); }

void ShScope::addType(ShType* obj)
        { addSymbol(obj); types.add(obj); }

void ShScope::addAnonType(ShType* obj)
        { own(obj); types.add(obj); }

void ShScope::addVariable(ShVariable* obj)
{
    addSymbol(obj);
    obj->scopeIndex = vars.size();
    vars.add(obj);
}

void ShScope::addAnonVar(ShVariable* obj)
{
    own(obj);
    obj->scopeIndex = vars.size();
    vars.add(obj);
}

void ShScope::addTypeAlias(ShTypeAlias* obj)
        { addSymbol(obj); typeAliases.add(obj); }

void ShScope::addConstant(ShConstant* obj)
        { addSymbol(obj); consts.add(obj); }

ShBase* ShScope::deepFind(const string& name) const
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
    if (owner != NULL)
        return owner->deepFind(name);
    return NULL;
}



#ifdef DEBUG
void ShScope::dump(string indent) const
{
    for (int i = 0; i < types.size(); i++)
        printf("%s# def %s\n", indent.c_str(), types[i]->getDefinition("*").c_str());
    for (int i = 0; i < typeAliases.size(); i++)
        printf("%sdef %s\n", indent.c_str(),
            typeAliases[i]->base->getDefinition(typeAliases[i]->name).c_str());
    for (int i = 0; i < vars.size(); i++)
        printf("%svar %s\n", indent.c_str(),
            vars[i]->type->getDefinition(vars[i]->name).c_str());
    for (int i = 0; i < consts.size(); i++)
    {
        ShConstant* c = consts[i];
        ShType* t = c->value.type;
        printf("%sconst %s = %s\n", indent.c_str(),
            t->getDefinition(c->name).c_str(),
            t->displayValue(c->value).c_str());
    }
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
        if (max <= UCHAR_MAX)
            return 1;
//        if (max <= USHRT_MAX)
//            return 2;
        if (max <= UINT_MAX)
            return 4;
        return 8;
    }
    if (min == LLONG_MIN)
        return 8;
    large t = ~min;
    if (max > t)
        t = max;
    if (t <= CHAR_MAX)
        return 1;
//    if (t <= SHRT_MAX)
//        return 2;
    if (t <= INT_MAX)
        return 4;
    return 8;
}


ShOrdinal::ShOrdinal(ShTypeId iTypeId, large min, large max)
    : ShType(iTypeId), derivedRangeType(NULL),
      range(min, max), size(range.physicalSize())  { }

ShOrdinal::ShOrdinal(const string& name, ShTypeId iTypeId, large min, large max)
    : ShType(name, iTypeId), derivedRangeType(NULL),
      range(min, max), size(range.physicalSize())  { }

ShRange* ShOrdinal::deriveRangeType()
{
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
        internal(40);
    return false;
}


// --- INTEGER TYPE --- //

ShInteger::ShInteger(large min, large max)
    : ShOrdinal(typeInt, min, max)  { }

ShInteger::ShInteger(const string& name, large min, large max)
    : ShOrdinal(name, typeInt, min, max)  { }

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
    { return new ShInteger(min, max); }


// --- CHAR TYPE --- //

ShChar::ShChar(int min, int max)
    : ShOrdinal(typeChar, min, max)  { }

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
    { return new ShChar(min, max); }


// --- ENUM TYPE --- //

ShEnum::ShEnum()
    : ShOrdinal(typeEnum, 0, 0)  { }

ShEnum::ShEnum(const BaseTable<ShConstant>& t, int min, int max)
    : ShOrdinal(typeEnum, min, max), values(t)  { }

void ShEnum::finish()
{
    range.max = values.size() - 1;
    recalcSize();
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
        int count = PTR_TO_STRING(p).size() / elemSize;
        for (; count > 0 ; count--, p += elemSize)
        {
            if (!s.empty() > 0)
                s += ", ";
            ShValue v;
            if (elementType->isLargePod())
                v.assignLarge(elementType, *plarge(p));
            else if (elementType->isVector())
                v.assignVec(elementType, PTR_TO_STRING(*pptr(p)));
            else if (elementType->isPodPointer())
                v.assignPtr(elementType, *pptr(p));
            else
                v.assignInt(elementType, *pint(p));
            s += elementType->displayValue(v);
        }
        return '[' + s + ']';
    }
}


// --- STRING TYPE --- //

ShString::ShString(const string& name, ShChar* elementType)
    : ShVector(name, elementType)  { }


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


// --- STATE --- //

/*
void ShState::addState(ShState* obj)
        { addSymbol(obj);  states.add(obj); }

void ShState::addArgument(ShArgument* obj)
        { addSymbol(obj);  args.add(obj); }

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

void ShValue::assignPtr(ShType* iType, ptr p)
        { _finalize(); type = iType; value.ptr_ = p; }
void ShValue::assignInt(ShType* iType, int i)
        { _finalize(); type = iType; value.int_ = i; }
void ShValue::assignLarge(ShType* iType, large l)
        { _finalize(); type = iType; value.large_ = l; }
void ShValue::assignVec(ShType* iType, const string& s)
        { _finalize(); type = iType; value.ptr_ = s._initialize(); }



// --- CONSTANT --- //

ShConstant::ShConstant(const string& name, const ShValue& iValue)
    : ShBase(name, baseConstant), value(iValue)  { }

ShConstant::ShConstant(const string& name, ShEnum* type, int value)
    : ShBase(name, baseConstant), value(type, value)  { }


// ------------------------------------------------------------------------ //


// --- MODULE --- //


ShModule::ShModule(const string& iFileName)
    : ShScope(extractFileName(iFileName), typeModule), fileName(iFileName),
      parser(iFileName), currentScope(NULL), compiled(false)
{
    if (queenBee != NULL)
        addUses(queenBee);
}

void ShModule::addObject(ShBase* obj)
{
    string objName = obj->name;
    try
    {
        if (obj->isType())
            currentScope->addType((ShType*)obj);
        else if (obj->isTypeAlias())
            currentScope->addTypeAlias((ShTypeAlias*)obj);
        else if (obj->isVariable())
            currentScope->addVariable((ShVariable*)obj);
        else if (obj->isConstant())
            currentScope->addConstant((ShConstant*)obj);
        else
            internal(11);
    }
    catch (EDuplicate& e)
    {
        delete obj;
        error("'" + objName + "' already defined within this scope");
    }
    catch (Exception& e)
    {
        delete obj;
        throw;
    }
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


// --- SYSTEM MODULE --- //

ShQueenBee::ShQueenBee()
    : ShModule("System"),
      defaultInt(new ShInteger("int", INT_MIN, INT_MAX)),
      defaultLarge(new ShInteger("large", LLONG_MIN, LLONG_MAX)),
      defaultChar(new ShChar("char", 0, 255)),
      defaultStr(new ShString("str", defaultChar)),
      defaultBool(new ShBool("bool")),
      defaultVoid(new ShVoid("void")),
      defaultTypeRef(new ShTypeRef("typeref"))
{
    addType(defaultInt);
    addType(defaultLarge);
    addType(defaultChar);
    addType(defaultStr);
    defaultChar->setDerivedVectorTypePleaseThisIsCheatingIKnow(defaultStr);
    addType(defaultBool);
    addType(defaultVoid);
    addType(defaultTypeRef);
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


