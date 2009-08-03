

#include "common.h"
#include "runtime.h"
#include "parser.h"
#include "typesys.h"
#include "vm.h"

#include <stdlib.h>


// --- HIS MAJESTY, THE COMPILER ------------------------------------------- //


struct CompilerOptions
{
    bool enableDump;
    bool enableAssert;
    bool linenumInfo;
    bool vmListing;

    CompilerOptions()
      : enableDump(true), enableAssert(true), linenumInfo(true),
        vmListing(true)  { }
};


class Compiler: protected Parser
{
protected:
    Module& mainModule;
    CompilerOptions options;
    mem fileId;
    bool started;

    CodeGen* codegen;
    Scope* scope;           // for storing definitions
    BlockScope* blockScope; // for local vars in nested blocks, can be NULL
    State* state;           // for this-vars and type objects

    Type* getTypeDerivators(Type*);
    Type* compoundCtorElem(Type*);
    void compoundCtor(Type*);
    void enumeration(const str&);
    void atom();
    void designator(bool expectAssignment = false);
    void factor();
    void term();
    void arithmExpr();
    void simpleExpr();
    void relation();
    void notLevel();
    void andLevel();
    void orLevel();
    void expression()
            { orLevel(); }
    void expression(Type* expectType, bool allowRange = false);
    Type* constExpr(Type* expectType, variant& result);
    Type* typeExpr();

    Type* getTypeAndIdent(str& ident);
    void definition();
    void variable();
    void echo();
    void assertion();
    void assignment()
            { designator(true); }
    void block();
    void statementList();

public:
    Compiler(const str&, fifo*, Module&);
    ~Compiler();

    void compile();
};


Compiler::Compiler(const str& _fn, fifo* _input, Module& _main)
  : Parser(_fn, _input), mainModule(_main), started(false),
    codegen(NULL), scope(NULL), blockScope(NULL), state(NULL)  { }

Compiler::~Compiler()  { }


// --- EXPRESSION ---------------------------------------------------------- //

/*
    1. <nested-expr>  <ident>  <number>  <string>  <char>  <compound-ctor>
    2. <array-sel>  <member-sel>  <function-call>
    3. unary-  <type-derivators>  as  is
    4. *  /  mod
    5. +  –
    6. |
    7. ==  <>  !=  <  >  <=  >=  in
    8. not
    9. and
    10. or  xor
*/


Type* Compiler::getTypeDerivators(Type* type)
{
    if (skipIf(tokLSquare))
    {
        if (token == tokRSquare)
            type = type->deriveVector();
        else if (skipIf(tokRange))
            type = type->deriveSet();
        else
        {
            Type* indexType = typeExpr();
            if (indexType->isNone())
                type = type->deriveVector();
            else
                type = type->createContainer(indexType);
        }
        skip(tokRSquare, "]");
        return getTypeDerivators(type);
    }

    else if (skipIf(tokNotEq)) // <>
        return getTypeDerivators(type->deriveFifo());

    else if (skipIf(tokLAngle))
    {
        skip(tokRAngle, ">");
        return getTypeDerivators(type->deriveFifo());
    }

    // TODO: function derivator
    return type;
}


Type* Compiler::compoundCtorElem(Type* type)
{
    // If the type of this container is not known then it is assumed one of:
    // * dictionary, if elements are key/value pairs
    // * range, if the constructor consists of a single range
    // * vector otherwise (and ranges are not allowed)
    // So sets and arrays are not possible to define if type is not known
    // beforehand.
    
    Type* first = NULL, * second = NULL;
    if (type != NULL)
    {
        if (type->isDict() || type->isArray())
        {
            first = PCont(type)->index;
            second = PCont(type)->elem;
        }
        else if (type->isVector())
            first = PCont(type)->elem;
        else if (type->isSet() || type->isOrdset())
            first = PCont(type)->index;
    }

    expression(first);
    Type* elemType = codegen->getTopType();

    if (skipIf(tokAssign))
    {
        if (type != NULL && !type->isDict() && !type->isArray())
            error("Key/value pair not allowed here");
        expression(second);
        if (type == NULL)
        {
            Type* keyType = elemType;
            elemType = codegen->getTopType();
            type = elemType->createContainer(keyType);
        }
    }

    else if (skipIf(tokRange))
    {
        if (!elemType->isOrdinal())
            error("Ordinal expected as range boundary");
        if (type != NULL)
        {
            if (type->isOrdset())
                type = CAST(Ordinal*, POrdset(type)->index)->deriveRange();
            else if (type->isRange())
                ;
            else
                error("Range not allowed: not an ordinal set");
        }
        else
            type = POrdinal(elemType)->deriveRange();
        expression(PRange(type)->base);
        codegen->mkRange(PRange(type));
    }

    else
    {
        if (type != NULL)
        {
            if (!type->isOrdset() && !type->isSet() && !type->isVector())
                error("Invalid container constructor");
        }
        else
            type = elemType->deriveVector();
    }

    return type;
}


void Compiler::compoundCtor(Type* expectType)
{
    // compound-ctor ::= "[" [ element-ctor { "," element-ctor } ] "]"
    // element-ctor ::= [ expr ( "=" | ".." ) ] expr

    // skip(tokLSquare, "[");
    if (skipIf(tokRSquare))
    {
        codegen->loadNullComp(expectType);
        return;
    }

    // First element
    Type* type = compoundCtorElem(expectType);

    if (type->isRange())
    {
        // If type is known and it's ordset
        if (expectType != NULL && expectType->isOrdset())
            codegen->rangeToOrdset(POrdset(expectType));
        // Otherwise we allow only a single range
        else
        {
            if (token != tokRSquare)
                error("Set constructor is not allowed when type is undefined");
            next();
            return;
        }
    }
    else if (type->isDict())
        codegen->pairToDict(PDict(type));
    else if (type->isArray())
        codegen->pairToArray(PArray(type));
    else if (type->isVector())
        codegen->elemToVec(PVec(type));
    else if (type->isOrdset() || type->isSet())
        codegen->elemToSet(PCont(type));
    else
        notimpl();

    while (skipIf(tokComma))
    {
        if (token == tokRSquare)    // allow trailing comma
            break;
        // The rest of elements: type is known already, either from the first
        // element, or it was known beforehand. Note that if an element is a 
        // range, compondElemCtor() returns that range type; in all other cases
        // container type is returned.
        type = compoundCtorElem(type);
        if (type->isRange())
            codegen->addRangeToOrdset(false);
        else if (type->isDict() || type->isArray())
            codegen->storeContainerElem(false);
        else if (type->isVector())
            codegen->elemCat();
        else if (type->isOrdset() || type->isSet())
            codegen->addToSet(false);
        else
            notimpl();
    }

    skip(tokRSquare, "]");
}


void Compiler::enumeration(const str& firstIdent)
{
    Enumeration* enumType = state->registerType(new Enumeration());
    enumType->addValue(firstIdent);
    while (skipIf(tokComma))
    {
        if (token == tokRParen) // allow trailing comma
            break;
        enumType->addValue(getIdentifier());
        next();
    }
    skip(tokRParen, ")");
    codegen->loadTypeRef(enumType);
}


void Compiler::atom()
{
    if (token == tokPrevIdent)  // from partial (typeless) definition or enum spec
    {
        Symbol* s = scope->findDeep(getPrevIdent());
        codegen->loadSymbol(s);
        redoIdent();
    }

    else if (token == tokIntValue)
    {
        codegen->loadInt(intValue);
        next();
    }

    else if (token == tokStrValue)
    {
        str value = strValue;
        if (value.size() == 1)
            codegen->loadChar(value[0]);
        else
            codegen->loadStr(value);
        next();
    }

    else if (token == tokIdent)
    {
        Symbol* s = scope->findDeep(strValue);
        codegen->loadSymbol(s);
        next();
    }

    else if (skipIf(tokLParen))
    {
        if (token == tokIdent)
        {
            str ident = strValue;
            if (next() == tokComma)
                enumeration(ident);
            else
            {
                undoIdent(ident);
                goto ICantBelieveIUsedAGotoStatementShameShame;
            }
        }
        else
        {
ICantBelieveIUsedAGotoStatementShameShame:
            expression();
            skip(tokRParen, ")");
        }
    }

    else if (skipIf(tokLSquare))
        compoundCtor(NULL);

    else
        errorWithLoc("Expression syntax");
}


void Compiler::designator(bool expectAssignment)
{
    atom();
    while (1)
    {
        if (skipIf(tokPeriod))
        {
            codegen->loadMember(getIdentifier());
            next();
        }
        else if (skipIf(tokLSquare))
        {
            // TODO: Compound typecast
            expression();
            codegen->loadContainerElem();
            skip(tokRSquare, "]");
        }
        else if (skipIf(tokLParen))
        {
            Type* typeRef = codegen->getLastTypeRef();
            if (typeRef != NULL)
            {
                if (typeRef->isState())
                {
                    // TODO: function call or typecast
                    notimpl();
                }
                else
                {
                    expression();
                    codegen->explicitCastTo(typeRef);
                    skip(tokRParen, ")");
                }
            }
            else
                // indirect function call?
                notimpl();
        }
        else if (expectAssignment && skipIf(tokAssign))
        {
            str code;
            Type* destType = codegen->detachDesignatorOp(code);
            expression(destType);
            codegen->store(code, destType);
        }
        else
            break;
    }
}


void Compiler::factor()
{
    bool isNeg = skipIf(tokMinus);
    designator();
    if (isNeg)
        codegen->arithmUnary(opNeg);
    else if (token == tokWildcard)
    {
        // anonymous type spec
        Type* type = codegen->getLastTypeRef();
        if (type != NULL)
        {
            next();
            codegen->loadTypeRef(getTypeDerivators(type));
        }
    }
}


void Compiler::term()
{
    factor();
    while (token == tokMul || token == tokDiv || token == tokMod)
    {
        OpCode op = token == tokMul ? opMul
                : token == tokDiv ? opDiv : opMod;
        next();
        factor();
        codegen->arithmBinary(op);
    }
}


void Compiler::arithmExpr()
{
    term();
    while (token == tokPlus || token == tokMinus)
    {
        OpCode op = token == tokPlus ? opAdd : opSub;
        next();
        term();
        codegen->arithmBinary(op);
    }
}


void Compiler::simpleExpr()
{
    arithmExpr();
    if (skipIf(tokCat))
    {
        Type* type = codegen->getTopType();
        if (!type->isVector())
            codegen->elemToVec();
        do
        {
            arithmExpr();
            type = codegen->getTopType();
            if (!type->isVector())
                codegen->elemCat();
            else
                codegen->cat();
        }
        while (skipIf(tokCat));
    }
}


void Compiler::relation()
{
    simpleExpr();
    if (token >= tokCmpFirst && token <= tokCmpLast)
    {
        OpCode op = OpCode(opCmpFirst + int(token - tokCmpFirst));
        next();
        simpleExpr();
        codegen->cmp(op);
    }
    else if (skipIf(tokIn))
    {
        simpleExpr();
        Type* type = codegen->getTopType();
        if (skipIf(tokRange))
        {
            codegen->implicitCastTo2(type);
            simpleExpr();
            codegen->inBounds();
        }
        else
        {
            if (type->isRange())
                codegen->inRange();
            else if (type->isOrdset() || type->isSet())
                codegen->inSet();
            else if (type->isDict())
                codegen->keyInDict();
            else if (type->isTypeRef())
            {
                Type* typeRef = codegen->getLastTypeRef();
                if (typeRef == NULL)
                    error("Variable typeref is not allowed on the right of 'in'");
                if (!typeRef->isOrdinal())
                    error("Ordinal typeref expected on the right of 'in'");
                codegen->implicitCastTo(typeRef); // note, the value is on the top of stack now
                codegen->loadInt(POrdinal(typeRef)->left);
                codegen->loadInt(POrdinal(typeRef)->right);
                codegen->inBounds();
            }
            else
                error("Range, set, dictionary, or ordinal typeref expected on the right of 'in'");
        }
    }
}


void Compiler::notLevel()
{
    bool isNot = skipIf(tokNot);
    relation();
    if (isNot)
        codegen->_not(); // 'not' is something reserved, probably only with Apple's GCC
}


void Compiler::andLevel()
{
    notLevel();
    Type* type = codegen->getTopType();
    if (type->isBool())
    {
        if (skipIf(tokAnd))
        {
            mem offs = codegen->boolJumpForward(opJumpAnd);
            andLevel();
            codegen->resolveJump(offs);
        }
    }
    else if (type->isInt())
    {
        while (token == tokShl || token == tokShr || token == tokAnd)
        {
            OpCode op = token == tokShl ? opBitShl
                    : token == tokShr ? opBitShr : opBitAnd;
            next();
            notLevel();
            codegen->arithmBinary(op);
        }
    }
}


void Compiler::orLevel()
{
    andLevel();
    Type* type = codegen->getTopType();
    if (type->isBool())
    {
        if (skipIf(tokOr))
        {
            mem offs = codegen->boolJumpForward(opJumpOr);
            orLevel();
            codegen->resolveJump(offs);
        }
        else if (skipIf(tokXor))
        {
            orLevel();
            codegen->boolXor();
        }
    }
    else if (type->isInt())
    {
        while (token == tokOr || token == tokXor)
        {
            OpCode op = token == tokOr ? opBitOr : opBitXor;
            next();
            andLevel();
            codegen->arithmBinary(op);
        }
    }
}


void Compiler::expression(Type* expectType, bool allowRange)
{
    if (expectType != NULL)
    {
        if (expectType->isTypeRef())
        {
            factor();
        }
        else if (expectType != NULL && expectType->isCompound() && token == tokLSquare)
        {
            next();
            compoundCtor(expectType);
        }
        else
            expression();
    }
    else
        expression();

    if (allowRange && skipIf(tokRange))
    {
        factor();
        codegen->mkRange(NULL);
    }
    else if (expectType != NULL)
        codegen->implicitCastTo(expectType);
}


Type* Compiler::constExpr(Type* expectType, variant& result)
{
    ConstCode constCode;
    CodeGen constCodeGen(&constCode);
    CodeGen* prevCodeGen = exchange(codegen, &constCodeGen);

    expression(expectType, true);

    Type* valueType = codegen->getTopType();
    constCodeGen.endConstExpr(NULL);
    constCode.run(result);
    if (valueType->isRange())
    {
        range* r = CAST(range*, result._obj());
        result = CAST(Range*, valueType)->base->createSubrange(r->left, r->right);
        valueType = defTypeRef;
    }
    codegen = prevCodeGen;
    return valueType;
}


Type* Compiler::typeExpr()
{
    variant result;
    Type* resultType = constExpr(defTypeRef, result);
    if (!resultType->isTypeRef())
        error("Type expression expected");
    return CAST(Type*, result._obj());
}


// ------------------------------------------------------------------------- //


Type* Compiler::getTypeAndIdent(str& ident)
{
    Type* type = NULL;
    if (token == tokIdent)
    {
        ident = strValue;
        if (next() == tokAssign)
            goto ICantBelieveIUsedAGotoStatement;
        undoIdent(ident);
    }
    type = typeExpr();
    ident = getIdentifier();
    next();
    type = getTypeDerivators(type);
ICantBelieveIUsedAGotoStatement:
    skip(tokAssign, "=");
    return type;
}


void Compiler::definition()
{
    // definition ::= 'def' [ type-expr ] ident { type-derivator } '=' type-expr
    str ident;
    Type* type = getTypeAndIdent(ident);
    variant value;
    Type* valueType = constExpr(type, value);
    if (type == NULL)
        type = valueType;
    if (type->isTypeRef())
        scope->addTypeAlias(ident, CAST(Type*, value._obj()));
    else
    {
        if (type->isOrdinal() && !POrdinal(type)->isInRange(value.as_ord()))
            error("Constant out of range");
        scope->addConstant(type, ident, value);
    }
    skipSep();
}


void Compiler::variable()
{
    // definition ::= 'var' [ type-expr ] ident { type-derivator } '=' expr
    str ident;
    Type* type = getTypeAndIdent(ident);
    expression(type);
    if (type == NULL)
        type = codegen->getTopType();
    if (type->isNullComp())
        error("Type undefined (null compound)");
    Variable* var;
    if (blockScope != NULL)
        var = blockScope->addLocalVar(type, ident);
    else
        var = state->addThisVar(type, ident);
    codegen->initVar(var);
    skipSep();
}


void Compiler::echo()
{
    mem codeOffs = codegen->getCurPos();
    if (token != tokSep)
    {
        while (1)
        {
            expression();
            codegen->dumpVar();
            if (!skipIf(tokComma))
                break;
        }
    }
    codegen->echoLn();
    if (!options.enableDump)
        codegen->discardCode(codeOffs);
    skipSep();
}


void Compiler::assertion()
{
    mem codeOffs = codegen->getCurPos();
    expression();
    if (!options.linenumInfo)
        codegen->linenum(fileId, getLineNum());
    codegen->assertion();
    if (!options.enableAssert)
        codegen->discardCode(codeOffs);
    skipSep();
}


void Compiler::block()
{
    BlockScope localScope(scope, codegen);
    scope = &localScope;
    BlockScope* saveBlockScope = exchange(blockScope, &localScope);
    statementList();
    localScope.deinitLocals();
    blockScope = saveBlockScope;
    scope = localScope.outer;
}


void Compiler::statementList()
{
    while (!skipIf(tokBlockEnd))
    {
        if (options.linenumInfo)
            codegen->linenum(fileId, getLineNum());

        if (skipIf(tokSep))
            ;
        else if (skipIf(tokDef))
            definition();
        else if (skipIf(tokVar))
            variable();
        else if (skipIf(tokDump))
            echo();
        else if (skipIf(tokAssert))
            assertion();
        else if (skipIf(tokBlockBegin))
            block();
        else
            assignment();
    }
}


void Compiler::compile()
{
    if (started)
        fatal(0x7001, "Compiler object can't be used more than once");
    started = true;

    fileId = mainModule.registerFileName(getFileName());
    CodeGen mainCodeGen(&mainModule);
    codegen = &mainCodeGen;
    scope = state = &mainModule;
    blockScope = NULL;

    try
    {
        next();
        statementList();
        skip(tokEof, "<EOF>");
    }
    catch (EDuplicate& e)
    {
        error("'" + e.ident + "' is already defined within this scope");
    }
    catch (EUnknownIdent& e)
    {
        error("'" + e.ident + "' is unknown in this context");
    }
    catch (EParser&)
    {
        throw;  // comes with file name and line no. already
    }
    catch (exception& e)
    {
        error(e.what());
    }

    mainCodeGen.end();

    if (options.vmListing)
    {
        outtext f(NULL, remove_filename_ext(getFileName()) + ".lst");
        mainModule.listing(f);
    }
}


int executeFile(const str& fileName)
{
    Module module(remove_filename_path(remove_filename_ext(fileName)));
    Compiler compiler(fileName, new intext(NULL, fileName), module);

    // Look at these two beautiful lines. Compiler, compile. Module, run. Love it.
    compiler.compile();
    variant result = module.run();

#ifdef DEBUG
//    queenBee->dumpContents(sio);
//    module.dumpContents(sio);
#endif

    if (result.is_null())
        return 0;
    else if (result.is_ord())
        return result._ord();
    else if (result.is_str())
    {
        serr << result.as_str() << endl;
        return 102;
    }
    else
        return 103;
}


// --- tests --------------------------------------------------------------- //


#define check(x) assert(x)


#ifdef XCODE
    const char* fileName = "../../src/tests/test.shn";
#else
    const char* fileName = "tests/test.shn";
#endif

int main()
{
    int exitcode = 0;
    initTypeSys();
    try
    {
        exitcode = executeFile(fileName);
    }
    catch (std::exception& e)
    {
        serr << "Error: " << e.what() << endl;
        exitcode = 101;
    }
    doneTypeSys();

#ifdef DEBUG
    assert(object::alloc == 0);
#endif

    return exitcode;
}

