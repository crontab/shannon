
#include <stdio.h>

#include "langobj.h"


inline int align(int size)
        { return ((size / memAlign) + 1) * memAlign; }

static void notImpl()
{
    throw EInternal(11, "Feature not implemented");
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
    : ShBase(name, baseType), typeId(iTypeId), derivedVectorType(NULL), derivedSetType(NULL)  { }

ShType::~ShType()  { }

string ShType::getDisplayName(const string& objName) const
{
    if (!name.empty())
        return name + (objName.empty() ? "" : " " + objName);
    else
        return getFullDefinition(objName);
}

bool ShType::canAssign(const ShValue& value) const
{
    return equals(value.type);
}

ShVector* ShType::deriveVectorType(ShScope* scope)
{
    if (derivedVectorType == NULL)
    {
        derivedVectorType = new ShVector(this);
        scope->addAnonType(derivedVectorType);
    }
    return derivedVectorType;
}

ShArray* ShType::deriveArrayType(ShType* indexType, ShScope* scope)
{
    if (!indexType->canBeArrayIndex())
        throw EInternal(10, indexType->getDisplayName("") + " can't be used as array index");
    if (isBool())
        return indexType->deriveSetType((ShBool*)this, scope);
    else
    {
        ShArray* array = new ShArray(this, indexType);
        scope->addAnonType(array);
        return array;
    }
}

ShSet* ShType::deriveSetType(ShBool* elementType, ShScope* scope)
{
    if (derivedSetType == NULL)
    {
        derivedSetType = new ShSet(elementType, this);
        scope->addAnonType(derivedSetType);
    }
    return derivedSetType;
}


// --- TYPE ALIAS --- //

ShTypeAlias::ShTypeAlias(const string& name, ShType* iBase)
    : ShBase(name, baseTypeAlias), base(iBase)  { }


// --- VARIABLE --- //

ShVariable::ShVariable(ShType* iType)
    : ShBase(baseVariable), type(iType)  { }

ShVariable::ShVariable(const string& name, ShType* iType)
    : ShBase(name, baseVariable), type(iType)  { }

ShArgument::ShArgument(const string& name, ShType* iType)
    : ShVariable(name, type)  { }


// --- CONSTANT --- //

ShConstant::ShConstant(const string& name, const ShValue& iValue)
    : ShBase(name, baseConstant), value(iValue)  { }


// --- SCOPE --- //

ShScope::ShScope(const string& name, ShTypeId iTypeId)
        : ShType(name, iTypeId), complete(false)  { }

ShBase* ShScope::own(ShBase* obj)
{
    if (obj->owner != NULL)
        throw EInternal(3, "obj->owner != NULL");
    obj->owner = this;
    return obj;
}

void ShScope::addSymbol(ShBase* obj)
{
    own(obj);
    if (obj->name.empty())
        throw EInternal(4, "obj->name is empty");
    symbols.addUnique(obj);
}

void ShScope::addUses(ShModule* obj)
        { addSymbol(obj); uses.add(obj); }

void ShScope::addType(ShType* obj)
        { addSymbol(obj); types.add(obj); }

void ShScope::addAnonType(ShType* obj)
        { own(obj); types.add(obj); }

void ShScope::addVariable(ShVariable* obj)
        { addSymbol(obj); vars.add(obj); }

void ShScope::addAnonVar(ShVariable* obj)
        { own(obj); vars.add(obj); }

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
        printf("%s# def %s\n", indent.c_str(), types[i]->getDisplayName("*").c_str());
    for (int i = 0; i < typeAliases.size(); i++)
        printf("%sdef %s\n", indent.c_str(),
            typeAliases[i]->base->getDisplayName(typeAliases[i]->name).c_str());
    for (int i = 0; i < vars.size(); i++)
        printf("%s%s\n", indent.c_str(),
            vars[i]->type->getDisplayName(vars[i]->name).c_str());
    for (int i = 0; i < consts.size(); i++)
    {
        ShConstant* c = consts[i];
        ShType* t = c->value.type;
        printf("%sconst %s = %s\n", indent.c_str(),
            t->getDisplayName(c->name).c_str(),
            t->displayValue(c->value).c_str());
    }
}
#endif


// --- LANGUAGE TYPES ----------------------------------------------------- //


ShOrdinal::ShOrdinal(const string& name, ShTypeId iTypeId, large min, large max)
    : ShType(name, iTypeId), range(min, max), size(range.physicalSize())  { }


// --- INTEGER TYPE --- //

int Range::physicalSize() const
{
    if (min >= 0)
    {
        if (max <= uint8max)
            return 1;
        if (max <= uint16max)
            return 2;
        if (max <= uint32max)
            return 4;
        return 8;
    }
    if (min == int64min)
        return 8;
    large t = ~min;
    if (max > t)
        t = max;
    if (t <= int8max)
        return 1;
    if (t <= int16max)
        return 2;
    if (t <= int32max)
        return 4;
    return 8;
}


ShInteger::ShInteger(const string& name, large min, large max)
    : ShOrdinal(name, typeInt, min, max)  { }

string ShInteger::getFullDefinition(const string& objName) const
{
    return itostring(range.min) + ".." + itostring(range.max)
        + ' ' + objName;
}

string ShInteger::displayValue(const ShValue& v) const
    { return itostring(isLarge() ? v.value.large_ : v.value.int_); }


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


// --- BOOL TYPE --- //

ShBool::ShBool(const string& name)
    : ShOrdinal(name, typeBool, 0, 1)  { }

string ShBool::getFullDefinition(const string& objName) const
    { return "false..true" + objName; }

string ShBool::displayValue(const ShValue& v) const
    { return v.value.int_ ? "true" : "false"; }



// --- VOID TYPE --- //

ShVoid::ShVoid(const string& name)
    : ShType(name, typeVoid)  { }

string ShVoid::getFullDefinition(const string& objName) const
    { throw EInternal(6, "anonymous void type"); }

string ShVoid::displayValue(const ShValue& v) const
    { return "null"; }


// --- VECTOR TYPE --- //

ShVector::ShVector(ShType* iElementType)
        : ShType(typeVector), elementType(iElementType)  { }

ShVector::ShVector(const string& name, ShType* iElementType)
        : ShType(name, typeVector), elementType(iElementType)  { }

string ShVector::getFullDefinition(const string& objName) const
    { return elementType->getDisplayName(objName) + "[]"; }

string ShVector::displayValue(const ShValue& v) const
    { notImpl(); return "null"; }


// --- STRING TYPE --- //

ShString::ShString(const string& name, ShChar* elementType)
    : ShVector(name, elementType)  { }

string ShString::displayValue(const ShValue& v) const
    { return PTR_TO_STRING(v.value.ptr_); }



// --- ARRAY TYPE --- //

ShArray::ShArray(ShType* iElementType, ShType* iIndexType)
        : ShVector(iElementType), indexType(iIndexType)  { }

string ShArray::getFullDefinition(const string& objName) const
{
    return elementType->getDisplayName(objName) + "[" + indexType->getDisplayName("") + "]";
}

string ShArray::displayValue(const ShValue& v) const
    { notImpl(); return "null"; }



// --- SET TYPE --- //

ShSet::ShSet(ShBool* iElementType, ShType* iIndexType)
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
        result += args[0]->type->getDisplayName(args[0]->name);
        for (int i = 1; i < args.size(); i++)
            result += ", " + args[i]->type->getDisplayName(args[0]->name);
    }
    result += ')';
    return result;
}

string ShState::getFullDefinition(const string& objName) const
{
    return "state " + objName + getArgsDefinition();
}
*/


// ------------------------------------------------------------------------ //


// --- MODULE --- //


ShModule::ShModule(const string& iFileName)
    : ShScope(extractFileName(iFileName), typeModule), fileName(iFileName),
      parser(iFileName), currentScope(NULL), compiled(false)
{
    if (queenBee != NULL)
        addUses(queenBee);
}

string ShModule::registerString(const string& v)
{
    stringLiterals.add(v);
    return v;
}

string ShModule::getFullDefinition(const string& objName) const
{
    throw EInternal(5, "anonymous module");
}

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
      defaultInt(new ShInteger("int", int32min, int32max)),
      defaultLarge(new ShInteger("large", int64min, int64max)),
      defaultChar(new ShChar("char")),
      defaultStr(new ShString("str", defaultChar)),
      defaultBool(new ShBool("bool")),
      defaultVoid(new ShVoid("void"))
{
    addType(defaultInt);
    addType(defaultLarge);
    addType(defaultChar);
    addType(defaultBool);
    addType(defaultVoid);
    addType(defaultStr);
    defaultChar->setDerivedVectorTypePleaseThisIsCheatingIKnow(defaultStr);
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


