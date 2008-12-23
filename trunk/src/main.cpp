
#include <stdio.h>

#include "str.h"
#include "except.h"
#include "langobj.h"


// --- VIRTUAL MACHINE ----------------------------------------------------- //


union VmQuant
{
    ptr ptr_;
    int int_;
    large large_;
};


class VmStack: public noncopyable
{
protected:
    PodStack<VmQuant> stack;
public:
    VmStack();
    VmQuant& push()           { return stack.push(); }
    VmQuant  pop()            { return stack.pop(); }
    void  pushInt(int v)      { push().int_ = v; }
    void  pushPtr(ptr v)      { push().ptr_ = v; }
    void  pushLarge(large v);
    int   popInt()            { return pop().int_; }
    ptr   popPtr()            { return pop().ptr_; }
    large popLarge();
    int   topInt()            { return stack.top().int_; }
    ptr   topPtr()            { return stack.top().ptr_; }
    large topLarge();
};


enum OpCode
{
    opNone = 0,
    
    opLoad0,        // []
    opLoadInt,      // [int]
    opLoadLarge,    // [int,int]
    opLoadChar,     // [int]
    opLoadFalse,    // []
    opLoadTrue,     // []
    opLoadNull,     // []
    opLoadStr,      // [string-data-ptr]
};


class VmCode: public noncopyable
{
protected:
    PodArray<ShValue> code;

public:
    ShType* const returnType;

    VmCode(ShType* iReturnType)
            : code(), returnType(iReturnType)  { }
};



// ------------------------------------------------------------------------- //
// --- HIS MAJESTY THE COMPILER -------------------------------------------- //
// ------------------------------------------------------------------------- //


// --- CONST EXPRESSION ---------------------------------------------------- //

/*
    - (unary), not
    *, /, div, mod, and, shl, shr, as
    +, –, or, xor
    =, <>, <, >, <=, >=, in, is
    >>, <<
*/

ShBase* ShModule::getQualifiedName()
{
    // qualified-name ::= { ident "." } ident
    string ident = parser.getIdent();
    ShBase* obj = currentScope->deepFind(ident);
    if (obj == NULL)
        error("Unknown identifier '" + ident + "'");
    while (parser.token == tokPeriod)
    {
        if (!obj->isScope())
            return obj;
        ShScope* scope = (ShScope*)obj;
        parser.next(); // "."
        ident = parser.getIdent();
        obj = scope->find(ident);
        if (obj == NULL)
            error("'" + ident + "' is not known within '" + scope->name + "'");
    }
    return obj;
}


ShValue ShModule::getOrdinalConst()
{
    // TODO: const ordinal expr
    // expr can start with a number, char, const ident or '-'
    if (parser.token == tokIntValue)
    {
        large value = parser.intValue;
        ShInteger* type = queenBee->defaultInt->contains(value) ?
            queenBee->defaultInt : queenBee->defaultLarge;
        parser.next();
        return ShValue(type, parser.intValue);
    }
    else if (parser.token == tokStrValue)
    {
        if (parser.strValue.size() != 1)
            error("Character expected instead of '" + parser.strValue + "'");
        int value = parser.strValue[0];
        parser.next();
        return ShValue(queenBee->defaultChar, value);
    }
    else if (parser.token == tokIdent)
    {
        ShBase* obj = getQualifiedName();
        if (obj->isConstant())
            return ((ShConstant*)obj)->value;
    }
    errorWithLoc("Constant expression expected");
    return ShValue();
}


ShValue ShModule::getConstExpr(ShType* typeHint)
{
    ShValue result;
    result = getOrdinalConst(); // TODO: general const expr
    if (typeHint == NULL)
        typeHint = result.type;

    if (typeHint->isOrdinal() && result.type->isOrdinal())
    {
        ShOrdinal* ordHint = (ShOrdinal*)typeHint;
        if (!ordHint->isCompatibleWith(result.type))
            error("Type mismatch in constant expression");
        if (!ordHint->contains(result.largeValue()))
            error("Value out of range");
    }
    else if (!typeHint->canAssign(result))
        error("Type mismatch in constant expression");

    return result;
}


// --- TYPES --------------------------------------------------------------- //

/*
ShOrdinal* ShModule::getRangeType()
{
    return NULL;
}
*/

ShType* ShModule::getDerivators(ShType* type)
{
    // array-derivator ::= "[" [ type ] "]"
    if (parser.token == tokLSquare)
    {
        parser.next();
        if (parser.token == tokRSquare)
        {
            type = type->deriveVectorType(currentScope);
            parser.next();
        }
        else
        {
            ShType* indexType = getType();
            parser.skip(tokRSquare, "]");
            if (!indexType->canBeArrayIndex())
                error(indexType->getDisplayName("") + " can't be used as array index");
            type = type->deriveArrayType(indexType, currentScope);
        }
        type = getDerivators(type);
    }
    return type;
}


ShType* ShModule::getType()
{
    // type ::= type-id { type-derivator }
    // type-id ::= qualified-name | "typeof" "(" type-expr ")" | range

    // TODO: check if this is a range first
    ShType* type = NULL;
    ShBase* obj = getQualifiedName();
    if (obj->isTypeAlias())
        type = ((ShTypeAlias*)obj)->base;
    else if (obj->isType())
        type = (ShType*)obj;
    if (type == NULL)
        errorWithLoc("Expected type specifier");
    return getDerivators(type);
}


// --- STATEMENTS/DEFINITIONS ---------------------------------------------- //


void ShModule::parseTypeDef()
{
    ShType* type = getType();
    string ident = parser.getIdent();
    type = getDerivators(type);
    addObject(new ShTypeAlias(ident, type));
}


void ShModule::parseVarConstDef(bool isVar)
{
    // TODO: skip type if ident is new or if it is followed by '='
    ShType* type = getType();
    string ident = parser.getIdent();
    type = getDerivators(type);
    parser.skip(tokAssign, "=");
    ShValue value = getConstExpr(type);
    if (isVar)
        addObject(new ShVariable(ident, type)); // TODO: initializer
    else
        addObject(new ShConstant(ident, value));
}


void ShModule::compile()
{
    try
    {
        currentScope = this;
        
        parser.next();
        
        if (parser.token == tokModule)
        {
            parser.next();
            string modName = parser.getIdent();
            if (strcasecmp(modName.c_str(), name.c_str()) != 0)
                error("Module name mismatch");
            setNamePleaseThisIsBadIKnow(modName);
            parser.skipSep();
        }

        while (parser.token != tokEof)
        {
            if (parser.skipIf(tokDef))
                parseTypeDef();
            else if (parser.skipIf(tokConst))
                parseVarConstDef(false);
            else if (parser.skipIf(tokVar))
                parseVarConstDef(true);
            else
                errorWithLoc("Expected definition or statement");
            parser.skipSep();
        }

        compiled = true;

#ifdef DEBUG
        dump("");
#endif

    }
    catch(Exception& e)
    {
        fprintf(stderr, "%s\n", e.what().c_str());
    }
}


// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //


VmStack::VmStack()
        : stack()  { }

void VmStack::pushLarge(large v)
        { push().int_ = int(v); push().int_ = int(v >> 32); }

large VmStack::popLarge()
        { return (large(popInt()) << 32) | popInt(); }

large VmStack::topLarge()
        { return (large(stack.at(-1).int_) << 32) | stack.at(-2).int_; }


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
        if (stackimpl::stackAlloc != 0)
            fprintf(stderr, "Internal: stackAlloc = %d\n", stackimpl::stackAlloc);
    }
} _atexit;




int main()
{
    try
    {
        initLangObjs();
        
#ifdef XCODE
        ShModule module("../../src/tests/test.sn");
#else
        ShModule module("tests/test.sn");
#endif
        module.compile();
        
        doneLangObjs();
    }
    catch (Exception& e)
    {
        fprintf(stderr, "\n*** Exception: %s\n", e.what().c_str());
    }

    return 0;
}

