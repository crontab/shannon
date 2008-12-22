#ifndef __LANGOBJ_H
#define __LANGOBJ_H

#include "str.h"
#include "except.h"
#include "baseobj.h"
#include "source.h"


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
const int   memAlign  = sizeof(ptr);


// --- BASIC LANGUAGE OBJECTS ---------------------------------------------- //


class ShType;
class ShBool;
class ShScope;
class ShState;
class ShVector;
class ShSet;
class ShArray;
class ShModule;
class ShQueenBee;


class ShBase: public BaseNamed
{
public:
    ShScope* owner;
    
    ShBase(): BaseNamed(), owner(NULL)  { }
    ShBase(const string& name): BaseNamed(name), owner(NULL)  { }
    
    virtual bool isType()       { return false; }
    virtual bool isTypeAlias()  { return false; }
    virtual bool isScope()      { return false; }
};


class ShType: public ShBase
{
    ShVector* derivedVectorType;
    ShSet* derivedSetType;

protected:
    virtual string getFullDefinition(const string& objName) const = 0;

public:
    ShType();
    ShType(const string& name);
    virtual ~ShType();
    string getDisplayName(const string& objName) const;
    virtual bool isType()
            { return true; }
    virtual bool isComplete() const
            { return true; }
    virtual bool isOrdinal() const = 0;
    virtual bool isComparable() const
            { return isOrdinal(); }
    bool canBeArrayIndex() const
            { return isOrdinal() || isComparable(); }
    virtual bool isChar() const
            { return false; }
    virtual bool isString() const
            { return false; }
    virtual bool isBool() const
            { return false; }
    ShVector* deriveVectorType(ShScope* scope);
    ShArray* deriveArrayType(ShType* indexType, ShScope* scope);
    ShSet* deriveSetType(ShBool* elementType, ShScope* scope);
    void setDerivedVectorTypePleaseThisIsCheatingIKnow(ShVector* v)
            { derivedVectorType = v; }
};


class ShTypeAlias: public ShBase
{
public:
    ShType* const base;
    ShTypeAlias(const string& name, ShType* iBase);
    virtual bool isTypeAlias()  { return true; }
};


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
    BaseTable<ShModule> uses; // not owned
    BaseList<ShType> types;
    BaseList<ShVariable> vars;
    BaseList<ShTypeAlias> typeAliases;
    
    ShBase* own(ShBase* obj);
    void addSymbol(ShBase* obj);

public:
    bool complete;

    ShScope(const string& name);
    virtual bool isScope()
            { return true; }
    virtual bool isComplete() const
            { return complete; };
    virtual bool isOrdinal() const
            { return false; }
    void addUses(ShModule*);
    void addAnonType(ShType*);
    void addType(ShType*);
    void addAnonVar(ShVariable*);
    void addTypeAlias(ShTypeAlias*);
    void addVariable(ShVariable*);
    void setCompleted()
            { complete = true; }
    ShBase* find(const string& name) const
            { return symbols.find(name); }
    ShBase* deepSearch(const string&) const;
    void dump(string indent) const;
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


class ShOrdinal: public ShType
{
public:
    ShOrdinal(const string& name): ShType(name)  { }
    virtual bool isOrdinal() const
            { return true; }
};


class ShInteger: public ShOrdinal
{
protected:
    virtual string getFullDefinition(const string& objName) const;

public:
    const Range range;
    const int size;

    ShInteger(const string& name, large min, large max);
    bool isUnsigned() const
            { return range.min >= 0; }
};


class ShChar: public ShOrdinal
{
protected:
    virtual string getFullDefinition(const string& objName) const;

public:
    ShChar(const string& name): ShOrdinal(name)  { }
    virtual bool isChar() const
            { return true; }
};


class ShBool: public ShOrdinal
{
protected:
    virtual string getFullDefinition(const string& objName) const;

public:
    ShBool(const string& name): ShOrdinal(name)  { }
    virtual bool isBool() const
            { return true; }
};


class ShVoid: public ShType
{
protected:
    virtual string getFullDefinition(const string& objName) const;

public:
    ShVoid(const string& name): ShType(name)  { }
    virtual bool isOrdinal() const
            { return false; }
};


class ShVector: public ShType
{
protected:
    virtual string getFullDefinition(const string& objName) const;

public:
    ShType* const elementType;
    ShVector(ShType* iElementType);
    ShVector(const string& name, ShType* iElementType);
    virtual bool isOrdinal() const
            { return false; }
    virtual bool isString() const
            { return elementType->isChar(); }
    virtual bool isComparable() const
            { return elementType->isChar(); }
};


class ShString: public ShVector
{
protected:
    // Note that any char[] is a string, too
    friend class ShQueenBee;
    ShString(const string& name, ShChar* elementType)
            : ShVector(name, elementType)  { }
public:
    virtual bool isString() const
            { return true; }
};


class ShArray: public ShVector
{
protected:
    virtual string getFullDefinition(const string& objName) const;

public:
    ShType* const indexType;
    ShArray(ShType* iElementType, ShType* iIndexType);
    virtual bool isOrdinal() const
            { return false; }
};


class ShSet: public ShArray
{
public:
    ShSet(ShBool* iElementType, ShType* iIndexType);
};


class ShState: public ShScope
{
protected:
    BaseList<ShArgument> args;
    BaseList<ShState> states;

    virtual string getFullDefinition(const string& objName) const;
    string getArgsDefinition() const;

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


// --- LITERAL VALUES ----------------------------------------------------- //


class ShValue: noncopyable
{
    union
    {
        ptr ptr_;
        int int_;
        large large_;
    } value;

public:
    ShType* const type;

    ShValue(ShOrdinal* iType, large iValue)
            : type(iType)  { value.large_ = iValue; }
    ShValue(ShString* iType, const string& iValue)
            : type(iType)  { value.ptr_ = ptr(iValue.c_bytes()); }
};



// ------------------------------------------------------------------------ //


// --- MODULE --- //

class ShModule: public ShScope
{
    Array<string> stringLiterals;
    string fileName;
    Parser parser;

    string registerString(const string& v);  // TODO: find duplicates (?)

    // --- Compiler ---
    ShScope* currentScope;
    void error(const string& msg)        { parser.error(msg); }
    void notImpl()                       { error("Feature not implemented"); }
    ShBase* getQualifiedName();
    ShType* getDerivators(ShType*);
    ShType* getType(ShBase* previewObj);
    ShBase* getAtom();
    void parseTypeDef();
    void parseObjectDef(ShBase* previewObj);

protected:
    virtual string getFullDefinition(const string& objName) const;

public:
    bool compiled;

    ShModule(const string& filename);
    void compile();
    void dump(string indent) const;
};


// --- SYSTEM MODULE --- //

class ShQueenBee: public ShModule
{
public:
    ShInteger* const defaultInt;     // "int"
    ShInteger* const defaultLarge;   // "large"
    ShChar* const defaultChar;       // "char"
    ShString* const defaultStr;      // "str"
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
