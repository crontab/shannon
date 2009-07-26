

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
    Scope* scope;

    void error(const str& msg)              { parser.error(msg); }
    void error(const char* msg)             { parser.error(msg); }
    void errorWithLoc(const str& msg)       { parser.errorWithLoc(msg); }
    void errorWithLoc(const char* msg)      { parser.errorWithLoc(msg); }

    Symbol* getQualifiedName();
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
    void expression()  { orLevel(); }

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
    codegen(NULL), scope(NULL)  { }

Compiler::~Compiler()  { }


// --- EXPRESSION ---------------------------------------------------------- //

/*
    <nested-expr>, <typecast>, <ident>, <number>, <string>, <char>,
        true, false, null, compound-ctor
    <array-sel>, <function-call>, <mute>
    -
    *, /, mod, as
    +, –
    |
    ==, <>, != <, >, <=, >=, in, is
    not
    and
    or, xor
    ..
*/


void Compiler::atom()
{
    if (parser.skipIf(tokLParen))
    {
        expression();
        parser.skip(tokRParen, ")");
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
    // TODO: if function
    // TODO: container ctor
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


// ------------------------------------------------------------------------- //


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
    int linenum = parser.getLineNum();
    expression();
    codegen->assertion(fileId, linenum);
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
    scope = &mainModule;

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
        parser.error("'" + e.ident + "' is unknown");
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
    queenBee->dumpContents(sio);
    module.dumpContents(sio);
#endif

    if (result.is_null())
        return 0;
    else if (result.is_ordinal())
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

