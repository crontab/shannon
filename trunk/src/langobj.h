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
class ShConstant;
class ShValue;
class ShOrdinal;
class ShVoid;
class ShRange;
class ShBool;
class ShScope;
class ShState;
class ShVector;
class ShString;
class ShSet;
class ShArray;
class ShModule;
class ShQueenBee;

class VmCode;

enum ShBaseId
{
    baseType, baseTypeAlias, baseVariable, baseConstant
};


class ShBase: public BaseNamed
{
    ShBaseId baseId;

public:
    ShScope* owner;
    
    ShBase(ShBaseId iBaseId);
    ShBase(const string& name, ShBaseId iBaseId);
    
    bool isType() const       { return baseId == baseType; }
    bool isTypeAlias() const  { return baseId == baseTypeAlias; }
    bool isVariable() const   { return baseId == baseVariable; }
    bool isConstant() const   { return baseId == baseConstant; }
    virtual bool isScope() const      { return false; }
};


enum ShTypeId
{
    typeVoid,
    typeInt, typeChar, typeEnum, typeBool,
    typeVector, typeArray, typeTypeRef, typeRange,
    typeModule
};


class ShType: public ShBase
{
    int typeId;
    ShVector* derivedVectorType;
    ShSet* derivedSetType;

protected:
    virtual string getFullDefinition(const string& objName) const = 0;

public:
    ShType(ShTypeId iTypeId);
    ShType(const string& name, ShTypeId iTypeId);
    virtual ~ShType();
    string getDisplayName(const string& objName) const;
    virtual string displayValue(const ShValue&) const = 0;
    virtual bool isComplete() const
            { return true; }
    virtual bool canBeArrayIndex() const
            { return false; }
    virtual bool isString() const
            { return false; }
    virtual bool isLarge() const
            { return false; }
    virtual bool isPointer() const = 0;

    bool isVoid() const { return typeId == typeVoid; }
    bool isTypeRef() const { return typeId == typeTypeRef; }
    bool isRange() const { return typeId == typeRange; }
    bool isOrdinal() const { return typeId >= typeInt && typeId <= typeBool; }
    bool isInt() const { return typeId == typeInt; }
    bool isChar() const { return typeId == typeChar; }
    bool isBool() const  { return typeId == typeBool; }
    bool isVector() const { return typeId == typeVector; }
    bool isArray() const { return typeId == typeArray; }

    virtual bool equals(ShType*) const = 0;
    // isCompatibleWith() is for binary operators, requires strict equality
    // of types unless redefined in descendant classes. Actually redefined
    // in ordinals.
    virtual bool isCompatibleWith(ShType* type) const
            { return equals(type); }
    // Assignments require strict equality of types except for ordinals, 
    // and also for a special case str = char
    virtual bool canAssign(ShType* type) const
            { return equals(type); }
    ShVector* deriveVectorType(ShScope* scope);
    ShArray* deriveArrayType(ShType* indexType, ShScope* scope);
    ShSet* deriveSetType(ShVoid* elementType, ShScope* scope);
    void setDerivedVectorTypePleaseThisIsCheatingIKnow(ShVector* v)
            { derivedVectorType = v; }
};


class ShTypeAlias: public ShBase
{
public:
    ShType* const base;
    ShTypeAlias(const string& name, ShType* iBase);
};


class ShVariable: public ShBase
{
public:
    ShType* const type;
    int scopeIndex;

    ShVariable(ShType* iType);
    ShVariable(const string& name, ShType* iType);
    virtual bool isArgument()  { return false; }
};


class ShArgument: public ShVariable
{
public:
    ShArgument(const string& name, ShType* iType);
    virtual bool isArgument()  { return true; }
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
    BaseList<ShConstant> consts;
    
    ShBase* own(ShBase* obj);
    void addSymbol(ShBase* obj);

public:
    bool complete;

    ShScope(const string& name, ShTypeId iTypeId);
    virtual bool isScope() const
            { return true; }
    virtual string displayValue(const ShValue&) const
            { return "*undefined*"; }
    virtual bool isComplete() const
            { return complete; };
    virtual bool isPointer() const
            { return true; }
    void addUses(ShModule*);
    void addAnonType(ShType*);
    void addType(ShType*);
    void addAnonVar(ShVariable*);
    void addVariable(ShVariable*);
    void addTypeAlias(ShTypeAlias*);
    void addConstant(ShConstant*);
    void setCompleted()
            { complete = true; }
    ShBase* find(const string& name) const
            { return symbols.find(name); }
    ShBase* deepFind(const string&) const;
    void dump(string indent) const;
};


// --- LANGUAGE TYPES ----------------------------------------------------- //

class EInvalidSubrange: public EMessage
{
public:
    EInvalidSubrange(ShOrdinal*);
};


struct Range
{
    const large min;
    const large max;

    Range(large iMin, large iMax): min(iMin), max(iMax)  { }
    int physicalSize() const;
};


class ShOrdinal: public ShType
{
    ShRange* derivedRangeType;
    
    virtual ShOrdinal* cloneWithRange(large min, large max) = 0;

public:
    const Range range;
    const int size;

    ShOrdinal(ShTypeId iTypeId, large min, large max);
    ShOrdinal(const string& name, ShTypeId iTypeId, large min, large max);
    virtual bool isPointer() const
            { return false; }
    virtual bool isCompatibleWith(ShType*) const = 0;
    virtual bool canAssign(ShType* type) const
            { return isCompatibleWith(type); }
    virtual bool canBeArrayIndex() const
            { return true; }
    virtual bool isLarge() const
            { return size > 4; }
    bool contains(large value) const
            { return value >= range.min && value <= range.max; }
    bool rangeEquals(const Range& r) const
            { return range.min == r.min && range.max == r.max; }
    bool rangeEquals(large n, large x) const
            { return range.min == n && range.max == x; }
    bool rangeIsGreaterOrEqual(large n, large x)
            { return range.min <= n && range.max >= x; }
    ShRange* deriveRangeType(ShScope* scope);
    ShOrdinal* deriveOrdinalFromRange(ShValue value, ShScope* scope);
};


class ShInteger: public ShOrdinal
{
protected:
    virtual string getFullDefinition(const string& objName) const;
    virtual ShOrdinal* cloneWithRange(large min, large max);
public:
    ShInteger(large min, large max);
    ShInteger(const string& name, large min, large max);
    virtual string displayValue(const ShValue& v) const;
    virtual bool isCompatibleWith(ShType* type) const
            { return type->isInt(); }
    bool isUnsigned() const
            { return range.min >= 0; }
    virtual bool equals(ShType* type) const
            { return type->isInt() && rangeEquals(((ShInteger*)type)->range); }
};


class ShChar: public ShOrdinal
{
protected:
    virtual string getFullDefinition(const string& objName) const;
    virtual ShOrdinal* cloneWithRange(large min, large max);
public:
    ShChar(int min, int max);
    ShChar(const string& name, int min, int max);
    virtual string displayValue(const ShValue& v) const;
    virtual bool isCompatibleWith(ShType* type) const
            { return type->isChar() || type->isString(); }
    virtual bool canAssign(ShType* type) const
            { return type->isChar(); }
    virtual bool equals(ShType* type) const
            { return type->isChar() && rangeEquals(((ShChar*)type)->range); }
    bool isFullRange() const
            { return range.min == 0 && range.max == 255; }
};


class ShBool: public ShOrdinal
{
protected:
    virtual string getFullDefinition(const string& objName) const;
    virtual ShOrdinal* cloneWithRange(large min, large max);
public:
    ShBool(const string& name);
    virtual string displayValue(const ShValue& v) const;
    virtual bool isCompatibleWith(ShType* type) const
            { return type->isBool(); }
    virtual bool equals(ShType* type) const
            { return type->isBool(); }
};


class ShVoid: public ShType
{
protected:
    virtual string getFullDefinition(const string& objName) const;

public:
    ShVoid(const string& name);
    virtual bool isPointer() const
            { return false; }
    virtual string displayValue(const ShValue& v) const;
    virtual bool equals(ShType* type) const
            { return type->isVoid(); }
    virtual bool canAssign(const ShValue& value) const
            { return false; }
};


class ShTypeRef: public ShType
{
    virtual string getFullDefinition(const string& objName) const;
public:
    ShTypeRef(const string& name);
    virtual string displayValue(const ShValue& v) const;
    virtual bool isPointer() const
            { return false; }
    virtual bool equals(ShType* type) const
            { return type->isTypeRef(); }
};


class ShRange: public ShType
{
    // Not to be confused with subrange, which is ordinal. A value of type
    // range is a pair of ordinals, not bigger than 32-bit each.
    virtual string getFullDefinition(const string& objName) const;
public:
    ShOrdinal* base;
    ShRange(ShOrdinal* iBase);
    ShRange(const string& name, ShOrdinal* iBase);
    virtual string displayValue(const ShValue& v) const;
    virtual bool isPointer() const
            { return false; }
    virtual bool isLarge() const
            { return true; }
    virtual bool equals(ShType* type) const
            { return type->isRange() && base->equals(((ShRange*)type)->base); }
};


class ShVector: public ShType
{
protected:
    virtual string getFullDefinition(const string& objName) const;

public:
    ShType* const elementType;
    ShVector(ShType* iElementType);
    ShVector(const string& name, ShType* iElementType);
    virtual string displayValue(const ShValue& v) const;
    virtual bool isString() const
            { return elementType->isChar() && ((ShChar*)elementType)->isFullRange(); }
    virtual bool canBeArrayIndex() const
            { return isString(); }
    virtual bool isCompatibleWith(ShType* type) const
            { return equals(type) || (isString() && type->isChar()); }
    virtual bool canAssign(ShType* type) const
            { return type->isCompatibleWith(type); }
    virtual bool isPointer() const
            { return true; }
    virtual bool equals(ShType* type) const
            { return type->isVector() && elementType->equals(((ShVector*)type)->elementType); }
};


class ShString: public ShVector
{
public:
    // Note that any char[] is a string, too. This class is used only once
    // for defining the built-in "str".
    ShString(const string& name, ShChar* elementType);
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
    virtual string displayValue(const ShValue& v) const;
    virtual bool equals(ShType* type) const
            { return type->isArray() && elementType->equals(((ShArray*)type)->elementType)
                && indexType->equals(((ShArray*)type)->indexType); }
};


class ShSet: public ShArray
{
public:
    ShSet(ShVoid* iElementType, ShType* iIndexType);
    virtual string displayValue(const ShValue& v) const;
};


/*
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
*/


/*
class ShFunction: public ShState
{
    BaseList<ShType> retTypes;
public:
    void addReturnType(ShType* obj)
            { retTypes.add(obj); }
};
*/


// --- LITERAL VALUES ----------------------------------------------------- //

struct ShValue
{
    union
    {
        ptr ptr_;
        int int_;
        large large_;
    } value;

    ShType* type;

    ShValue(): type(NULL)  { }
    ShValue(const ShValue& v)
            : type(v.type) { value = v.value; }
    ShValue(ShType* iType)
            : type(iType)  { }
    ShValue(ShType* iType, large iValue)
            : type(iType)  { value.large_ = iValue; }
    ShValue(ShType* iType, ptr iValue)
            : type(iType)  { value.ptr_ = iValue; }
    ShValue(ShString* iType, const string& iValue)
            : type(iType)  { value.ptr_ = ptr(iValue.c_bytes()); }
    void operator= (const ShValue& v)
            { value = v.value; type = v.type; }
    int rangeMin() const
            { return int(value.large_); }
    int rangeMax() const
            { return int(value.large_ >> 32); }
};


class ShConstant: public ShBase
{
public:
    const ShValue value;
    ShConstant(const string& name, const ShValue& iValue);
};


// ------------------------------------------------------------------------ //


// --- MODULE --- //

class ShModule: public ShScope
{
    Array<string> stringLiterals;
    string fileName;
    Parser parser;

    string registerString(const string& v);  // TODO: find duplicates (?)
    void addObject(ShBase*); // deletes the object in case of an exception

    // --- Compiler ---
    ShScope* currentScope;
    void error(const string& msg)           { parser.error(msg); }
    void error(const char* msg)             { parser.error(msg); }
    void errorWithLoc(const string& msg)    { parser.errorWithLoc(msg); }
    void errorWithLoc(const char* msg)      { parser.errorWithLoc(msg); }
    void errorNotFound(const string& msg)   { parser.errorNotFound(msg); }
    void notImpl()                          { error("Feature not implemented"); }
    ShBase* getQualifiedName();
    ShType* getDerivators(ShType*);
    ShType* getType();
    ShType* getTypeOrNewIdent(string* strToken);
    void parseAtom(VmCode&);
    void parseDesignator(VmCode&);
    void parseFactor(VmCode&);
    void parseTerm(VmCode&);
    void parseSimpleExpr(VmCode&);
    void parseSubrange(VmCode&);
    void parseExpr(VmCode&);
    ShValue getConstExpr(ShType* typeHint);

    void parseTypeDef();
    void parseVarConstDef(bool isVar);
    void parseVarDef();

protected:
    virtual string getFullDefinition(const string& objName) const;

public:
    bool compiled;

    ShModule(const string& filename);
    void compile();
    void dump(string indent) const;
    virtual bool equals(ShType* type) const
            { return false; }
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
    ShTypeRef* const defaultTypeRef; // "typeref"
    
    ShQueenBee();
};


// ------------------------------------------------------------------------ //


extern ShQueenBee* queenBee;

void initLangObjs();
void doneLangObjs();

ShModule* findModule(const string& name);
void registerModule(ShModule* module);


#endif
