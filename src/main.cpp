

#include "common.h"
#include "runtime.h"
#include "source.h"
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


class Compiler: noncopyable
{
protected:
    Parser& parser;
    Module& mainModule;
    CompilerOptions options;
    mem fileId;
    bool started;

    CodeGen* codegen;
    Scope* scope;           // for storing definitions
    BlockScope* blockScope; // for local vars in nested blocks
    State* state;           // for this-vars

    void error(const str& msg)              { parser.error(msg); }
    void error(const char* msg)             { parser.error(msg); }
    void errorWithLoc(const str& msg)       { parser.errorWithLoc(msg); }
    void errorWithLoc(const char* msg)      { parser.errorWithLoc(msg); }

    Type* getTypeDerivators(Type*);
    void compoundCtorElem(Type*&);
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
    Compiler(Parser&, Module&);
    ~Compiler();

    void compile();
};


Compiler::Compiler(Parser& _parser, Module& _main)
  : parser(_parser), mainModule(_main), started(false),
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
    if (parser.skipIf(tokLSquare))
    {
        if (parser.skipIf(tokRSquare))
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
            parser.skip(tokRSquare, "]");
        }
        return getTypeDerivators(type);
    }

    else if (parser.skipIf(tokNotEq)) // <>
        return getTypeDerivators(type->deriveFifo());

    else if (parser.skipIf(tokLAngle))
    {
        parser.skip(tokRAngle, ">");
        return getTypeDerivators(type->deriveFifo());
    }

    // TODO: function derivator
    return type;
}


void Compiler::compoundCtorElem(Type*& type)
{
    expression();
    Type* elemType = codegen->getTopType();

    if (parser.skipIf(tokEqual))
    {
        if (type != NULL)
        {
            if (type->isDict())
                codegen->implicitCastTo(PDict(type)->index, "Dictionary key type mismatch");
            else
                error("Key/value pair not allowed here");
        }
        expression();
        if (type != NULL)
            codegen->implicitCastTo(PContainer(type)->elem, "Dictionary element type mismatch");
        else
        {
            Type* keyType = elemType;
            elemType = codegen->getTopType();
            type = elemType->createContainer(keyType);
        }
    }

    else if (parser.skipIf(tokRange))
    {
        if (type != NULL)
        {
            if (type->isOrdset())
                elemType = POrdset(type)->index;
            else if (type->isSet())
                elemType = PSet(type)->index;
            else if (type->isRange())
                elemType = PRange(type)->base;
            else
                error("Range not allowed here");
            codegen->implicitCastTo(elemType, "Range boundary type mismatch");
        }
        expression();
        codegen->implicitCastTo(elemType, "Range boundary type mismatch");
        codegen->mkRange();
    }

    else
    {
        if (type != NULL)
        {
            if (type->isOrdset())
                elemType = POrdset(type)->index;
            else if (type->isSet())
                elemType = PSet(type)->index;
            else if (type->isVector())
                elemType = PVec(type)->elem;
            else
                error("Invalid container constructor");
            codegen->implicitCastTo(elemType, "Element type mismatch");
        }
        else
            type = queenBee->defVariant->deriveVector();
    }
}


void Compiler::compoundCtor(Type* type)
{
    parser.skip(tokLSquare, "[");
    if (parser.skipIf(tokRSquare))
        codegen->loadNullContOrRange(type);
    else
    {
        // TODO: opPairToDict
        // TODO: make sure null container can't be initialized
        notimpl();
    }
}


void Compiler::atom()
{
    if (!parser.prevIdent.empty())  // from partial (typeless) definition
    {
        Symbol* s = scope->findDeep(parser.prevIdent);
        codegen->loadSymbol(s);
        parser.redoIdent();
    }

    else if (parser.token == tokIntValue)
    {
        codegen->loadInt(parser.intValue);
        parser.next();
    }

    else if (parser.token == tokStrValue)
    {
        str value = parser.strValue;
        if (value.size() == 1)
            codegen->loadChar(value[0]);
        else
            codegen->loadStr(value);
        parser.next();
    }

    else if (parser.token == tokIdent)
    {
        Symbol* s = scope->findDeep(parser.strValue);
        codegen->loadSymbol(s);
        parser.next();
    }

    else if (parser.skipIf(tokLParen))
    {
        expression();
        parser.skip(tokRParen, ")");
    }

    else if (parser.token == tokLSquare)
        compoundCtor(NULL);

    else
        errorWithLoc("Expression syntax");
}


void Compiler::designator()
{
    atom();
    while (1)
    {
        if (parser.skipIf(tokPeriod))
        {
            codegen->loadMember(parser.getIdentifier());
            parser.next();
        }
        // TODO: array item selection
        // TODO: function call
        else
            break;
    }
}


void Compiler::factor()
{
    bool isNeg = parser.skipIf(tokMinus);
    designator();
    if (isNeg)
        codegen->arithmUnary(opNeg);
    else if (parser.token == tokWildcard)
    {
        // anonymous type spec
        Type* type = codegen->getTopTypeRefValue();
        if (type != NULL)
        {
            parser.next();
            codegen->loadTypeRef(getTypeDerivators(type));
        }
    }
}


void Compiler::term()
{
    factor();
    while (parser.token == tokMul || parser.token == tokDiv || parser.token == tokMod)
    {
        OpCode op = parser.token == tokMul ? opMul
                : parser.token == tokDiv ? opDiv : opMod;
        parser.next();
        factor();
        codegen->arithmBinary(op);
    }
}


void Compiler::arithmExpr()
{
    term();
    while (parser.token == tokPlus || parser.token == tokMinus)
    {
        OpCode op = parser.token == tokPlus ? opAdd : opSub;
        parser.next();
        term();
        codegen->arithmBinary(op);
    }
}


void Compiler::simpleExpr()
{
    arithmExpr();
    if (parser.skipIf(tokCat))
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
        while (parser.skipIf(tokCat));
    }
}


void Compiler::relation()
{
    simpleExpr();
    if (parser.token >= tokCmpFirst && parser.token <= tokCmpLast)
    {
        OpCode op = OpCode(opCmpFirst + int(parser.token - tokCmpFirst));
        parser.next();
        simpleExpr();
        codegen->cmp(op);
    }
}


void Compiler::notLevel()
{
    bool isNot = parser.skipIf(tokNot);
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
        if (parser.skipIf(tokAnd))
        {
            mem offs = codegen->boolJumpForward(opJumpAnd);
            andLevel();
            codegen->resolveJump(offs);
        }
    }
    else if (type->isInt())
    {
        while (parser.token == tokShl || parser.token == tokShr || parser.token == tokAnd)
        {
            OpCode op = parser.token == tokShl ? opBitShl
                    : parser.token == tokShr ? opBitShr : opBitAnd;
            parser.next();
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
        if (parser.skipIf(tokOr))
        {
            mem offs = codegen->boolJumpForward(opJumpOr);
            orLevel();
            codegen->resolveJump(offs);
        }
        else if (parser.skipIf(tokXor))
        {
            orLevel();
            codegen->boolXor();
        }
    }
    else if (type->isInt())
    {
        while (parser.token == tokOr || parser.token == tokXor)
        {
            OpCode op = parser.token == tokOr ? opBitOr : opBitXor;
            parser.next();
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

    if (expectType != NULL && expectType->isContainer() && parser.token == tokLSquare)
        compoundCtor(PContainer(expectType));
    else
        expression();

    Type* valueType = codegen->getTopType();
    if (parser.skipIf(tokRange))
    {
        if (expectType != NULL && !expectType->isTypeRef())
            error("Subrange type is not expected here");
        expression();
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
    // TODO: shorter path?
    Type* resultType = constExpr(defTypeRef, result);
    if (!resultType->isTypeRef())
        error("Type expression expected");
    return CAST(Type*, result._obj());
}


// ------------------------------------------------------------------------- //


Type* Compiler::getTypeAndIdent(str& ident)
{
    Type* type = NULL;
    if (parser.token == tokIdent)
    {
        ident = parser.strValue;
        if (parser.next() != tokAssign)
        {
            parser.undoIdent(ident);
            type = typeExpr();
            ident = parser.getIdentifier();
            parser.next();
        }
    }
    else
        type = typeExpr();
    if (type != NULL)
        type = getTypeDerivators(type);
    if (parser.token != tokAssign)
        error("Initialization is mandatory");
    parser.next();
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
    {
        if (valueType->isNullCont())
            error("Undefined type (empty container)");
        type = valueType;
    }
    if (type->isTypeRef())
        scope->addTypeAlias(ident, CAST(Type*, value._obj()));
    else
        scope->addConstant(type, ident, value);
    parser.skipSep();
}


void Compiler::variable()
{
    // definition ::= 'var' [ type-expr ] ident { type-derivator } '=' expr
    str ident;
    Type* type = getTypeAndIdent(ident);
    expression();
    if (type == NULL)
    {
        type = codegen->getTopType();
        if (type->isNullCont())
            error("Undefined type (empty container)");
    }
    else
        codegen->implicitCastTo(type, "Type mismatch in initialization");
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
    parser.skipSep();
}


void Compiler::echo()
{
    mem codeOffs = codegen->getCurPos();
    if (parser.token != tokSep)
    {
        while (1)
        {
            expression();
            codegen->echo();
            if (parser.token == tokComma)
            {
                codegen->echoSpace();
                parser.next();
            }
            else
                break;
        }
    }
    codegen->echoLn();
    if (!options.enableEcho)
        codegen->discardCode(codeOffs);
    parser.skipSep();
}


void Compiler::assertion()
{
    mem codeOffs = codegen->getCurPos();
    expression();
    if (!options.linenumInfo)
        codegen->linenum(fileId, parser.getLineNum());
    codegen->assertion();
    if (!options.enableAssert)
        codegen->discardCode(codeOffs);
    parser.skipSep();
}


void Compiler::block()
{
    while (!parser.skipIf(tokBlockEnd))
    {
        if (options.linenumInfo)
            codegen->linenum(fileId, parser.getLineNum());

        if (parser.skipIf(tokSep))
            ;
        else if (parser.skipIf(tokDef))
            definition();
        else if (parser.skipIf(tokVar))
            variable();
        else if (parser.skipIf(tokEcho))
            echo();
        else if (parser.skipIf(tokAssert))
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

    fileId = mainModule.registerFileName(parser.getFileName());
    CodeGen mainCodeGen(&mainModule);
    codegen = &mainCodeGen;
    scope = state = &mainModule;

    try
    {
        parser.next();
        block();
        parser.skip(tokEof, "<EOF>");
    }
    catch (EDuplicate& e)
    {
        parser.error("'" + e.ident + "' is already defined within this scope");
    }
    catch (EUnknownIdent& e)
    {
        parser.error("'" + e.ident + "' is unknown in this context");
    }
    catch (EParser&)
    {
        throw;  // comes with file name and line no. already
    }
    catch (exception& e)
    {
        parser.error(e.what());
    }

    mainCodeGen.end();

    if (options.vmListing)
    {
        out_text f(NULL, remove_filename_ext(parser.getFileName()) + ".lst");
        mainModule.listing(f);
    }
}


int executeFile(const str& fileName)
{
    Parser parser(fileName, new in_text(NULL, fileName));
    str moduleName = remove_filename_path(remove_filename_ext(fileName));
    Module module(moduleName);
    Compiler compiler(parser, module);

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

