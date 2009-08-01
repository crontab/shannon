

#include "common.h"
#include "runtime.h"
#include "parser.h"
#include "typesys.h"
#include "vm.h"

#include <stdlib.h>


// --- HIS MAJESTY, THE COMPILER ------------------------------------------- //


struct CompilerOptions
{
    bool enableEcho;
    bool enableAssert;
    bool linenumInfo;
    bool vmListing;

    CompilerOptions()
      : enableEcho(true), enableAssert(true), linenumInfo(true),
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
    BlockScope* blockScope; // for local vars in nested blocks
    State* state;           // for this-vars

    Type* getTypeDerivators(Type*);
    Type* compoundCtorElem(Type*);
    void compoundCtor(Type*);
    void atom();
    void designator();
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
    void expression(Type* expectType);
    Type* constExpr(Type* expectType, variant& result);
    Type* typeExpr();

    Type* getTypeAndIdent(str& ident);
    void definition();
    void variable();
    void echo();
    void assertion();
    void block();

public:
    Compiler(const str&, fifo_intf*, Module&);
    ~Compiler();

    void compile();
};


Compiler::Compiler(const str& _fn, fifo_intf* _input, Module& _main)
  : Parser(_fn, _input), mainModule(_main), started(false),
    codegen(NULL), scope(NULL), blockScope(NULL), state(NULL)  { }

Compiler::~Compiler()  { }


template <class T>
    inline T exchange(T& target, const T& value)
        { T temp = target; target = value; return temp; }


// --- EXPRESSION ---------------------------------------------------------- //

/*
    1. <nested-expr>  <ident>  <number>  <string>  <char>  <compound-ctor>
    2. <array-sel>  <member-sel>  <function-call>
    3. unary-  <type-derivators>  as  is
    4. *  /  mod
    5. +  –
    6. |
    7. ==  <>  !=  <  >  <=  >=  has
    8. not
    9. and
    10. or  xor
*/


Type* Compiler::getTypeDerivators(Type* type)
{
    if (skipIf(tokLSquare))
    {
        if (skipIf(tokRSquare))
            type = type->deriveVector();
        else
        {
            Type* indexType = typeExpr();
            if (type->isNone())
                type = type->deriveSet();
            else if (indexType->isNone())
                type = type->deriveVector();
            else
                type = type->createContainer(indexType);
            skip(tokRSquare, "]");
        }
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
    // If the type of this container is not known then it is assumed to be of:
    // * dictionary, if elements are key/value pairs
    // * range, if the constructor consists of a single range
    // * vector otherwise (and ranges are not allowed)
    // So sets are not possible to define if type is not given beforehand.
    expression();
    Type* elemType = codegen->getTopType();

    if (skipIf(tokAssign))
    {
        if (type != NULL)
        {
            if (!type->isDict())
                error("Key/value pair not allowed here");
            codegen->implicitCastTo(PDict(type)->index, "Dictionary key type mismatch");
        }
        expression();
        if (type != NULL)
            codegen->implicitCastTo(PDict(type)->elem, "Dictionary element type mismatch");
        else
        {
            Type* keyType = elemType;
            elemType = codegen->getTopType();
            type = elemType->createContainer(keyType);
        }
    }

    else if (skipIf(tokRange))
    {
        if (type != NULL)
        {
            if (type->isOrdset())
                elemType = POrdset(type)->index;
            else if (type->isRange())
                elemType = PRange(type)->base;
            else
                error("Range not allowed: not a set");
            codegen->implicitCastTo(elemType, "Range boundary type mismatch");
        }
        else
        {
            if (!elemType->isOrdinal())
                error("Ordinal expected as range boundary");
            type = POrdinal(elemType)->deriveRange();
        }
        expression();
        codegen->implicitCastTo(elemType, "Range boundary type mismatch");
        codegen->mkRange();
    }

    else
    {
        if (type != NULL)
        {
            if (!type->isOrdset() && !type->isSet() && !type->isVector())
                error("Invalid container constructor");
            // codegen->implicitCastTo(elemType, "Element type mismatch");
        }
        else
            type = elemType->deriveVector();
    }

    return type;
}


void Compiler::compoundCtor(Type* type)
{
    skip(tokLSquare, "[");
    if (skipIf(tokRSquare))
    {
        codegen->loadNullComp(type);
        return;
    }

    type = compoundCtorElem(type);

    if (type->isRange())
    {
        if (token != tokRSquare)
            error("Set constructor is not allowed when type is undefined");
        next();
        return;
    }
    
    if (type->isDict())
        codegen->pairToDict(PDict(type));
    else if (type->isVector() || type->isArray())
        codegen->elemToVec(PVec(type));
    else
        notimpl();

    while (skipIf(tokComma))
    {
        if (token == tokRSquare)
            break;
        compoundCtorElem(type);
        if (type->isDict())
            codegen->storeContainerElem(false);
        else if (type->isVector() || type->isArray())
            codegen->elemCat();
    }

    skip(tokRSquare, "]");
}


void Compiler::atom()
{
    if (!prevIdent.empty())  // from partial (typeless) definition
    {
        Symbol* s = scope->findDeep(prevIdent);
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
        expression();
        skip(tokRParen, ")");
    }

    else if (token == tokLSquare)
        compoundCtor(NULL);

    else
        errorWithLoc("Expression syntax");
}


void Compiler::designator()
{
    atom();
    while (1)
    {
        if (skipIf(tokPeriod))
        {
            codegen->loadMember(getIdentifier());
            next();
        }
        // TODO: array item selection
        // TODO: static typecast
        // TODO: function call
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
        Type* type = codegen->getTopTypeRefValue();
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


Type* Compiler::constExpr(Type* expectType, variant& result)
{
    ConstCode constCode;
    CodeGen constCodeGen(&constCode);
    CodeGen* prevCodeGen = exchange(codegen, &constCodeGen);

    if (expectType != NULL)
    {
        if (expectType->isTypeRef())
            factor();
        else if (expectType->isCompound() && token == tokLSquare)
            compoundCtor(expectType);
        else
            expression();
    }
    else
        expression();

    Type* valueType = codegen->getTopType();
    if (skipIf(tokRange))
    {
        if (expectType != NULL && !expectType->isTypeRef())
            error("Subrange type is not expected here");
        factor();
        codegen->implicitCastTo(valueType, "Range boundary type mismatch");
        codegen->mkRange();
        constCodeGen.endConstExpr(NULL);
        constCode.run(result);
        range* r = CAST(range*, result.as_obj());
        result = CAST(Ordinal*, valueType)->createSubrange(r->left, r->right);
        valueType = defTypeRef;
    }
    else
    {
        constCodeGen.endConstExpr(expectType);
        constCode.run(result);
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
    expression();
    if (type == NULL)
        type = codegen->getTopType();
    else
        codegen->implicitCastTo(type, "Type mismatch in initialization");
    if (type->isNullComp())
        error("Type undefined (null compound)");
    if (blockScope != NULL)
    {
        Variable* var = blockScope->addLocalVar(type, ident);
        codegen->initLocalVar(var);
    }
    else
    {
        Variable* var = state->addThisVar(type, ident);
        codegen->initThisVar(var);
    }
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
            codegen->echo();
            if (token == tokComma)
            {
                codegen->echoSpace();
                next();
            }
            else
                break;
        }
    }
    codegen->echoLn();
    if (!options.enableEcho)
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
        else if (skipIf(tokEcho))
            echo();
        else if (skipIf(tokAssert))
            assertion();
        else
            notimpl();
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

    try
    {
        next();
        block();
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
        out_text f(NULL, remove_filename_ext(getFileName()) + ".lst");
        mainModule.listing(f);
    }
}


int executeFile(const str& fileName)
{
    Module module(remove_filename_path(remove_filename_ext(fileName)));
    Compiler compiler(fileName, new in_text(NULL, fileName), module);

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
    else if (result.is(variant::STR))
    {
        serr << result.as_str() << endl;
        return 102;
    }
    else
    {
        serr << result << endl;
        return 102;
    }
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

