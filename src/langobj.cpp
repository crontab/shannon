
#include "langobj.h"


// --- BASIC LANGUAGE OBJECTS ---------------------------------------------- //

ShType::~ShType()  { }


ShTypeAlias::ShTypeAlias(const string& name, ShType* iBase)
        : ShBase(name), base(iBase)  { }


// --- VARIABLE --- //

ShVector* ShType::getVectorType()
{
    if (vector == NULL)
        vector = new ShVector(this);
    return vector;
}


ShVariable::ShVariable(ShType* iType)
        : ShBase(), type(iType)  { }

ShVariable::ShVariable(const string& name, ShType* iType)
        : ShBase(name), type(iType)  { }

ShArgument::ShArgument(const string& name, ShType* iType)
        : ShVariable(name, type)  { }


// --- SCOPE --- //

ShScope::ShScope()
        : ShType(), complete(false)  { }

ShScope::ShScope(const string& name)
        : ShType(name), complete(false)  { }

void ShScope::addSymbol(ShBase* obj) throw(EDuplicate)
{
    if (obj->owner != NULL)
        throw EInternal(3, "ShScope::addSymbol(): obj->owner != NULL");
    if (obj->name.empty())
        throw EInternal(3, "ShScope::addSymbol(): obj->name is empty");
    symbols.addUnique(obj);
    *(ShScope**)&obj->owner = this; // mute the const field
}

void ShScope::addType(ShType* obj) throw(EDuplicate)
        { addSymbol(obj); types.add(obj); }


void ShScope::addVar(ShVariable* obj) throw(EDuplicate)
        { addSymbol(obj); vars.add(obj); }

void ShScope::addTypeAlias(ShTypeAlias* obj) throw(EDuplicate)
        { addSymbol(obj); typeAliases.add(obj); }


ShBase* ShScope::deepSearch(const string& name) const throw(ENotFound)
{
    ShBase* obj = find(name);
    if (obj != NULL)
        return obj;
    if (owner != NULL)
        return owner->deepSearch(name);
    throw ENotFound(name);
}


// --- STATE --- //

void ShState::addState(ShState* obj)
        { addSymbol(obj);  states.add(obj); }

void ShState::addArgument(ShArgument* obj)
        { addSymbol(obj);  args.add(obj); }


// --- MODULE --- //

ShModule::ShModule(const string& name)
            : ShScope(name)  { }


// --- LANGUAGE TYPES ----------------------------------------------------- //


inline int align(int size)
        { return ((size / memAlign) + 1) * memAlign; }


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


ShVector::ShVector(ShType* iElementType)
        : ShType(), elementType(iElementType)  { }

ShVector::ShVector(const string& name, ShType* iElementType)
        : ShType(name), elementType(iElementType)  { }


// --- SYSTEM MODULE --- //

ShQueenBee::ShQueenBee()
    : ShModule("System"),
      defaultInt(new ShInteger("int", int32min, int32max)),
      defaultLarge(new ShInteger("large", int64min, int64max)),
      defaultChar(new ShChar("char")),
      defaultString(defaultChar->getVectorType()),  // owned by defaultChar
      defaultStr(new ShTypeAlias("str", defaultString)),
      defaultBool(new ShBool("bool"))
{
    addType(defaultInt);
    addType(defaultLarge);
    addType(defaultChar);
    addTypeAlias(defaultStr);
    addType(defaultBool);
}

