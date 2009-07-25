

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
    Type* designator();
    Type* expression()  { return designator(); }
    Type* expression(Type* hint);

    void parseEcho();
    void parseAssert();
    void parseBlock();
    void parseDefinition();

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


Type* Compiler::designator()
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
    return codegen->getTopType();
}


Type* Compiler::expression(Type* hint)
{
    Type* type = expression();
    return type;
}


// ------------------------------------------------------------------------- //


void Compiler::parseDefinition()
{
    expression();
    codegen->exit();
}


void Compiler::parseEcho()
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


void Compiler::parseAssert()
{
    mem codeOffs = codegen->getCurPos();
    int linenum = parser.getLineNum();
    expression();
    codegen->assertion(fileId, linenum);
    if (!options.enableAssert)
        codegen->discardCode(codeOffs);
    parser.skipSep();
}


void Compiler::parseBlock()
{
    while (!parser.skipIf(tokBlockEnd))
    {
        if (options.linenumInfo)
            codegen->linenum(fileId, parser.getLineNum());

        if (parser.skipIf(tokSep))
            ;
        else if (parser.skipIf(tokDef))
            parseDefinition();
        else if (parser.skipIf(tokEcho))
            parseEcho();
        else if (parser.skipIf(tokAssert))
            parseAssert();
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
        parseBlock();
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

