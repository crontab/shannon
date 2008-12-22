
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

ShBase* ShModule::getQualifiedName()
{
    // qualified-name ::= { ident "." } ident
    string ident = parser.getIdent();
    ShBase* obj = currentScope->deepSearch(ident);
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
    // TODO: read constant names as well; check previewObj
    // TODO: const expr
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
    return ShValue();
}


ShValue ShModule::getConstExpr(ShType* typeHint)
{
    // TODO:
    ShValue result;
    if (typeHint == NULL)
    {
        result = getOrdinalConst();
        typeHint = result.type;
    }
    else if (typeHint->isOrdinal())
    {
        result = getOrdinalConst();
        if (result.type != NULL && !((ShOrdinal*)typeHint)->isCompatibleWith(result.type))
            error("Type mismatch in constant expression");
    }
    if (result.type == NULL)
        error("Constant expression expected");
    return result;
}


// --- TYPES --------------------------------------------------------------- //

/*
ShRange* ShModule::getRangeType()
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
            ShType* indexType = getType(NULL);
            parser.skip(tokRSquare, "]");
            if (!indexType->canBeArrayIndex())
                error(indexType->getDisplayName("") + " can't be used as array index");
            type = type->deriveArrayType(indexType, currentScope);
        }
        type = getDerivators(type);
    }
    return type;
}


ShType* ShModule::getType(ShBase* previewObj)
{
    // type ::= type-id { type-derivator }
    // type-id ::= ident | "typeof" "(" type-expr ")" | range
    // TODO: range type
    if (previewObj == NULL)
        previewObj = getQualifiedName();
    ShType* type = NULL;
    if (previewObj->isTypeAlias())
        type = ((ShTypeAlias*)previewObj)->base;
    else if (previewObj->isType())
        type = (ShType*)previewObj;
    else
        parser.errorWithLoc("Expected type identifier");
    return getDerivators(type);
}


// --- STATEMENTS/DEFINITIONS ---------------------------------------------- //

ShBase* ShModule::getAtom()
{
    if (parser.token == tokIdent)
        return getQualifiedName();
    error("Error in statement");
    return NULL;
}


void ShModule::parseTypeDef()
{
    // type-alias ::= "def" type ident { type-derivator }
    ShType* type = getType(NULL);
    string ident = parser.getIdent();
    type = getDerivators(type);
    ShTypeAlias* alias = new ShTypeAlias(ident, type);
    try
    {
        currentScope->addTypeAlias(alias);
    }
    catch (Exception& e)
    {
        delete alias;
        throw;
    }
    parser.skipSep();
}


void ShModule::parseConstDef()
{
    // const-def ::= "const" [ type ] ident "=" const-expr
    string ident;
    ShType* type = NULL;
    if (parser.token == tokIdent)
    {
        ident = parser.strValue;
        parser.next();
        ShBase* obj = deepFind(ident);
        if (obj != NULL && obj->isType())
        {
            type = getType(obj);
            ident = parser.skipIdent();
        }
    }
    else
    {
        type = getType(NULL);
        ident = parser.skipIdent();
    }
    
    if (type != NULL)
        type = getDerivators(type);
        
    parser.skip(tokEqual, "=");
    
    ShValue value = getConstExpr(type);
    if (type == NULL)
        type = value.type;
        
    parser.skipSep();
}


void ShModule::parseObjectDef(ShBase* previewObj)
{
    // object-def ::= type ident [ "=" expr ]
    ShType* type = getType(previewObj);
    string ident = parser.getIdent();
    type = getDerivators(type);
    ShVariable* var = new ShVariable(ident, type);
    try
    {
        currentScope->addVariable(var);
    }
    catch(Exception& e)
    {
        delete var;
        throw;
    }

    if (parser.token == tokEqual)
    {
        parser.next();
        ShValue value = getOrdinalConst();
        if (value.type == NULL)
            error("Constant value expected");
    }

    parser.skipSep();
}


void ShModule::compile()
{
    try
    {
        currentScope = this;
        
        parser.next();
        
        // module-header ::= "module" ident
        if (parser.token == tokModule)
        {
            parser.next();
            string modName = parser.getIdent();
            if (strcasecmp(modName.c_str(), name.c_str()) != 0)
                error("Module name mismatch");
            setNamePleaseThisIsBadIKnow(modName);
            parser.skipSep();
        }

        // { statement | definition }
        while (parser.token != tokEof)
        {
            // definition ::= type-def | const-def | object-def | function-def

            if (parser.token == tokDef)
            {
                parser.next();
                parseTypeDef();
            }
            
            else if (parser.token == tokConst)
            {
                parser.next();
                parseConstDef();
            }
            
            else
            {
                ShBase* obj = getAtom();
                if (obj->isType() || obj->isTypeAlias())
                    parseObjectDef(obj);
                else
                    parser.errorWithLoc("Expected definition or statement");
            }
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

