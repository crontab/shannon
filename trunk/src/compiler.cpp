
#include "vm.h"
#include "compiler.h"


Compiler::Compiler(Context& c, Module& mod, buffifo* f)
    : Parser(f), context(c), module(mod)  { }


Compiler::~Compiler()
    { }


Type* Compiler::getConstValue(Type* expectType, variant& result, bool atomType)
{
    CodeSeg constCode(NULL);
    CodeGen constCodeGen(constCode, state, true);
    CodeGen* prevCodeGen = exchange(codegen, &constCodeGen);
    if (atomType)
        atom(expectType);
    else
        constExpr(expectType);
    if (codegen->getTopType()->isReference())
        error("References not allowed in const expressions");
    Type* resultType = constCodeGen.runConstExpr(expectType, result);
    codegen = prevCodeGen;
    return resultType;
}


Type* Compiler::getTypeValue(bool atomType)
{
    // atomType excludes enums and subrange type definitions
    variant result;
    getConstValue(defTypeRef, result, atomType);
    return state->registerType(cast<Type*>(result._rtobj()));
}


Type* Compiler::getTypeAndIdent(str& ident)
{
    Type* type = NULL;
    if (token == tokIdent)
    {
        ident = strValue;
        if (next() == tokAssign)
            goto ICantBelieveIUsedAGotoStatement;
        // TODO: see if the type specifier is a single ident and parse it immediately here
        undoIdent(ident);
    }
    type = getTypeValue(false);
    ident = getIdentifier();
    next();
    type = getTypeDerivators(type);
ICantBelieveIUsedAGotoStatement:
    expect(tokAssign, "'='");
    return type;
}


void Compiler::definition()
{
    str ident;
    Type* type = getTypeAndIdent(ident);
    variant value;
    Type* valueType = getConstValue(type, value, false);
    if (type == NULL)
        type = valueType;
    if (type->isAnyOrd() && !POrdinal(type)->isInRange(value.as_ord()))
        error("Constant out of range");
    state->addDefinition(ident, type, value, scope);
    skipSep();
}


void Compiler::variable()
{
    // TODO: const variables
    str ident;
    Type* type = getTypeAndIdent(ident);
    expression(type);
    if (type == NULL)
        type = codegen->getTopType();
    if (type->isNullCont())
        error("Type undefined (null container)");
    if (scope->isLocal())
    {
        LocalVar* var = PBlockScope(scope)->addLocalVar(ident, type);
        codegen->initLocalVar(var);
    }
    else
    {
        SelfVar* var = state->addSelfVar(ident, type);
        codegen->initSelfVar(var);
    }
    skipSep();
}


void Compiler::assertion()
{
    assert(token == tokAssert);
    if (context.options.enableAssert)
    {
        integer ln = getLineNum();
        beginRecording();
        next();
        expression(NULL);
        str s = endRecording();
        module.registerString(s);
        if (!context.options.lineNumbers)
            codegen->linenum(ln);
        codegen->assertion(s);
    }
    else
        skipToSep();
    skipSep();
}


void Compiler::dumpVar()
{
    assert(token == tokDump);
    if (context.options.enableDump)
        do
        {
            beginRecording();
            next();
            expression(NULL);
            str s = endRecording();
            module.registerString(s);
            codegen->dumpVar(s);
        }
        while (token == tokComma);
    else
        skipToSep();
    skipSep();
}


void Compiler::programExit()
{
    expression(NULL);
    codegen->programExit();
    skipSep();
}


void Compiler::otherStatement()
{
    // TODO: call, pipe, etc
    memint stkLevel = codegen->getStackLevel();
    designator(NULL);
    if (skipIf(tokAssign))
    {
        str storerCode = codegen->lvalue();
        expression(codegen->getTopType());
        if (!isSep())
            error("Statement syntax");
        codegen->assignment(storerCode);
    }
    if (isSep() && codegen->getStackLevel() != stkLevel)
        error("Unused value in statement");
    skipSep();
}


void Compiler::block()
{
    if (skipBlockBegin())
    {
        BlockScope localScope(scope, codegen);
        scope = &localScope;
        try
        {
            statementList();
            skipBlockEnd();
            localScope.deinitLocals();
            scope = localScope.outer;
        }
        catch (exception&)
        {
            scope = localScope.outer;
            throw;
        }
    }
    else
        singleStatement();
}


void Compiler::singleStatement()
{
    skipEmptyLines();
    if (context.options.lineNumbers)
        codegen->linenum(getLineNum());
    if (skipIf(tokDef))
        definition();
    else if (skipIf(tokVar))
        variable();
    else if (skipIf(tokIf))
        ifBlock();
    else if (skipIf(tokBegin))
        block();
    else if (token == tokAssert)
        assertion();
    else if (token == tokDump)
        dumpVar();
    else if (skipIf(tokExit))
        programExit();
    else
        otherStatement();
    skipEmptyLines();
}


void Compiler::statementList()
{
    while (!isBlockEnd())
        singleStatement();
}


void Compiler::ifBlock()
{
    expression(queenBee->defBool);
    memint fwd = codegen->boolJumpForward(opJumpFalse);
    block();
    if (token == tokElif || token == tokElse)
    {
        memint out = codegen->jumpForward(opJump);
        codegen->resolveJump(fwd);
        fwd = out;
        if (skipIf(tokElif))
            ifBlock();
        else if (skipIf(tokElse))
            block();
    }
    codegen->resolveJump(fwd);
}


void Compiler::compileModule()
{
    // The system module is always added implicitly
    module.addUses(queenBee);
    // Start parsing and code generation
    CodeGen mainCodeGen(*module.getCodeSeg(), &module, false);
    codegen = &mainCodeGen;
    scope = state = &module;
    try
    {
        try
        {
            next();
            statementList();
            expect(tokEof, "End of file");
        }
        catch (EDuplicate& e)
        {
            strValue.clear(); // don't need the " near..." part in error message
            error("'" + e.ident + "' is already defined within this scope");
        }
        catch (EUnknownIdent& e)
        {
            strValue.clear(); // don't need the " near..." part in error message
            error("'" + e.ident + "' is unknown in this context");
        }
    }
    catch (emessage& e)
    {
        str s;
        if (!getFileName().empty())
        {
            s += getFileName() + '(' + to_string(getLineNum()) + ')';
            if (!strValue.empty() || token == tokStrValue)  // may be an empty string literal
                s += " near '" + to_displayable(to_printable(strValue)) + '\'';
            s += ": ";
        }
        s += e.msg;
        error(s);
    }

    mainCodeGen.end();
    module.registerCodeSeg(module.getCodeSeg());
    module.setComplete();
}

