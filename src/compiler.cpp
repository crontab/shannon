
#include "vm.h"
#include "compiler.h"


evoidfunc::evoidfunc() throw() { }
evoidfunc::~evoidfunc() throw() { }
const char* evoidfunc::what() throw() { return "Void function called"; }


Compiler::AutoScope::AutoScope(Compiler& c)
    : BlockScope(c.scope, c.codegen), compiler(c)
        { compiler.scope = this; }


Compiler::AutoScope::~AutoScope()
        { deinitLocals(); compiler.scope = outer; }


Compiler::LoopInfo::LoopInfo(Compiler& c)
    : compiler(c), prevLoopInfo(c.loopInfo),
      stackLevel(c.codegen->getStackLevel()),
      continueTarget(c.codegen->getCurrentOffs()),
      breakJumps()
        { compiler.loopInfo = this; }


Compiler::LoopInfo::~LoopInfo()
    { compiler.loopInfo = prevLoopInfo; }


void Compiler::LoopInfo::resolveBreakJumps()
{
    for (memint i = 0; i < breakJumps.size(); i++)
        compiler.codegen->resolveJump(breakJumps[i]);
    breakJumps.clear();
}


Compiler::Compiler(Context& c, Module& mod, buffifo* f)
    : Parser(f), context(c), module(mod), scope(NULL),
      state(NULL), loopInfo(NULL)  { }


Compiler::~Compiler()
    { }


Type* Compiler::getTypeAndIdent(str* ident)
{
    Type* type = NULL;
    if (token == tokIdent)
    {
        *ident = strValue;
        if (next() == tokAssign)
            goto ICantBelieveIUsedAGotoStatement;
        undoIdent(*ident);
    }
    type = getTypeValue(false);
    *ident = getIdentifier();
    type = getTypeDerivators(type);
ICantBelieveIUsedAGotoStatement:
    return type;
}


void Compiler::definition()
{
    str ident;
    Type* type = getTypeAndIdent(&ident);
    if (type && type->isAnyState())
        state->addDefinition(ident, defTypeRef, PState(type), scope);
    else
    {
        expect(tokAssign, "'='");
        variant value;
        Type* valueType = getConstValue(type, value, false);
        if (type == NULL)
            type = valueType;
        if (type->isAnyOrd() && !POrdinal(type)->isInRange(value.as_ord()))
            error("Constant out of range");
        state->addDefinition(ident, type, value, scope);
        skipSep();
    }
}


void Compiler::variable()
{
    str ident;
    Type* type = getTypeAndIdent(&ident);
    expect(tokAssign, "'='");
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


void Compiler::block()
{
    skipAnySeps();
    if (skipIf(tokLCurly))
    {
        AutoScope local(*this);
        statementList();
        skipMultiBlockEnd();
    }
    else
        singleStatement();
    skipAnySeps();
}


void Compiler::singleStatement()
{
    if (context.options.lineNumbers)
        codegen->linenum(getLineNum());
    if (skipIf(tokDef))
        definition();
    else if (skipIf(tokVar))
        variable();
    else if (skipIf(tokBegin))
        block();
    else if (skipIf(tokIf))
        ifBlock();
    else if (skipIf(tokCase))
        caseBlock();
    else if (skipIf(tokWhile))
        whileBlock();
    else if (skipIf(tokContinue))
        doContinue();
    else if (skipIf(tokBreak))
        doBreak();
    else if (skipIf(tokDel))
        doDel();
    else if (skipIf(tokIns))
        doIns();
    else if (token == tokAssert)
        assertion();
    else if (token == tokDump)
        dumpVar();
    else if (skipIf(tokExit))
        programExit();
    else
        otherStatement();
}


void Compiler::statementList()
{
    skipAnySeps();
    while (!isBlockEnd())
    {
        singleStatement();
        skipAnySeps();
    }
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
    // TODO: pipes, fifo ops
    memint stkLevel = codegen->getStackLevel();
    try
    {
        designator(NULL);
    }
    catch (evoidfunc&)
    {
        skipSep();
        return;
    }

    if (skipIf(tokAssign))
    {
        str storerCode = codegen->lvalue();
        expression(codegen->getTopType());
        if (!isSep())
            error("Statement syntax");
        codegen->assignment(storerCode);
    }

    skipSep();

    if (codegen->getStackLevel() == stkLevel + 1)
    {
        if (codegen->lastWasFuncCall())
            codegen->popValue();
        else
            error("Unused value in statement");
    }
    assert(codegen->getStackLevel() == stkLevel);
}


void Compiler::ifBlock()
{
    expression(queenBee->defBool);
    memint out = codegen->boolJumpForward(opJumpFalse);
    block();
    if (token == tokElif || token == tokElse)
    {
        memint t = codegen->jumpForward();
        codegen->resolveJump(out);
        out = t;
        if (skipIf(tokElif))
            ifBlock();
        else if (skipIf(tokElse))
            block();
    }
    codegen->resolveJump(out);
}


void Compiler::caseLabel(Type* ctlType)
{
    // Expects the case control variable to be the top stack element
    caseValue(ctlType);
    expect(tokColon, "':'");
    memint out = codegen->boolJumpForward(opJumpFalse);
    block();
    if (!isBlockEnd())
    {
        memint t = codegen->jumpForward();
        codegen->resolveJump(out);
        out = t;
        if (skipIf(tokElse))
            block();
        else
            caseLabel(ctlType);
    }
    codegen->resolveJump(out);
}


void Compiler::caseBlock()
{
    AutoScope local(*this);
    expression(NULL);
    Type* ctlType = codegen->getTopType();
    LocalVar* ctlVar = local.addLocalVar("__case", ctlType);
    codegen->initLocalVar(ctlVar);
    skipMultiBlockBegin();
    caseLabel(ctlType);
    skipMultiBlockEnd();
}


void Compiler::whileBlock()
{
    LoopInfo loop(*this);
    expression(queenBee->defBool);
    memint out = codegen->boolJumpForward(opJumpFalse);
    block();
    codegen->jump(loop.continueTarget);
    codegen->resolveJump(out);
    loop.resolveBreakJumps();
}


void Compiler::doContinue()
{
    if (loopInfo == NULL)
        error("'continue' not within loop");
    codegen->deinitFrame(loopInfo->stackLevel);
    codegen->jump(loopInfo->continueTarget);
}


void Compiler::doBreak()
{
    if (loopInfo == NULL)
        error("'break' not within loop");
    codegen->deinitFrame(loopInfo->stackLevel);
    loopInfo->breakJumps.push_back(codegen->jumpForward());
}


void Compiler::doDel()
{
    designator(NULL);
    codegen->deleteContainerElem();
}


void Compiler::doIns()
{
    designator(NULL);
    expect(tokAssign, "'='");
    str inserterCode = codegen->insLvalue();
    expression(codegen->getTopType());
    if (!isSep())
        error("Statement syntax");
    codegen->assignment(inserterCode);
    skipSep();
}


void Compiler::stateBody(State* newState)
{
    CodeGen newCodeGen(*newState->getCodeSeg(), newState, false);
    CodeGen* saveCodeGen = exchange(codegen, &newCodeGen);
    State* saveState = exchange(state, newState);
    Scope* saveScope = exchange(scope, cast<Scope*>(newState));
    try
    {
        memint prologOffs = codegen->prolog();
        skipMultiBlockBegin();
        statementList();
        skipMultiBlockEnd();
        codegen->epilog(prologOffs);
    }
    catch (exception&)
    {
        scope = saveScope;
        state = saveState;
        codegen = saveCodeGen;
        throw;
    }
    scope = saveScope;
    state = saveState;
    codegen = saveCodeGen;
    newCodeGen.end();
    module.registerCodeSeg(newState->getCodeSeg());
}


void Compiler::compileModule()
{
    // The system module is always added implicitly
    module.addUses(queenBee);
    // Start parsing and code generation
    CodeGen mainCodeGen(*module.getCodeSeg(), &module, false);
    codegen = &mainCodeGen;
    scope = state = &module;
    loopInfo = NULL;
    try
    {
        try
        {
            memint prologOffs = codegen->prolog();
            next();
            statementList();
            expect(tokEof, "End of file");
            codegen->epilog(prologOffs);
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
    catch (exception& e)
    {
        str s;
        if (!getFileName().empty())
        {
            s += getFileName() + '(' + to_string(getLineNum()) + ')';
            if (!strValue.empty() || token == tokStrValue)  // may be an empty string literal
                s += " near '" + to_displayable(to_printable(strValue)) + '\'';
            s += ": ";
        }
        s += e.what();
        error(s);
    }

    mainCodeGen.end();
    module.registerCodeSeg(module.getCodeSeg());
    module.setComplete();
}

