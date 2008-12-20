
#include <stdio.h>

#include "charset.h"
#include "str.h"
#include "contain.h"
#include "except.h"
#include "source.h"
#include "baseobj.h"
#include "bsearch.h"


const large int8min   = -128LL;
const large int8max   = 127LL;
const large uint8max  = 255LL;
const large int16min  = -32768LL;
const large int16max  = 32767LL;
const large uint16max = 65535LL;
const large int32min  = -2147483648LL;
const large int32max  = 2147483647LL;
const large uint32max = 4294967295LL;
const large int64min  = LARGE_MIN;
const large int64max  = LARGE_MAX;
const int   memAlign  = sizeof(int);


// --- BASIC LANGUAGE OBJECTS ---------------------------------------------- //


class ShType;
class ShScope;
class ShState;
class ShVector;


class ShBase: public Base
{
public:
    ShScope* const owner;
    
    ShBase(): Base(), owner(NULL)  { }
    ShBase(const string& name): Base(name), owner(NULL)  { }
};


class ShType: public ShBase
{
    Auto<ShVector> vector;
public:
    ShType(): ShBase()  { }
    ShType(const string& name): ShBase(name)  { }
    virtual ~ShType();
    virtual bool isComplete() const  { return true; }
    ShVector* getVectorType();
};


class ShTypeAlias: public ShType
{
public:
    ShType* const base;
    ShTypeAlias(const string& name, ShType* iBase);
};


// --- VARIABLE --- //

class ShVariable: public ShBase
{
public:
    ShType* const type;

    ShVariable(ShType* iType);
    ShVariable(const string& name, ShType* iType);
    virtual bool isArgument()
            { return false; }
};


class ShArgument: public ShVariable
{
public:
    ShArgument(const string& name, ShType* iType);
    virtual bool isArgument()
            { return true; }
};


// --- SCOPE --- //

class ShScope: public ShType
{
protected:
    BaseTable<ShBase> symbols;
    BaseList<ShType> types;
    BaseList<ShVariable> vars;
    
    void addSymbol(ShBase* obj) throw(EDuplicate);

public:
    bool complete;

    ShScope();
    ShScope(const string& name);
    virtual bool isComplete() const
            { return complete; };
    void addAnonType(ShType* obj)
            { types.add(obj); }
    void addType(ShType* obj) throw(EDuplicate);
    void addAnonVar(ShVariable* obj) throw(EDuplicate)
            { vars.add(obj); }
    void addVar(ShVariable* obj) throw(EDuplicate);
    void setCompleted()
            { complete = true; }
    ShBase* find(const string& name) const
            { return symbols.find(name); }
    ShBase* deepSearch(const string&) const throw(ENotFound);
};


// --- STATE --- //

class ShState: public ShScope
{
protected:
    BaseList<ShArgument> args;
    BaseList<ShState> states;

public:
    void addArgument(ShArgument*);
    void addState(ShState*);
};


// --- FUNCTION --- //

class ShFunction: public ShState
{
    BaseList<ShType> retTypes;
public:
    void addReturnType(ShType* obj)
            { retTypes.add(obj); }
};


// --- MODULE --- //

class ShModule: public ShScope
{
public:
    ShModule(const string& name);
};


// --- LANGUAGE TYPES ----------------------------------------------------- //


struct Range
{
    const large min;
    const large max;

    Range(large iMin, large iMax): min(iMin), max(iMax)  { }
    bool has(large v) const   { return v >= min && v <= max; }
    int  physicalSize() const;
};


class ShInteger: public ShType
{
public:
    const Range range;
    const int size;

    ShInteger(const string& name, large min, large max);
    bool isUnsigned() const
            { return range.min >= 0; }
};


class ShChar: public ShType
{
public:
    ShChar(const string& name)
            : ShType(name)  { }
};


class ShVector: public ShType
{
public:
    ShType* const elementType;
    ShVector(ShType* iElementType);
    ShVector(const string& name, ShType* iElementType);
};


// --- SYSTEM MODULE --- //

class ShQueenBee: public ShModule
{
public:
    ShInteger* const defaultInt;     // "int"
    ShInteger* const defaultLarge;   // "large"
    ShChar* const defaultChar;       // "char"
    ShVector* const defaultString;   // anonymous
    ShTypeAlias* const defaultStr;   // "str"
    
    ShQueenBee();
};


// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //


// --- BASIC LANGUAGE OBJECTS ---------------------------------------------- //

ShType::~ShType()
{
}


ShTypeAlias::ShTypeAlias(const string& name, ShType* iBase)
        : ShType(name), base(iBase)  { }


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
      defaultString(defaultChar->getVectorType()),
      defaultStr(new ShTypeAlias("str", defaultString))
{
    addType(defaultInt);
    addType(defaultLarge);
    addType(defaultChar);
    addType(defaultStr);
}




// ------------------------------------------------------------------------- //


class _AtExit
{
public:
    ~_AtExit()
    {
        if (Base::objCount != 0)
            fprintf(stderr, "Internal: objCount = %d\n", Base::objCount);
        if (stralloc != 0)
            fprintf(stderr, "Internal: stralloc = %d\n", stralloc);
        if (FifoChunk::chunkCount != 0)
            fprintf(stderr, "Internal: chunkCount = %d\n", FifoChunk::chunkCount);
    }
} _atexit;




int main()
{
    try
    {
        ShQueenBee system;
    }
    catch (Exception& e)
    {
        fprintf(stderr, "\n*** Exception: %s\n", e.what().c_str());
    }

    printf("sizeof(Base): %lu  sizeof(ShBase): %lu\n", sizeof(Base), sizeof(ShBase));
    return 0;
}

