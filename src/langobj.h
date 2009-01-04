#ifndef __LANGOBJ_H
#define __LANGOBJ_H

#include "common.h"
#include "baseobj.h"
#include "source.h"
#include "vm.h"


class ShType;
class ShConstant;
class ShVariable;
class ShValue;
class ShOrdinal;
class ShInteger;
class ShVoid;
class ShRange;
class ShVector;
class ShSet;
class ShArray;
class ShReference;
class ShSymScope;
class ShScope;
class ShModule;

class VmCodeGen;


// --- BASIC LANGUAGE OBJECTS ---------------------------------------------- //


enum ShBaseId
{
    baseType, baseVariable, baseConstant
};


class ShBase: public BaseNamed
{
    ShBaseId baseId;

public:
    ShBase(ShBaseId iBaseId);
    ShBase(const string&, ShBaseId iBaseId);
    
    bool isType() const       { return baseId == baseType; }
    bool isVariable() const   { return baseId == baseVariable; }
    bool isConstant() const   { return baseId == baseConstant; }
};


enum StorageModel
{
    // Order is important, it's in sync with VM ops, also the order of first 3
    // is used in some code generation routines. If you change this, also take
    // a look at ShType::setTypeId(), ShType::isPod() and in case you are
    // adding a non-POD type, all places where stoVec is mentioned.
    stoByte, stoInt, stoLarge, stoPtr, stoVec, stoVoid,
    _stoMax
};


enum ShTypeId
{
    typeInt8, typeInt32, typeInt64, typeChar, typeEnum, typeBool,
    typeVector, typeArray, typeTypeRef, typeRange,
    typeReference,
    typeSymScope, typeLocalScope, typeModule,
    typeVoid,
    _typeMax
};


class ShType: public ShBase
{
    ShTypeId typeId;

protected:
    ShScope* owner;
    ShVector* derivedVectorType;
    ShSet* derivedSetType;
    ShReference* derivedRefType;

    virtual string getFullDefinition(const string& objName) const = 0;

public:
    const StorageModel storageModel;
    const offs staticSize;
    const offs staticSizeAligned;

    ShType(ShTypeId);
    ShType(const string&, ShTypeId);
    virtual ~ShType();
    
    void setOwner(ShScope*);
    ShTypeId getTypeId() const { return typeId; }
    void setTypeId(ShTypeId);

    string getDefinition(const string& objName) const;
    string getDefinition() const;
    string getDefinitionQ() const; // quoted
    virtual string displayValue(const ShValue&) const = 0;

    bool isVoid() const { return typeId == typeVoid; }
    bool isTypeRef() const { return typeId == typeTypeRef; }
    bool isRange() const { return typeId == typeRange; }
    bool isOrdinal() const { return typeId >= typeInt8 && typeId <= typeBool; }
    bool isInt() const { return typeId >= typeInt8 && typeId <= typeInt64; }
    bool isLargeInt() const { return typeId == typeInt64; }
    bool isChar() const { return typeId == typeChar; }
    bool isEnum() const  { return typeId == typeEnum; }
    bool isBool() const  { return typeId == typeBool; }
    bool isVector() const { return typeId == typeVector; }
    bool isEmptyVec() const;
    bool isArray() const { return typeId == typeArray; }
    bool isReference() const { return typeId == typeReference; }
    bool isLocalScope() const { return typeId == typeLocalScope; }
    bool isModule() const { return typeId == typeModule; }

    bool isPod() const
            { return storageModel != stoVec; }
    bool isString() const;
    bool canBeArrayIndex()
            { return canCompareWith(this); }
    virtual bool equals(ShType*) const = 0;
    bool canBeArrayElement() const
            { return staticSize > 0; }
    virtual bool canCompareWith(ShType* type) const
            { return false; }
    virtual bool canCheckEq(ShType* type) const
            { return canCompareWith(type); }
    virtual bool canAssign(ShType* type) const
            { return equals(type); }
    virtual bool canStaticCastTo(ShType* type) const
            { return equals(type); }

    ShVector* deriveVectorType();
    ShArray* deriveArrayType(ShType* indexType);
    ShSet* deriveSetType(ShVoid* elementType);
    ShReference* deriveRefType();
    void setDerivedVectorTypePleaseThisIsCheatingIKnow(ShVector* v)
            { derivedVectorType = v; }
};

typedef ShType* PType;


// --- SYMBOLS-ONLY SCOPE --- //

class ShSymScope: public ShType
{
protected:
    BaseTable<ShModule> uses; // not owned
    BaseTable<ShBase> symbols;

    virtual string getFullDefinition(const string& objName) const
            { return "*undefined*"; }

public:
    ShSymScope* const parent;

    ShSymScope(const string&, ShTypeId, ShSymScope* iParent);

    virtual string displayValue(const ShValue&) const
            { return "*undefined*"; }
    virtual bool equals(ShType* type) const
            { return false; }

    void addUses(ShModule*);
    void addSymbol(ShBase*);
    void finalizeVars(VmCodeGen*);
    ShBase* find(const string& ident) const
            { return symbols.find(ident); }
    ShBase* deepFind(const string&) const;
};


// --- SCOPE --- //

class ShScope: public ShSymScope
{
protected:
    BaseList<ShType> types;
    BaseList<ShVariable> vars;
    BaseList<ShConstant> consts;
    
public:
    ShScope(const string&, ShTypeId, ShSymScope* iParent);
    ~ShScope();
    void addAnonType(ShType*);
    void addTypeAlias(const string&, ShType*, ShSymScope*);
    void addConstant(ShConstant*, ShSymScope*);
    virtual ShVariable* addVariable(const string&, ShType*, ShSymScope*, VmCodeGen*) = 0;
    void dump(string indent) const;
};


class ShVariable: public ShBase
{
public:
    ShType* const type;
    ShScope* const ownerScope;
    offs const dataOffset;

    ShVariable(const string&, ShType*, ShScope*, offs);
    bool isLocal() const { return ownerScope->isLocalScope(); }
};


// --- LANGUAGE TYPES ----------------------------------------------------- //

struct EInvalidSubrange: public Exception
{
    EInvalidSubrange(ShOrdinal*);
};


struct Range
{
    large min;
    large max;

    Range(large iMin, large iMax): min(iMin), max(iMax)  { }
    int physicalSize() const;
};


class ShOrdinal: public ShType
{
protected:
    ShRange* derivedRangeType;
    Range const range;

    virtual ShOrdinal* cloneWithRange(large min, large max) = 0;
    void reassignMax(int max)
        { ((Range&)range).max = max; }

public:
    ShOrdinal(ShTypeId, large min, large max);
    ShOrdinal(const string&, ShTypeId, large min, large max);
    virtual bool canStaticCastTo(ShType* type) const
            { return type->isOrdinal(); }
    bool contains(large v) const
            { return v >= range.min && v <= range.max; }
    bool contains(const ShValue&) const;
    bool rangeEquals(const Range& r) const
            { return range.min == r.min && range.max == r.max; }
    bool rangeEquals(large n, large x) const
            { return range.min == n && range.max == x; }
    bool rangeIsGreaterOrEqual(ShOrdinal* obj)
            { return range.min <= obj->range.min && range.max >= obj->range.max; }
    bool rangeIsGreaterOrEqual(large n, large x)
            { return range.min <= n && range.max >= x; }
    ShRange* deriveRangeType();
    ShOrdinal* deriveOrdinalFromRange(const ShValue& value);
};

typedef ShOrdinal* POrdinal;
typedef ShInteger* PInteger;

class ShInteger: public ShOrdinal
{
protected:
    virtual string getFullDefinition(const string& objName) const;
    virtual ShOrdinal* cloneWithRange(large min, large max);
public:
    ShInteger(const string&, large min, large max);
    virtual string displayValue(const ShValue& v) const;
    virtual bool canAssign(ShType* type) const
            { return type->isInt() && (isLargeInt() == type->isLargeInt()); }
    virtual bool canCompareWith(ShType* type) const
            { return canAssign(type); }
    virtual bool equals(ShType* type) const
            { return type->isInt() && rangeEquals(((ShInteger*)type)->range); }
};

class ShChar: public ShOrdinal
{
protected:
    virtual string getFullDefinition(const string& objName) const;
    virtual ShOrdinal* cloneWithRange(large min, large max);
public:
    ShChar(const string&, int min, int max);
    virtual string displayValue(const ShValue& v) const;
    virtual bool canAssign(ShType* type) const
            { return type->isChar(); }
    virtual bool canCompareWith(ShType* type) const
            { return type->isChar() || type->isString(); }
    virtual bool equals(ShType* type) const
            { return type->isChar() && rangeEquals(((ShChar*)type)->range); }
    bool isFullRange() const
            { return range.min == 0 && range.max == 255; }
};


class ShEnum: public ShOrdinal
{
protected:
    BaseTable<ShConstant> values;

    ShEnum(const BaseTable<ShConstant>& t, int min, int max);
    virtual string getFullDefinition(const string& objName) const;
    virtual ShOrdinal* cloneWithRange(large min, large max);

public:
    ShEnum();
    virtual bool equals(ShType* type) const
            { return type == this; }
    virtual bool canAssign(ShType* type) const
            { return type->isEnum() && values._isClone(((ShEnum*)type)->values); }
    virtual bool canCompareWith(ShType* type) const
            { return canAssign(type); }
    virtual string displayValue(const ShValue& v) const;
    void registerConst(ShConstant* obj)
            { values.add(obj); }
    int nextValue()
            { return values.size(); }
    void finish();
};


class ShBool: public ShOrdinal
{
protected:
    virtual string getFullDefinition(const string& objName) const;
    virtual ShOrdinal* cloneWithRange(large min, large max);
public:
    ShBool(const string&);
    virtual string displayValue(const ShValue& v) const;
    virtual bool canAssign(ShType* type) const
            { return type->isBool(); }
    virtual bool canCompareWith(ShType* type) const
            { return type->isBool(); }
    virtual bool equals(ShType* type) const
            { return type->isBool(); }
};


class ShVoid: public ShType
{
protected:
    virtual string getFullDefinition(const string& objName) const;

public:
    ShVoid(const string&);
    virtual string displayValue(const ShValue& v) const;
    virtual bool equals(ShType* type) const
            { return type->isVoid(); }
};


class ShTypeRef: public ShType
{
    virtual string getFullDefinition(const string& objName) const;
public:
    ShTypeRef(const string&);
    virtual string displayValue(const ShValue& v) const;
    virtual bool canCheckEq(ShType* type) const
            { return type->isTypeRef(); }
    virtual bool equals(ShType* type) const
            { return type->isTypeRef(); }
};


typedef ShRange* PRange;

class ShRange: public ShType
{
    // Not to be confused with subrange, which is ordinal. A value of type
    // range is a pair of ordinals, not bigger than 32-bit each.
    virtual string getFullDefinition(const string& objName) const;
public:
    ShOrdinal* base;
    ShRange(ShOrdinal* iBase);
    ShRange(const string&, ShOrdinal* iBase);
    virtual string displayValue(const ShValue& v) const;
    virtual bool canCheckEq(ShType* type) const
            { return equals(type); }
    virtual bool equals(ShType* type) const
            { return type->isRange() && base->equals(((ShRange*)type)->base); }
};


typedef ShVector* PVector;

class ShVector: public ShType
{
protected:
    virtual string getFullDefinition(const string& objName) const;

public:
    ShType* const elementType;

    ShVector(ShType* iElementType);
    ShVector(const string&, ShType* iElementType);

    virtual string displayValue(const ShValue& v) const;
    virtual bool canCompareWith(ShType* type) const
            { return isString() && (type->isString() || type->isChar()); }
    virtual bool canCheckEq(ShType* type) const
            { return canCompareWith(type)
                || (isPodVector() && equals(type))
                || isEmptyVec() || type->isEmptyVec(); }
    virtual bool canAssign(ShType* type) const
            { return equals(type) || elementEquals(type) || type->isEmptyVec(); }
    virtual bool equals(ShType* type) const
            { return type->isVector() && elementEquals(((ShVector*)type)->elementType); }
    virtual bool canStaticCastTo(ShType* type) const
            { return isEmptyVec() || equals(type); }

    bool elementEquals(ShType* elemType) const
            { return elementType->equals(elemType); }
    bool isEmptyVec() const
            { return elementType->isVoid(); }

    bool isPodVector() const
            { return elementType->isPod(); }
};

inline bool ShType::isEmptyVec() const
        { return isVector() && PVector(this)->isEmptyVec(); }


class ShArray: public ShType
{
protected:
    virtual string getFullDefinition(const string& objName) const;

public:
    ShType* const elementType;
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


typedef ShReference* PReference;

class ShReference: public ShType
{
protected:
    virtual string getFullDefinition(const string& objName) const;

public:
    ShType* const base;
    ShReference(ShType* iBase);

    virtual bool equals(ShType* type) const
            { return type->isReference() && base->equals(PReference(type)->base); }
    virtual string displayValue(const ShValue& v) const;
};


// --- LITERAL VALUES ----------------------------------------------------- //


union podvalue
{
    ptr ptr_;
    int int_;
    large large_;
};


struct ShValue: noncopyable
{
    ShType* type;
    podvalue value;

    ShValue(): type(NULL)  { }
    ShValue(ShType*, const podvalue&);
    ShValue(ShType* iType, int iValue): type(iType) { value.int_ = iValue; }
    ShValue(ShType* iType, ptr iValue): type(iType) { value.ptr_ = iValue; }
    ~ShValue()  { _finalize(); }

    void assignInt(ShType* iType, int i);
    void assignLarge(ShType* iType, large l);
    void assignPtr(ShType* iType, ptr p);
    void assignVec(ShType* iType, const string& s);
    void assignVoid(ShType* iType);
    void assignFromBuf(ShType*, ptr);
    void assignToBuf(ptr);
    void assignValue(ShType* type, const podvalue& v)
        { assignFromBuf(type, ptr(&v)); }
    void clear()
        { _finalize(); type = NULL; }

    int rangeMin() const
            { return int(value.large_); }
    int rangeMax() const
            { return int(value.large_ >> 32); }

protected:
    void _finalize();
};


class ShConstant: public ShBase
{
public:
    const ShValue value;
    ShConstant(const string&, const ShValue&);
    ShConstant(const string&, ShEnum* type, int value);
    ShConstant(const string&, ShTypeRef*, ShType*);
};


// ------------------------------------------------------------------------ //


class ShLocalScope: public ShScope
{
    // Usually belongs to a function/state, plus modules can create local
    // scopes for nested blocks to avoid pollution of the static space.
protected:
    virtual string getFullDefinition(const string& objName) const;
public:
    ShLocalScope(const string& iName, ShSymScope* iParent);
    virtual ShVariable* addVariable(const string&, ShType*, ShSymScope*, VmCodeGen*);
};


// --- STATE --- //

class ShStateBase: public ShScope
{
    // basis for modules, functions and states
public:
    ShLocalScope localScope;
    ShStateBase(const string&, ShTypeId, ShSymScope* iParent);
};


// --- MODULE --- //

class ShModule: public ShStateBase
{
protected:
    Array<string> vectorConsts;

    virtual string getFullDefinition(const string& objName) const;
    virtual ShVariable* addVariable(const string&, ShType* type,
                ShSymScope*, VmCodeGen*);

public:
    offs dataSize;
    VmCodeSegment codeseg;
    
    ShModule(const string&);
    ~ShModule();

    string registerString(const string& v) // TODO: find duplicates
            { vectorConsts.add(v); return v; }
    void dump(string indent) const;

    void execute();
};


// --- SYSTEM MODULE --- //

class ShQueenBee: public ShModule
{
public:
    ShTypeRef* const defaultTypeRef; // "typeref"
    ShInteger* const defaultInt;     // "int"
    ShInteger* const defaultLarge;   // "large"
    ShChar* const defaultChar;       // "char"
    ShVector* const defaultStr;      // "str"
    ShBool* const defaultBool;       // "bool"
    ShVoid* const defaultVoid;       // "void"
    ShVector* const defaultEmptyVec;
    
    ShQueenBee();
    void setup();
};


// ------------------------------------------------------------------------ //


extern ShQueenBee* queenBee;

void initLangObjs();
void doneLangObjs();

ShModule* findModule(const string&);
void registerModule(ShModule*);


#endif

