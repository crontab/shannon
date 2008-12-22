
#include <stdio.h>

#include "langobj.h"


inline int align(int size)
        { return ((size / memAlign) + 1) * memAlign; }


// --- BASIC LANGUAGE OBJECTS ---------------------------------------------- //


// --- TYPE --- //


ShType::ShType(): ShBase(), derivedVectorType(NULL), derivedSetType(NULL)  { }

ShType::ShType(const string& name)
    : ShBase(name), derivedVectorType(NULL), derivedSetType(NULL)  { }

ShType::~ShType()  { }

string ShType::getDisplayName(const string& objName) const
{
    if (!name.empty())
        return name + (objName.empty() ? "" : " " + objName);
    else
        return getFullDefinition(objName);
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
        throw EInternal(10, indexType->getDisplayName("*") + " can't be used as array index");
    ShArray* array = new ShArray(this, indexType);
    scope->addAnonType(array);
    return array;
}

ShSet* ShType::deriveSetType(ShScope* scope)
{
    if (!canBeArrayIndex())
        throw EInternal(10, getDisplayName("*") + " can't be used as set element");
    if (derivedSetType == NULL)
    {
        derivedSetType = new ShSet(this);
        scope->addAnonType(derivedSetType);
    }
    return derivedSetType;
}


// --- TYPE ALIAS --- //

ShTypeAlias::ShTypeAlias(const string& name, ShType* iBase)
        : ShBase(name), base(iBase)  { }


// --- VARIABLE --- //

ShVariable::ShVariable(ShType* iType)
        : ShBase(), type(iType)  { }

ShVariable::ShVariable(const string& name, ShType* iType)
        : ShBase(name), type(iType)  { }

ShArgument::ShArgument(const string& name, ShType* iType)
        : ShVariable(name, type)  { }


// --- SCOPE --- //

ShScope::ShScope(const string& name)
        : ShType(name), complete(false)  { }

ShBase* ShScope::own(ShBase* obj)
{
    if (obj->owner != NULL)
        throw EInternal(3, "obj->owner != NULL");
    obj->owner = this;
    return obj;
}

void ShScope::addSymbol(ShBase* obj) throw(EDuplicate)
{
    own(obj);
    if (obj->name.empty())
        throw EInternal(4, "obj->name is empty");
    symbols.addUnique(obj);
}

void ShScope::addUses(ShModule* obj)
        { addSymbol(obj); uses.add(obj); }

void ShScope::addType(ShType* obj) throw(EDuplicate)
        { addSymbol(obj); types.add(obj); }

void ShScope::addAnonType(ShType* obj)
        { own(obj); types.add(obj); }

void ShScope::addVar(ShVariable* obj) throw(EDuplicate)
        { addSymbol(obj); vars.add(obj); }

void ShScope::addAnonVar(ShVariable* obj)
        { own(obj); vars.add(obj); }

void ShScope::addTypeAlias(ShTypeAlias* obj) throw(EDuplicate)
        { addSymbol(obj); typeAliases.add(obj); }


ShBase* ShScope::deepSearch(const string& name) const throw(ENotFound)
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
        return owner->deepSearch(name);
    throw ENotFound(name);
}


void ShScope::dump(string indent) const
{
//    printf("%s%s:\n", indent.c_str(), getName().c_str());
    for (int i = 0; i < types.size(); i++)
        printf("%s# %s\n", indent.c_str(), types[i]->getDisplayName("*").c_str());
}


// --- LANGUAGE TYPES ----------------------------------------------------- //


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
    : ShType(name), range(min, max), size(range.physicalSize())
{
}

string ShInteger::getFullDefinition(const string& objName) const
{
    return itostring(range.min) + ".." + itostring(range.max)
        + ' ' + objName;
}


// --- CHAR TYPE --- //

string ShChar::getFullDefinition(const string& objName) const
{
    throw EInternal(8, "anonymous char type");
}


// --- BOOL TYPE --- //

string ShBool::getFullDefinition(const string& objName) const
{
    throw EInternal(7, "anonymous bool type");
}


// --- VOID TYPE --- //

string ShVoid::getFullDefinition(const string& objName) const
{
    throw EInternal(6, "anonymous void type");
}


// --- VECTOR TYPE --- //

ShVector::ShVector(ShType* iElementType)
        : ShType(), elementType(iElementType)  { }

ShVector::ShVector(const string& name, ShType* iElementType)
        : ShType(name), elementType(iElementType)  { }

string ShVector::getFullDefinition(const string& objName) const
{
    return elementType->getDisplayName(objName) + "[]";
}

bool ShVector::isComparable() const
{
    return elementType == queenBee->defaultChar;
}


// --- ARRAY TYPE --- //

ShArray::ShArray(ShType* iElementType, ShType* iIndexType)
        : ShVector(iElementType), indexType(iIndexType)  { }

ShArray::ShArray(const string& name, ShType* iElementType, ShType* iIndexType)
        : ShVector(name, iElementType), indexType(iIndexType)  { }

string ShArray::getFullDefinition(const string& objName) const
{
    return elementType->getDisplayName(objName) + "[" + indexType->getDisplayName("") + "]";
}


// --- SET TYPE --- //

ShSet::ShSet(ShType* iBaseType): ShType(), baseType(iBaseType)  { }

string ShSet::getFullDefinition(const string& objName) const
{
    return baseType->getDisplayName(objName) + "{}";
}


// --- STATE --- //

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


// --- LITERAL VALUES ----------------------------------------------------- //



// ------------------------------------------------------------------------ //


// --- MODULE --- //


ShModule::ShModule(const string& iFileName)
    : ShScope(extractFileName(iFileName)), fileName(iFileName),
      parser(iFileName), currentScope(NULL), compiled(false)
{
    if (queenBee != NULL)
        addUses(queenBee);
}

string ShModule::getFullDefinition(const string& objName) const
{
    throw EInternal(5, "anonymous module");
}

void ShModule::dump(string indent) const
{
    printf("\n%smodule %s\n", indent.c_str(), name.c_str());
    ShScope::dump(indent);
}


// --- SYSTEM MODULE --- //

ShQueenBee::ShQueenBee()
    : ShModule("System"),
      defaultInt(new ShInteger("int", int32min, int32max)),
      defaultLarge(new ShInteger("large", int64min, int64max)),
      defaultChar(new ShChar("char")),
      defaultString(NULL),
      defaultStr(NULL),
      defaultBool(new ShBool("bool")),
      defaultVoid(new ShVoid("void"))
{
    addType(defaultInt);
    addType(defaultLarge);
    addType(defaultChar);
    addType(defaultBool);
    addType(defaultVoid);
    defaultString = defaultChar->deriveVectorType(this);
    defaultStr = new ShTypeAlias("str", defaultString);
    addTypeAlias(defaultStr);
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


