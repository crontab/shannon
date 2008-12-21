#ifndef __LANGOBJ_H
#define __LANGOBJ_H

#include "str.h"
#include "except.h"
#include "baseobj.h"
#include "source.h"


union ShQuant
{
    ptr ptr_;
    int int_;
};


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
const int   memAlign  = sizeof(ShQuant);


// --- BASIC LANGUAGE OBJECTS ---------------------------------------------- //


class ShType;
class ShScope;
class ShState;
class ShVector;


class ShBase: public BaseNamed
{
public:
    ShScope* const owner;
    
    ShBase(): BaseNamed(), owner(NULL)  { }
    ShBase(const string& name): BaseNamed(name), owner(NULL)  { }
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


class ShTypeAlias: public ShBase
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
    BaseList<ShTypeAlias> typeAliases;
    
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
    void addAnonVar(ShVariable* obj)
            { vars.add(obj); }
    void addVar(ShVariable* obj) throw(EDuplicate);
    void addTypeAlias(ShTypeAlias* obj) throw(EDuplicate);
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
    ShChar(const string& name): ShType(name)  { }
};


class ShBool: public ShType
{
public:
    ShBool(const string& name): ShType(name)  { }
};


class ShVoid: public ShType
{
public:
    ShVoid(const string& name): ShType(name)  { }
};


class ShVector: public ShType
{
public:
    ShType* const elementType;
    ShVector(ShType* iElementType);
    ShVector(const string& name, ShType* iElementType);
};


// --- LITERAL VALUES ----------------------------------------------------- //


class ShStringValue: public BaseNamed
{
public:
    ShStringValue(const string&);
    const string& getValue() const  { return name; }
};


// ------------------------------------------------------------------------ //


// --- MODULE --- //

class ShModule: public ShScope
{
    string fileName;
    Parser parser;

    string registerString(const string& v)  // TODO: find duplicates
            { return v; }

public:
    bool compiled;

    ShModule(const string& filename);
    void compile();
};


// --- SYSTEM MODULE --- //

class ShQueenBee: public ShModule
{
public:
    ShInteger* const defaultInt;     // "int"
    ShInteger* const defaultLarge;   // "large"
    ShChar* const defaultChar;       // "char"
    ShVector* const defaultString;   // <anonymous>
    ShTypeAlias* const defaultStr;   // "str"
    ShBool* const defaultBool;       // "bool"
    ShVoid* const defaultVoid;       // "void"
    
    ShQueenBee();
};


// ------------------------------------------------------------------------ //


extern ShQueenBee* queenBee;

void initLangObjs();
void doneLangObjs();

ShModule* findModule(const string& name);
void registerModule(ShModule* module);


#endif
