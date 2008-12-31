#ifndef __LANGOBJ_H
#define __LANGOBJ_H

#include "str.h"
#include "except.h"
#include "baseobj.h"
#include "source.h"
#include "vm.h"


class ShType;
class ShConstant;
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

// offs memAlign(int offs);


// --- BASIC LANGUAGE OBJECTS ---------------------------------------------- //


enum ShBaseId
{
    baseType, baseTypeAlias, baseVariable, baseConstant
};


class ShBase: public BaseNamed
{
    ShBaseId baseId;

public:
    ShBase(ShBaseId iBaseId);
    ShBase(const string& name, ShBaseId iBaseId);
    
    bool isType() const       { return baseId == baseType; }
    bool isTypeAlias() const  { return baseId == baseTypeAlias; }
    bool isVariable() const   { return baseId == baseVariable; }
    bool isConstant() const   { return baseId == baseConstant; }
    virtual bool isScope() const      { return false; }
};


enum StorageModel
{
    // Order is important, it's in sync with VM ops, also the order of first 3
    // is used in some code generation routines. If you change this, also take
    // a look at ShType::staticSize()
    stoByte, stoInt, stoLarge, stoPtr, stoVec, stoVoid,
    _stoMax
};


enum ShTypeId
{
    typeInt8, typeInt32, typeInt64, typeChar, typeEnum, typeBool,
    typeVector, typeArray, typeTypeRef, typeRange,
    typeReference,
    typeLocalSymScope, typeLocalScope, typeModule,
    typeVoid,
    _typeMax
};


class ShType: public ShBase
{
    ShTypeId typeId;
    StorageModel stoModel;
    offs size;
    offs alignedSize;

protected:

    ShVector* derivedVectorType;
    ShSet* derivedSetType;
    ShReference* derivedRefType;

    ShScope* owner;
    virtual string getFullDefinition(const string& objName) const = 0;

public:
    ShType(ShTypeId iTypeId);
    ShType(const string& name, ShTypeId iTypeId);
    virtual ~ShType();
    
    void setOwner(ShScope*);
    ShTypeId getTypeId() const { return typeId; }
    void setTypeId(ShTypeId iTypeId);

    string getDefinition(const string& objName) const;
    string getDefinition() const;
    string getDefinitionQ() const; // quoted
    virtual string displayValue(const ShValue&) const = 0;

    bool isVoid() const { return typeId == typeVoid; }
    bool isTypeRef() const { return typeId == typeTypeRef; }
    bool isRange() const { return typeId == typeRange; }
    bool isOrdinal() const { return typeId >= typeInt8 && typeId <= typeBool; }
    bool isInt() const { return typeId >= typeInt8 && typeId <= typeInt64; }
    bool isChar() const { return typeId == typeChar; }
    bool isEnum() const  { return typeId == typeEnum; }
    bool isBool() const  { return typeId == typeBool; }
    bool isVector() const { return typeId == typeVector; }
    bool isEmptyVec() const;
    bool isArray() const { return typeId == typeArray; }
    bool isReference() const { return typeId == typeReference; }

    StorageModel storageModel() const
            { return stoModel; }
    offs staticSize() const
            { return size; }
    offs staticSizeAligned() const
            { return alignedSize; }
    bool isPod() const
            { return stoModel != stoVec; }
    bool isString() const;
    bool canBeArrayIndex()
            { return canCompareWith(this); }
    virtual bool equals(ShType*) const = 0;
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
    ShReference* deriveRefType();
    void setDerivedVectorTypePleaseThisIsCheatingIKnow(ShVector* v)
            { derivedVectorType = v; }

    // run-time functions
    virtual void rtFinalize(ptr) const
            { }
};

typedef ShType* PType;


class ShTypeAlias: public ShBase
{
public:
    ShType* const base;
    ShTypeAlias(const string& name, ShType* iBase);
};


class ShVariable: public ShBase
{
public:
    ShType* type;
    offs dataOffset;
    bool local;

    ShVariable(ShType* iType);
    ShVariable(const string& name, ShType* iType);
    bool isLocal() const { return local; }
};


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

    ShSymScope(ShSymScope* iParent);
    ShSymScope(const string& iName, ShTypeId iTypeId)
            : ShType(iName, iTypeId), parent(NULL)  { }

    virtual bool isScope() const
            { return true; }
    virtual string displayValue(const ShValue&) const
            { return "*undefined*"; }
    virtual bool equals(ShType* type) const
            { return false; }

    void addUses(ShModule*);
    void addSymbol(ShBase* obj);
    void finalizeVars(VmCodeGen& code);
    ShBase* find(const string& name) const
            { return symbols.find(name); }
    ShBase* deepFind(const string&) const;
};


// --- SCOPE --- //

class ShScope: public ShSymScope
{
protected:
    BaseList<ShType> types;
    BaseList<ShVariable> vars;
    BaseList<ShTypeAlias> typeAliases;
    BaseList<ShConstant> consts;
    
public:
    ShScope(const string& name, ShTypeId iTypeId);
    ~ShScope();
    void addAnonType(ShType*);
    void addType(ShType*, ShSymScope*);
    void addTypeAlias(ShTypeAlias*, ShSymScope*);
    void addConstant(ShConstant*, ShSymScope*);
    virtual void addVariable(ShVariable*, ShSymScope*);
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
    Range const range;

    virtual ShOrdinal* cloneWithRange(large min, large max) = 0;
    void reassignMax(int max)
        { ((Range&)range).max = max; }

public:
    ShOrdinal(ShTypeId iTypeId, large min, large max);
    ShOrdinal(const string& name, ShTypeId iTypeId, large min, large max);
    virtual bool canStaticCastTo(ShType* type) const
            { return type->isOrdinal(); }
    bool isLargeInt() const
            { return getTypeId() == typeInt64; }
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
    ShInteger(const string& name, large min, large max);
    virtual string displayValue(const ShValue& v) const;
    virtual bool canAssign(ShType* type) const
            { return type->isInt() && (isLargeInt() == PInteger(type)->isLargeInt()); }
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
//    ShChar(int min, int max);
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
    virtual string displayValue(const ShValue& v) const;
    virtual bool equals(ShType* type) const
            { return type->isVoid(); }
};


class ShTypeRef: public ShType
{
    virtual string getFullDefinition(const string& objName) const;
public:
    ShTypeRef(const string& name);
    virtual string displayValue(const ShValue& v) const;
    virtual bool canCheckEq(ShType* type) const
            { return type->isTypeRef(); }
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

    bool elementEquals(ShType* elemType) const
            { return elementType->equals(elemType); }
    bool isEmptyVec() const
            { return elementType->isVoid(); }

    bool isPodVector() const
            { return elementType->isPod(); }

    virtual void rtFinalize(ptr) const;
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
    ShValue(ShType* iType, int iValue): type(iType) { value.int_ = iValue; }
    ~ShValue()  { _finalize(); }

    void assignInt(ShType* iType, int i);
    void assignLarge(ShType* iType, large l);
    void assignPtr(ShType* iType, ptr p);
    void assignVec(ShType* iType, const string& s);
    void assignVoid(ShType* iType);
    void assignFromBuf(ShType*, const ptr);
    void assignToBuf(ptr);

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


class ShLocalScope: public ShScope
{
    // Usually belongs to a function/state, plus modules can create local
    // scopes for nested blocks to avoid pollution of the static space.
protected:
    virtual string getFullDefinition(const string& objName) const;
    VmCodeGen* codegen; // for registering vars and getting their addresses

public:
    ShLocalScope(VmCodeGen* iCodeGen);

    virtual void addVariable(ShVariable*, ShSymScope*);
};


// --- MODULE --- //

class ShModule: public ShScope
{
protected:
    // --- Compiler ---

    struct CompilerOptions
    {
        bool enableEcho;
        bool enableAssert;
        bool linenumInfo;
        CompilerOptions()
            : enableEcho(true), enableAssert(true), linenumInfo(true)  { }
    };

    string fileName;
    Parser parser;
    VmCodeGen* codegen;
    Array<string> vectorConsts;
    CompilerOptions options;
    ShLocalScope modLocalScope;

    ShSymScope* symbolScope; // for symbols only
    ShScope* varScope;    // static or stack-local scope

    string registerString(const string& v) // TODO: fund duplicates
            { vectorConsts.add(v); return v; }
    string registerVector(const string& v)
            { vectorConsts.add(v); return v; }
    virtual void addVariable(ShVariable*, ShSymScope*);

    void error(const string& msg)           { parser.error(msg); }
    void error(EDuplicate& e);
    void error(const char* msg)             { parser.error(msg); }
    void errorWithLoc(const string& msg)    { parser.errorWithLoc(msg); }
    void errorWithLoc(const char* msg)      { parser.errorWithLoc(msg); }
    void errorNotFound(const string& msg)   { parser.errorNotFound(msg); }
    void notImpl()                          { error("Feature not implemented"); }

    ShBase* getQualifiedName();
    ShType* getDerivators(ShType*);
    ShType* getTypeOrNewIdent(string* strToken);
    void    getConstCompound(ShType*, ShValue&);
    ShType* getTypeExpr(bool anyObj);
    ShType* parseIfFunc(VmCodeGen& code);
    ShType* parseAtom(VmCodeGen&, bool isLValue);
    ShType* parseDesignator(VmCodeGen&, bool isLValue);
    ShInteger* arithmResultType(ShInteger* left, ShInteger* right);
    ShType* parseFactor(VmCodeGen&);
    ShType* parseTerm(VmCodeGen&);
    ShType* parseArithmExpr(VmCodeGen&);
    ShType* parseSimpleExpr(VmCodeGen&);
    ShType* parseRelExpr(VmCodeGen&);
    ShType* parseNotLevel(VmCodeGen&);
    ShType* parseAndLevel(VmCodeGen&);
    ShType* parseOrLevel(VmCodeGen&);
    ShType* parseSubrange(VmCodeGen&);
    ShType* parseBoolExpr(VmCodeGen& code)
            { return parseOrLevel(code); }
    ShType* parseExpr(VmCodeGen& code)
            { return parseSubrange(code); }
    ShType* parseExpr(VmCodeGen& code, ShType* resultType);
    void getConstExpr(ShType* typeHint, ShValue& result);

    ShEnum* parseEnumType();
    void parseTypeDef();
    void parseVarConstDef(bool isVar, VmCodeGen&);
    void parseEcho(VmCodeGen&);
    void parseAssert(VmCodeGen&);
    void parseOtherStatement(VmCodeGen&);
    void parseBlock(VmCodeGen& code);
    void enterBlock(VmCodeGen& code);

protected:
    virtual string getFullDefinition(const string& objName) const;

public:
    bool compiled;

    ShModule(const string& filename);
    ~ShModule();
    bool compile();
    void dump(string indent) const;

    // --- RUNTIME --- //
protected:
    void setupRuntime();
    VmCodeSegment mainCode;
    offs dataSize;
    char* dataSegment;
    
public:
    void execute();
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

