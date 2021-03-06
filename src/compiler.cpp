
#include "vm.h"
#include "compiler.h"


Compiler::AutoScope::AutoScope(Compiler* c) throw()
    : BlockScope(c->scope, c->codegen), compiler(c)
        { compiler->scope = this; }


Compiler::AutoScope::~AutoScope() throw()
        { compiler->scope = outer; }


StkVar* Compiler::AutoScope::addInitStkVar(const str& name, Type* type)
{
    StkVar* var = addStkVar(name, type);
    compiler->codegen->initStkVar(var);
    return var;
}


Compiler::ReturnInfo::ReturnInfo(Compiler& c) throw()
    : compiler(c), prev(c.returnInfo), topLevelReturned(false), jumps()
        { compiler.returnInfo = this; }


Compiler::ReturnInfo::~ReturnInfo() throw()
    { compiler.returnInfo = prev; }


void Compiler::ReturnInfo::resolveJumps()
{
    for (memint i = 0; i < jumps.size(); i++)
        compiler.codegen->resolveJump(jumps[i]);
    jumps.clear();
}


Compiler::LoopInfo::LoopInfo(Compiler& c) throw()
    : compiler(c), prev(c.loopInfo),
      stackLevel(c.codegen->getStackLevel()),
      continueTarget(c.codegen->getCurrentOffs()),
      jumps()
        { compiler.loopInfo = this; }


Compiler::LoopInfo::~LoopInfo() throw()
    { compiler.loopInfo = prev; }


void Compiler::LoopInfo::resolveJumps()
{
    for (memint i = 0; i < jumps.size(); i++)
        compiler.codegen->resolveJump(jumps[i]);
    jumps.clear();
}


Compiler::Compiler(Context& c, Module* mod, buffifo* f)
    : Parser(f), context(c), constStack(c.options.stackSize),
      module(mod), scope(NULL), state(NULL),
      loopInfo(NULL), returnInfo(NULL)  { }


Compiler::~Compiler()
    { }


Type* Compiler::getTypeAndIdent(str* ident)
{
    Type* type = NULL;
    if (token == tokIdent)
    {
        *ident = strValue;
        if (next() == tokAssign || isEos())
            goto ICantBelieveIUsedAGotoStatement;
        undoIdent(*ident);
    }
    type = getTypeValue(/* true */);
    *ident = getIdentifier();
    type = getTypeDerivators(type);
ICantBelieveIUsedAGotoStatement:
    return type;
}


void Compiler::definition()
{
    str ident;
    Type* type = getTypeAndIdent(&ident);
    if (type && type->isState())
        state->addDefinition(ident, defTypeRef, PState(type), scope);
    else
    {
        expect(tokAssign, "'='");
        variant value;
        Type* valueType = getConstValue(type, value);
        if (type == NULL)
            type = valueType;
        if (type->isAnyOrd() && !POrdinal(type)->isInRange(value.as_ord()))
            error("Constant out of range");
        state->addDefinition(ident, type, value, scope);
        skipEos();
    }
}


void Compiler::classDef()
{
    str ident = getIdentifier();
    skipLParen();
    State* type = cast<State*>(getStateDerivator(queenBee->defSelfStub, false));
    state->addDefinition(ident, defTypeRef, type, scope);
}


void Compiler::variable()
{
    str ident;
    Type* type = getTypeAndIdent(&ident);
    if (isEos())
    {
        // Argument reclamation
        if (!isStateScope())
            error("Argument reclamation not allowed here");
        Symbol* sym = state->findShallow(ident);
        if (!sym->isArgVar())
            error("Only function arguments can be reclaimed");
        ArgVar* arg = PArgVar(sym);
        if (type == NULL)
            type = arg->type;
        codegen->loadArgVar(arg);
        InnerVar* var = state->reclaimArg(arg, type);
        codegen->initInnerVar(var);
    }
    else
    {
        expect(tokAssign, "'='");
        expression(type);
        if (type == NULL)
            type = codegen->getTopType();
        if (type->isNullCont())
            error("Type undefined (null container)");
        if (isLocalScope())
        {
            StkVar* var = PBlockScope(scope)->addStkVar(ident, type);
            codegen->initStkVar(var);
        }
        else if (isStateScope())
        {
            InnerVar* var = state->addInnerVar(ident, type);
            codegen->initInnerVar(var);
        }
        else
            notimpl();
    }
    skipEos();
}


void Compiler::statementList()
{
    while (!isBlockEnd() && !(isModuleScope() && eof()))
    {
        singleStatement();
        skipWsSeps();
    }
}


void Compiler::singleOrMultiBlock()
{
    if (skipIf(tokColon))
    {
        skipWsSeps();
        singleStatement();
    }
    else
    {
        skipMultiBlockBegin("':' or '{'");
        statementList();
        skipMultiBlockEnd();
    }
}


void Compiler::nestedBlock()
{
    AutoScope local(this);
    singleOrMultiBlock();
    skipWsSeps();
    local.deinitLocals();
}


void Compiler::singleStatement()
{
    if (skipIf(tokSemi))
        return;

    if (isStateScope() && returnInfo->topLevelReturned)
        error("Statement after 'return'");
    if (context.options.lineNumbers)
        codegen->linenum(getLineNum());

    if (skipIf(tokDef))
        definition();
    else if (skipIf(tokClass))
        classDef();
    else if (skipIf(tokVar))
        variable();
    else if (skipIf(tokBegin))
        nestedBlock();
    else if (skipIf(tokIf))
        ifBlock();
    else if (skipIf(tokSwitch))
        switchBlock();
    else if (skipIf(tokWhile))
        whileBlock();
    else if (skipIf(tokFor))
        forBlock();
    else if (skipIf(tokContinue))
        doContinue();
    else if (skipIf(tokBreak))
        doBreak();
    else if (skipIf(tokReturn))
        doReturn();
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

    codegen->endStatement();
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
        module->registerString(s);
        if (!context.options.lineNumbers)
            codegen->linenum(ln);
        codegen->assertion(ln, s);
    }
    else
        skipToEos();
    skipEos();
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
            module->registerString(s);
            codegen->dumpVar(s);
        }
        while (token == tokComma);
    else
        skipToEos();
    skipEos();
}


void Compiler::programExit()
{
    expression(NULL);
    codegen->programExit();
    skipEos();
}


void Compiler::otherStatement()
{
    // TODO: pipes
    memint stkLevel = codegen->getStackLevel();
    try
    {
        designator(NULL);
    }
    catch (evoidfunc&)
    {
        skipEos();
        return;
    }

    if (skipIf(tokAssign))
    {
        str storerCode = codegen->lvalue();
        expression(codegen->getTopType());
        codegen->assign(storerCode);
    }

    else if (token >= tokAddAssign && token <= tokModAssign)
    {
        str storerCode = codegen->arithmLvalue(token);
        next();
        expression(codegen->getTopType());
        codegen->assign(storerCode);
    }

    else if (skipIf(tokCatAssign))
    {
        codegen->catLvalue();
        expression(NULL);
        codegen->catAssign();
    }

    else if (skipIf(tokPush))
    {
        do
        {
            expression(NULL);
            codegen->fifoPush();
        }
        while (skipIf(tokPush));
        codegen->popValue();
    }

	// TODO: for fifoPull(): store the fifo in a local var so that designators can be
	// parsed and assigned properly

    skipEos();

    if (codegen->getStackLevel() == stkLevel + 1)
    {
        if (codegen->canDiscardValue())
            codegen->popValue();
        else
            error("Unused value in statement");
    }
    assert(codegen->getStackLevel() == stkLevel);
}


void Compiler::doIns()
{
    designator(NULL);
    expect(tokAssign, "'='");
    str inserterCode = codegen->insLvalue();
    expression(NULL);
    codegen->insAssign(inserterCode);
    skipEos();
}


void Compiler::doDel()
{
    designator(NULL);
    codegen->deleteContainerElem();
    skipEos();
}


void Compiler::ifBlock()
{
    expression(queenBee->defBool);
    memint out = codegen->boolJumpForward(opJumpFalse);
    nestedBlock();
    if (token == tokElif || token == tokElse)
    {
        memint t = codegen->jumpForward();
        codegen->resolveJump(out);
        out = t;
        if (skipIf(tokElif))
            ifBlock();
        else if (skipIf(tokElse))
            nestedBlock();
    }
    codegen->resolveJump(out);
}


void Compiler::caseLabel(Type* ctlType)
{
    // Expects the case control variable to be the top stack element
    expect(tokCase, "'case' or 'default'");
    caseValue(ctlType);
    memint out = codegen->boolJumpForward(opJumpFalse);
    nestedBlock();
    if (!isBlockEnd())
    {
        memint t = codegen->jumpForward();
        codegen->resolveJump(out);
        out = t;
        if (skipIf(tokDefault))
            nestedBlock();
        else
            caseLabel(ctlType);
    }
    codegen->resolveJump(out);
}


void Compiler::switchBlock()
{
    AutoScope local(this);
    expression(NULL);
    Type* ctlType = codegen->getTopType();
    local.addInitStkVar("__switch", ctlType);
    skipMultiBlockBegin("'{'");
    caseLabel(ctlType);
    local.deinitLocals();
    skipMultiBlockEnd();
}


void Compiler::whileBlock()
{
    LoopInfo loop(*this);
    expression(queenBee->defBool);
    memint out = codegen->boolJumpForward(opJumpFalse);
    nestedBlock();
    codegen->jump(loop.continueTarget);
    codegen->resolveJump(out);
    loop.resolveJumps();
}


void Compiler::forBlockTail(StkVar* ctlVar, memint outJumpOffs, memint incJumpOffs)
{
    if (incJumpOffs >= 0)
        codegen->resolveJump(incJumpOffs);
    codegen->incStkVar(ctlVar);
    codegen->jump(loopInfo->continueTarget);
    codegen->resolveJump(outJumpOffs);
    loopInfo->resolveJumps();
}


void Compiler::forBlock()
{
    AutoScope local(this);
    str ident = getIdentifier();
    str ident2;
    if (skipIf(tokComma))
        ident2 = getIdentifier();
    expect(tokAssign, "'='");
    expression(NULL);
    Type* iterType = codegen->getTopType();

    // Simple integer range iteration
    if (iterType->isAnyOrd())
    {
        if (!ident2.empty())
            error("Key/value pair is not allowed for range loops");
        StkVar* ctlVar = local.addInitStkVar(ident, iterType);
        {
            LoopInfo loop(*this);
            expect(tokRange, "'..'");
            expression(iterType);
            codegen->stkVarCmp(ctlVar, opGreaterThan);
            memint out = codegen->boolJumpForward(opJumpTrue);
            nestedBlock();
            forBlockTail(ctlVar, out);
        }
    }

    // Vector iterator
    else if (iterType->isAnyVec() || iterType->isNullCont())
    {
        StkVar* vecVar = local.addInitStkVar(LOCAL_ITERATOR_NAME, iterType);
        codegen->loadConst(queenBee->defInt, 0);
        StkVar* ctlVar = local.addInitStkVar(ident, queenBee->defInt);
        {
            LoopInfo loop(*this);
            codegen->stkVarCmpLength(ctlVar, vecVar);
            memint out = codegen->boolJumpForward(opJumpTrue);
            if (!ident2.empty())
            {
                AutoScope inner(this);
                if (iterType->isNullCont())
                {
                    // For a null container we don't know the element type, neither
                    // do we care, because this code below is never executed, however
                    // we want the ident2 variable to exist within the block
                    codegen->loadConst(defVoid, variant());
                    inner.addInitStkVar(ident2, defVoid);
                }
                else
                {
                    // TODO: optimize this?
                    codegen->loadStkVar(vecVar);
                    codegen->loadStkVar(ctlVar);
                    codegen->loadContainerElem();
                    inner.addInitStkVar(ident2, PContainer(iterType)->elem);
                }
                nestedBlock();
                inner.deinitLocals();
            }
            else
                nestedBlock();
            forBlockTail(ctlVar, out);
        }
    }

    // Byte set and byte dict
    else if (iterType->isByteSet() || iterType->isByteDict())
    {
        if (iterType->isByteSet() && !ident2.empty())
            error("Key/value pair is not allowed for set loops");
        Container* contType = PContainer(iterType);
        Ordinal* idxType = POrdinal(contType->index);
        StkVar* contVar = local.addInitStkVar(LOCAL_ITERATOR_NAME, contType);
        codegen->loadConst(idxType, idxType->left);
        StkVar* ctlVar = local.addInitStkVar(ident, idxType);
        {
            LoopInfo loop(*this);
            if (iterType->isByteSet())
            {
                codegen->loadConst(idxType, idxType->right);
                codegen->stkVarCmp(ctlVar, opGreaterThan);
            }
            else
                codegen->stkVarCmpLength(ctlVar, contVar);
            memint out = codegen->boolJumpForward(opJumpTrue);
            // TODO: optimize this?
            codegen->loadStkVar(ctlVar);
            codegen->loadStkVar(contVar);
            codegen->inCont();
            memint inc = codegen->boolJumpForward(opJumpFalse);
            if (!ident2.empty()) // dict only
            {
                AutoScope inner(this);
                // TODO: optimize this?
                codegen->loadStkVar(contVar);
                codegen->loadStkVar(ctlVar);
                codegen->loadContainerElem();
                inner.addInitStkVar(ident2, contType->elem);
                nestedBlock();
                inner.deinitLocals();
            }
            else
                nestedBlock();
            forBlockTail(ctlVar, out, inc);
        }
    }

    // Other sets and dictionaries
    else if (iterType->isAnySet() || iterType->isAnyDict())
    {
        if (iterType->isAnySet() && !ident2.empty())
            error("Key/value pair is not allowed for set loops");
        Container* contType = PContainer(iterType);
        StkVar* contVar = local.addInitStkVar(LOCAL_ITERATOR_NAME, iterType);
        codegen->loadConst(queenBee->defInt, 0);
        StkVar* idxVar = local.addInitStkVar(LOCAL_INDEX_NAME, queenBee->defInt);
        {
            LoopInfo loop(*this);
            codegen->stkVarCmpLength(idxVar, contVar);
            memint out = codegen->boolJumpForward(opJumpTrue);
            {
                AutoScope inner(this);
                codegen->loadStkVar(contVar);
                codegen->loadStkVar(idxVar);
                codegen->loadKeyByIndex();
                inner.addInitStkVar(ident, contType->index);
                if (!ident2.empty()) // dict only
                {
                    codegen->loadStkVar(contVar);
                    codegen->loadStkVar(idxVar);
                    codegen->loadDictElemByIndex();
                    inner.addInitStkVar(ident2, contType->elem);
                }
                nestedBlock();
                inner.deinitLocals();
            }
            forBlockTail(idxVar, out);
        }
    }

    else
        error("Invalid iterator type in 'for' statement");
    local.deinitLocals();
}


void Compiler::doContinue()
{
    if (loopInfo == NULL)
        error("'continue' not within loop");
    codegen->deinitFrame(loopInfo->stackLevel);
    codegen->jump(loopInfo->continueTarget);
    skipEos();
}


void Compiler::doBreak()
{
    if (loopInfo == NULL)
        error("'break' not within loop");
    codegen->deinitFrame(loopInfo->stackLevel);
    loopInfo->jumps.push_back(codegen->jumpForward());
    skipEos();
}


void Compiler::doReturn()
{
    if (!state->isCtor && state->prototype->returns && !isEos())
    {
        expression(state->prototype->returnType);
        codegen->storeResultVar();
    }
    skipEos();
    if (isStateScope())
        // Don't generate a jump at the end of a function body (and make sure
        // there are no statements beyond this point)
        returnInfo->topLevelReturned = true;
    else
    {
#ifdef DEBUG
        codegen->deinitFrame(state->varCount);
#endif
        returnInfo->jumps.push_back(codegen->jumpForward());
    }
}


void Compiler::stateBody(State* newState)
{
    CodeGen newCodeGen(*newState->getCodeSeg(), module, newState, false);
    CodeGen* saveCodeGen = exchange(codegen, &newCodeGen);
    State* saveState = exchange(state, newState);
    Scope* saveScope = exchange(scope, cast<Scope*>(newState));
    try
    {
        ReturnInfo ret(*this);
        codegen->prolog();
        singleOrMultiBlock();
        ret.resolveJumps();
        codegen->epilog();
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
    newState->setComplete();
    module->registerCodeSeg(newState->getCodeSeg());
}


void Compiler::compileModule()
{
    // The system module is always added implicitly
    module->addUsedModule(queenBee);
    // Start parsing and code generation
    scope = state = module;
    CodeGen mainCodeGen(*module->getCodeSeg(), module, state, false);
    codegen = &mainCodeGen;
    loopInfo = NULL;
    try
    {
        try
        {
            ReturnInfo ret(*this);
            codegen->prolog();
            next();
            skipWsSeps();
            statementList();
            expect(tokEof, "End of file");
            ret.resolveJumps();
            codegen->epilog();
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
    module->setComplete();
    module->registerCodeSeg(module->getCodeSeg());
}

