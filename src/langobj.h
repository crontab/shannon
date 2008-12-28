#ifndef __LANGOBJ_H
#define __LANGOBJ_H

#include "str.h"
#include "except.h"
#include "baseobj.h"
#include "source.h"


// --- BASIC LANGUAGE OBJECTS ---------------------------------------------- //


class ShType;
class ShConstant;
class ShValue;
class ShOrdinal;
class ShInteger;
class ShVoid;
class ShRange;
class ShBool;
class ShScope;
class ShState;
class ShVector;
class ShSet;
class ShArray;
class ShModule;
class ShQueenBee;


class VmCode;

union VmQuant
{
    int op_;
    int int_;
    ptr ptr_;
#ifdef PTR64
    large large_;
#endif
};


typedef PodArray<VmQuant> VmCodeSegment;



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
    ShTypeId typeId;
    ShVector* derivedVectorType;
    ShSet* derivedSetType;

protected:
    virtual string getFullDefinition(const string& objName) const = 0;

public:
    ShType(ShTypeId iTypeId);
    ShType(const string& name, ShTypeId iTypeId);
    virtual ~ShType();

    string getDefinition(const string& objName) const;
    string getDefinition() const;
    string getDefinitionQ() const; // quoted
    virtual string displayValue(const ShValue&) const = 0;

    bool isVoid() const { return typeId == typeVoid; }
    bool isTypeRef() const { return typeId == typeTypeRef; }
    bool isRange() const { return typeId == typeRange; }
    bool isOrdinal() const { return typeId >= typeInt && typeId <= typeBool; }
    bool isInt() const { return typeId == typeInt; }
    bool isChar() const { return typeId == typeChar; }
    bool isEnum() const  { return typeId == typeEnum; }
    bool isBool() const  { return typeId == typeBool; }
    bool isVector() const { return typeId == typeVector; }
    bool isEmptyVec() const;
    bool isArray() const { return typeId == typeArray; }
    bool isPodPointer() const { return typeId == typeTypeRef; }

    virtual int staticSize() const = 0;
    int staticSizeRequired() const;
    int staticSizeAligned() const;
    virtual bool isPod() const
            { return true; }
    virtual bool isString() const
            { return false; }
    virtual bool isLargePod() const
            { return false; }
    virtual bool equals(ShType*) const = 0;
    virtual bool canBeArrayIndex() const
            { return false; }
    bool canBeArrayElement() const
            { return staticSize() > 0; }
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
    int dataOffset;

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
    ShScope(const string& name, ShTypeId iTypeId);
    ~ShScope();
    virtual bool isPod() const
            { return false; }
    virtual bool isScope() const
            { return true; }
    virtual string displayValue(const ShValue&) const
            { return "*undefined*"; }
    void addUses(ShModule*);
    void addAnonType(ShType*);
    void addType(ShType*);
    virtual void addVariable(ShVariable*) = 0;
    void addTypeAlias(ShTypeAlias*);
    void addConstant(ShConstant*);
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
    large min;
    large max;

    Range(large iMin, large iMax): min(iMin), max(iMax)  { }
    int physicalSize() const;
};


class ShOrdinal: public ShType
{
protected:
    ShRange* derivedRangeType;
    Range range;
    int size;

    void recalcSize()
            { size = range.physicalSize(); }
    virtual ShOrdinal* cloneWithRange(large min, large max) = 0;

public:
    ShOrdinal(ShTypeId iTypeId, large min, large max);
    ShOrdinal(const string& name, ShTypeId iTypeId, large min, large max);
    virtual int staticSize() const
            { return size; }
    virtual bool canBeArrayIndex() const
            { return true; }
    virtual bool canStaticCastTo(ShType* type) const
            { return type->isOrdinal(); }
    bool isLargeInt() const;
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
    ShInteger(large min, large max);
    ShInteger(const string& name, large min, large max);
    virtual string displayValue(const ShValue& v) const;
    bool isLargeInt() const
            { return size > 4; }
    virtual bool canAssign(ShType* type) const
            { return type->isInt() && (isLargeInt() == PInteger(type)->isLargeInt()); }
    virtual bool canCompareWith(ShType* type) const
            { return canAssign(type); }
    virtual bool isLargePod() const
            { return isLargeInt(); }
    virtual bool equals(ShType* type) const
            { return type->isInt() && rangeEquals(((ShInteger*)type)->range); }
};

inline bool ShOrdinal::isLargeInt() const
        { return isInt() && PInteger(this)->isLargeInt(); }


class ShChar: public ShOrdinal
{
protected:
    virtual string getFullDefinition(const string& objName) const;
    virtual ShOrdinal* cloneWithRange(large min, large max);
public:
    ShChar(int min, int max);
    ShChar(const string& name, int min, int max);
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
    ShBool(const string& name);
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
    ShVoid(const string& name);
    virtual int staticSize() const
            { return 0; }
    virtual string displayValue(const ShValue& v) const;
    virtual bool equals(ShType* type) const
            { return type->isVoid(); }
};


class ShTypeRef: public ShType
{
    virtual string getFullDefinition(const string& objName) const;
public:
    ShTypeRef(const string& name);
    virtual int staticSize() const
            { return sizeof(ptr); }
    virtual string displayValue(const ShValue& v) const;
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
    virtual int staticSize() const
            { return sizeof(large); }
    virtual string displayValue(const ShValue& v) const;
    virtual bool isLargePod() const
            { return true; }
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
    ShVector(const string& name, ShType* iElementType);
    virtual string displayValue(const ShValue& v) const;
    virtual int staticSize() const
            { return sizeof(ptr); }
    virtual bool isPod() const
            { return false; }
    virtual bool isString() const
            { return elementType->isChar() && ((ShChar*)elementType)->isFullRange(); }
    virtual bool canBeArrayIndex() const
            { return isString(); }
    virtual bool canCompareWith(ShType* type) const
            { return isString() && (type->isString() || type->isChar()); }
    virtual bool canCheckEq(ShType* type) const
            { return canCompareWith(type) || equals(type) || isEmptyVec() || type->isEmptyVec(); }
    virtual bool canAssign(ShType* type) const
            { return equals(type) || elementEquals(type) || type->isEmptyVec(); }
    virtual bool equals(ShType* type) const
            { return type->isVector() && elementEquals(((ShVector*)type)->elementType); }
    virtual bool canStaticCastTo(ShType* type) const
            { return isEmptyVec() || equals(type); }
    bool isPodVector() const
            { return elementType->isPod(); }
    bool elementEquals(ShType* elemType) const
            { return elementType->equals(elemType); }
    bool isEmptyVec() const
            { return elementType->isVoid(); }
};

inline bool ShType::isEmptyVec() const
        { return isVector() && PVector(this)->isEmptyVec(); }


class ShArray: public ShVector
{
protected:
    virtual string getFullDefinition(const string& objName) const;

public:
    ShType* const indexType;
    ShArray(ShType* iElementType, ShType* iIndexType);
    virtual string displayValue(const ShValue& v) const;
    virtual bool equals(ShType* type) const
            { return type->isArray() && elementEquals(((ShArray*)type)->elementType)
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


union podvalue
{
    ptr ptr_;
    int int_;
    large large_;
};


struct ShValue: noncopyable
{
    podvalue value;
    ShType* type;

    ShValue(): type(NULL)  { }
    ShValue(const ShValue&);
//    ShValue(ShType* iType)
//            : type(iType) { }
    ShValue(ShType* iType, int iValue)
            : type(iType) { value.int_ = iValue; }
/*
    ShValue(ShType* iType, large iValue)
            : type(iType) { value.large_ = iValue; }
    ShValue(ShType* iType, ptr iValue)
            : type(iType) { value.ptr_ = iValue; }
*/
    ~ShValue()
            { _finalize(); }

    void operator= (const ShValue&);

    void assignPtr(ShType* iType, ptr p);
    void assignInt(ShType* iType, int i);
    void assignLarge(ShType* iType, large l);
    void assignVec(ShType* iType, const string& s);
    void assignFromBuf(ShType*, const ptr);
    int  assignToBuf(ptr);

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
    ShConstant(const string& name, const ShValue& iValue);
    ShConstant(const string& name, ShEnum* type, int value);
};


// ------------------------------------------------------------------------ //


// --- MODULE --- //

class ShModule: public ShScope
{
    // --- Compiler ---

    string fileName;
    Parser parser;
    Array<string> vectorConsts;

    string registerString(const string& v) // TODO: fund duplicates
            { vectorConsts.add(v); return v; }
    string registerVector(const string& v)
            { vectorConsts.add(v); return v; }
    void addObject(ShBase*); // deletes the object in case of an exception

    ShScope* currentScope;

    void error(const string& msg)           { parser.error(msg); }
    void error(const char* msg)             { parser.error(msg); }
    void errorWithLoc(const string& msg)    { parser.errorWithLoc(msg); }
    void errorWithLoc(const char* msg)      { parser.errorWithLoc(msg); }
    void errorNotFound(const string& msg)   { parser.errorNotFound(msg); }
    void notImpl()                          { error("Feature not implemented"); }
    ShBase* getQualifiedName();
    ShType* getDerivators(ShType*);
    ShType* getType(bool require);
    ShType* getTypeOrNewIdent(string* strToken);
    void    getConstCompound(ShType*, ShValue&);
    ShType* parseAtom(VmCode&);
    ShType* parseDesignator(VmCode&);
    ShInteger* arithmResultType(ShInteger* left, ShInteger* right);
    ShType* parseFactor(VmCode&);
    ShType* parseTerm(VmCode&);
    ShType* parseArithmExpr(VmCode&);
    ShType* parseSimpleExpr(VmCode&);
    ShType* parseRelExpr(VmCode&);
    ShType* parseNotLevel(VmCode&);
    ShType* parseAndLevel(VmCode&);
    ShType* parseOrLevel(VmCode&);
    ShType* parseSubrange(VmCode&);
    ShType* parseBoolExpr(VmCode& code)
            { return parseOrLevel(code); }
    ShType* parseExpr(VmCode& code)
            { return parseSubrange(code); }
    void getConstExpr(ShType* typeHint, ShValue& result);

    ShEnum* parseEnumType();
    void parseTypeDef();
    void parseVarConstDef(bool isVar);
    void parseVarDef();

protected:
    virtual string getFullDefinition(const string& objName) const;

public:
    bool compiled;

    ShModule(const string& filename);
    ~ShModule();
    void compile();
    void dump(string indent) const;
    virtual int staticSize() const
            { return 0; }
    virtual bool equals(ShType* type) const
            { return false; }
    virtual void addVariable(ShVariable*);

    // --- RUNTIME --- //
protected:
    void setupRuntime(VmCode& main, VmCode& fin);
    
public:
    int dataSize;
    char* dataSegment;
    VmCodeSegment mainCode;
    VmCodeSegment finCode;
};


// --- SYSTEM MODULE --- //

class ShQueenBee: public ShModule
{
public:
    ShInteger* const defaultInt;     // "int"
    ShInteger* const defaultLarge;   // "large"
    ShChar* const defaultChar;       // "char"
    ShVector* const defaultStr;      // "str"
    ShBool* const defaultBool;       // "bool"
    ShVoid* const defaultVoid;       // "void"
    ShTypeRef* const defaultTypeRef; // "typeref"
    ShVector* const defaultEmptyVec;
    
    ShQueenBee();
};


// ------------------------------------------------------------------------ //


extern ShQueenBee* queenBee;

void initLangObjs();
void doneLangObjs();

ShModule* findModule(const string& name);
void registerModule(ShModule* module);


#endif
