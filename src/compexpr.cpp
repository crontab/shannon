
#include "vm.h"
#include "compiler.h"


// --- EXPRESSION ---------------------------------------------------------- //

/*
    <nested-expr>  <ident>  <number>  <string>  <char>  <type-spec>
        <vec-ctor>  <dict-ctor>  <if-func>  <typeof>
    @ <array-sel>  <member-sel>  <function-call>
    unary-  ?  #  as  is
    |
    *  /  mod
    +  –
    ==  <>  <  >  <=  >=  in
    not
    and
    or  xor
    range, enum
*/


Type* Compiler::getEnumeration(const str& firstIdent)
{
    Enumeration* enumType = state->registerType(new Enumeration());
    enumType->addValue(state, scope, firstIdent);
    expect(tokComma, "','");
    do
    {
        enumType->addValue(state, scope, getIdentifier());
    }
    while (skipIf(tokComma));
    return enumType;
}


Type* Compiler::getStateDerivator(Type* retType, bool allowProto)
{
    FuncPtr* proto = state->registerType(new FuncPtr(retType));
    if (!skipIf(tokRParen))
    {
        do
        {
            bool isPtr = skipIf(tokVar);
            Type* argType = getTypeValue(/* true */);
            str ident;
            if (token == tokIdent)
            {
                ident = getIdentifier();
                argType = getTypeDerivators(argType);
            }
            if (skipIf(tokAssign))
            {
                if (isPtr)
                    error("Var arguments can not have a default value");
                variant defValue;
                getConstValue(argType, defValue);
                proto->addFormalArg(ident, argType, false, &defValue);
            }
            else
                proto->addFormalArg(ident, argType, isPtr, NULL);
        }
        while (skipIf(tokComma));
        skipRParen();
    }
    if (skipIf(tokEllipsis))
    {
        if (!allowProto)
            error("Function pointer type not allowed here");
        return proto;
    }
    else
    {
        State* newState = state->registerType(new State(state, proto));
        stateBody(newState);
        return newState;
    }
}


Type* Compiler::getTypeDerivators(Type* type)
{
    // TODO: anonymous functions are static, named ones are not
    if (skipIf(tokLSquare))  // container
    {
        if (skipIf(tokRSquare))
            return getTypeDerivators(type)->deriveVec(state);

        else if (skipIf(tokRange))
        {
            if (!type->isAnyOrd())
                error("Range can be derived from ordinal type only");
            expect(tokRSquare, "']'");
            return POrdinal(type)->getRangeType();
        }

        else
        {
            Type* indexType = getTypeValue(/* false */);
            expect(tokRSquare, "']'");
            if (indexType->isVoid())
                return getTypeDerivators(type)->deriveVec(state);
            else
                return getTypeDerivators(type)->deriveContainer(state, indexType);
        }
    }

    else if (skipIf(tokLessThan))  // fifo
    {
        expect(tokGreaterThan, "'>'");
        return getTypeDerivators(type)->deriveFifo(state);
    }

    else if (skipIf(tokLParen))  // prototype/function
        return getStateDerivator(type, true);

    else if (skipIf(tokCaret)) // ^
    {
        type = getTypeDerivators(type);
        if (type->isReference())
            error("Double reference");
        if (!type->isDerefable())
            error("Reference can not be derived from this type");
        return type->getRefType();
    }

    return type;
}


void Compiler::builtin(Builtin* b, bool skipFirst)
{
    if (b->prototype)
    {
        skipLParen();
        actualArgs(b->prototype, skipFirst);
    }
    if (b->compile)
        b->compile(this, b);
    else
        codegen->staticCall(b->staticFunc);
}


void Compiler::identifier(str ident)
{
    // Go up the current scope hierarchy within the module. Currently not
    // everything is accessible even if found by the code below: an error
    // will be thrown by the CodeGen in case a symbol can not be accessed.
    if (token == tokPrevIdent)
        redoIdent();
    else
        next();
    Scope* sc = scope;
    do
    {
        // TODO: implement loading from outer scopes
        Symbol* sym = sc->find(ident);
        if (sym)
        {
            if (sym->isBuiltin())
                builtin(PBuiltin(sym));
            else
                codegen->loadSymbol(sym);
            return;
        }
        sc = sc->outer;
    }
    while (sc != NULL);

    // Look up in used modules; search backwards
    for (memint i = module->usedModuleVars.size(); i--; )
    {
        InnerVar* m = module->usedModuleVars[i];
        Symbol* sym = m->getModuleType()->find(ident);
        if (sym)
        {
            if (sym->isBuiltin())
                builtin(PBuiltin(sym));
            else if (codegen->isCompileTime())
                codegen->loadSymbol(sym);
            else
            {
                codegen->loadVariable(m);
                codegen->loadMember(sym->host, sym);
            }
            return;
        }
    }

    throw EUnknownIdent(ident);
}


void Compiler::dotIdentifier(str ident)
{
    Type* type = codegen->getTopType();
    Builtin* b = queenBee->findBuiltin(ident);
    if (b)
    {
        // See if the first formal arg of the builtin matches the current top stack item
        if (b->prototype->formalArgs.size() == 0)
            error("Invalid builtin call");
        Type* firstArgType = b->prototype->formalArgs[0]->type;
        if (firstArgType)
            codegen->implicitCast(firstArgType, "Builtin not applicable to this type");
        builtin(b, true);
    }
    else if (type->isFuncPtr())
    {
        // Scope resolution: we have a state name followed by '.', but because
        // state names are by default transformed into function pointers, we 
        // need to roll it back
        codegen->implicitCast(defTypeRef, "Invalid member selection");
        codegen->loadSymbol(codegen->undoStateRef()->findShallow(ident));
    }
    else if (type->isAnyState())
        // State object (variable or subexpr) on the stack followed by '.' and member:
        codegen->loadMember(PState(type), PState(type)->findShallow(ident));
    else
        error("Invalid member selection");
}


void Compiler::vectorCtor(Type* typeHint)
{
    // if (typeHint && !typeHint->isAnyVec())
    //     error("Vector constructor not expected here");

    Type* elemType = NULL;
    if (typeHint)
    {
        if (typeHint->isAnyCont())
            elemType = PContainer(typeHint)->elem;
        else if (typeHint->isRange())
            elemType = PRange(typeHint)->elem;
    }

    if (skipIf(tokRSquare))
    {
        // Since typeHint can be anything, empty vector [] can actually be an
        // empty constant for any type:
        codegen->loadEmptyConst(typeHint ? typeHint : queenBee->defNullCont);
        return;
    }

    expression(elemType);

    if (skipIf(tokRange))
    {
        expression(elemType);
        codegen->mkRange();
    }

    else
    {
        Container* contType = codegen->elemToVec(
            typeHint && typeHint->isAnyCont() ? PContainer(typeHint) : NULL);
        while (skipIf(tokComma))
        {
            expression(contType->elem);
            codegen->elemCat();
        }
    }
    expect(tokRSquare, "]");
}


void Compiler::fifoCtor(Type* typeHint)
{
    if (typeHint && !typeHint->isAnyFifo())
        error("Fifo constructor not expected here");
    Fifo* fifoType = PFifo(typeHint);
    if (skipIf(tokRAngle))
    {
        if (fifoType == NULL)
            codegen->loadEmptyConst(queenBee->defNullCont);
        else
            codegen->loadFifo(fifoType);
        return;
    }
    expression(fifoType ? fifoType->elem : NULL);
    fifoType = codegen->elemToFifo();
    while (skipIf(tokComma))
    {
        expression(fifoType->elem);
        codegen->fifoEnq();
    }
    expect(tokRAngle, "'>'");
}


void Compiler::dictCtor(Type* typeHint)
{
    if (skipIf(tokRCurly))
    {
        codegen->loadEmptyConst(typeHint ? typeHint : queenBee->defNullCont);
        return;
    }

    if (typeHint && !typeHint->isAnySet() && !typeHint->isAnyDict())
        error("Set/dict constructor not expected here");
    Container* type = PContainer(typeHint);

    expression(type ? type->index : NULL);

    // Dictionary
    if (skipIf(tokAssign))
    {
        expression(type ? type->elem : NULL);
        type = codegen->pairToDict();
        while (skipIf(tokComma))
        {
            expression(type->index);
            codegen->checkDictKey();
            expect(tokAssign, "=");
            expression(type->elem);
            codegen->dictAddPair();
        }
    }

    // Set
    else
    {
        if (skipIf(tokRange))
        {
            expression(type ? type->index : NULL);
            type = codegen->rangeToSet();
        }
        else
            type = codegen->elemToSet();
        while (skipIf(tokComma))
        {
            expression(type->index);
            if (skipIf(tokRange))
            {
                codegen->checkRangeLeft();
                expression(type->index);
                codegen->setAddRange();
            }
            else
                codegen->setAddElem();
        }
    }

    expect(tokRCurly, "}");
}


void Compiler::typeOf()
{
    designator(NULL);
    Type* type = codegen->getTopType();
    codegen->undoSubexpr();
    codegen->loadTypeRefConst(type);
}


void Compiler::ifFunc()
{
    skipLParen();
    expression(queenBee->defBool);
    memint jumpFalse = codegen->boolJumpForward(opJumpFalse);
    expect(tokComma, "','");
    expression(NULL);
    Type* exprType = codegen->getTopType();
    codegen->justForget(); // will get the expression type from the second branch
    memint jumpOut = codegen->jumpForward();
    codegen->resolveJump(jumpFalse);
    expect(tokComma, "','");
    expression(exprType);
    codegen->resolveJump(jumpOut);
    skipRParen();
}


void Compiler::actualArgs(FuncPtr* proto, bool skipFirst)
{
    // skipFirst is for member-style builtin calls
    memint i = int(skipFirst);
    if (token != tokRParen)
    {
        do
        {
            if (i >= proto->formalArgs.size())
                error("Too many arguments");
            FormalArg* arg = proto->formalArgs[i];
            if (arg->hasDefValue && (token == tokComma || token == tokRParen))
                codegen->loadConst(arg->type, arg->defValue);
            else
            {
                // TODO: improve 'type mismatch' error message; note however that
                // it's important to pass the arg type to expression()
                expression(arg->type);
                if (arg->isPtr)
                    codegen->toLea();
            }
            i++;
        }
        while (skipIf(tokComma));
    }
    skipRParen();
    while (i < proto->formalArgs.size())
    {
        FormalArg* arg = proto->formalArgs[i];
        if (!arg->hasDefValue)
            error("Too few arguments");
        codegen->loadConst(arg->type, arg->defValue);
        i++;
    }
}


void Compiler::atom(Type* typeHint)
{
    if (token == tokPrevIdent)  // from partial (typeless) definition
        identifier(getPrevIdent());

    else if (token == tokIntValue)
    {
        codegen->loadConst(queenBee->defInt, integer(intValue));
        next();
    }

    else if (token == tokStrValue)
    {
        str value = strValue;
        if (value.size() == 1)
            codegen->loadConst(queenBee->defChar, value[0]);
        else
        {
            module->registerString(value);
            codegen->loadConst(queenBee->defStr, value);
        }
        next();
    }

    else if (token == tokIdent)
        identifier(strValue);

    else if (skipIf(tokLParen))
    {
        if (codegen->isCompileTime() && token == tokIdent)  // Enumeration?
        {
            str ident = strValue;
            if (next() == tokComma)
            {
                codegen->loadTypeRef(getEnumeration(ident));
                goto skipExpr;
            }
            undoIdent(ident);
        }
        expression(typeHint);
skipExpr:
        skipRParen();
    }

    else if (skipIf(tokLSquare))
        vectorCtor(typeHint);

    else if (skipIf(tokLAngle))
        fifoCtor(typeHint);

    else if (skipIf(tokLCurly))
        dictCtor(typeHint);

    else if (skipIf(tokIf))
        ifFunc();

    else if (skipIf(tokTypeOf))
        typeOf();

    else if (skipIf(tokThis))
        codegen->loadThis();

    else
        error("Expression syntax");

    while (token == tokWildcard && codegen->tryImplicitCast(defTypeRef))
    {
        next(); // *
        codegen->loadTypeRef(getTypeDerivators(codegen->undoTypeRef()));
    }
}


void Compiler::designator(Type* typeHint)
{
    bool isAt = skipIf(tokAt);
    Type* refTypeHint = typeHint && typeHint->isReference() ? PReference(typeHint)->to : NULL;

    atom(refTypeHint ? refTypeHint : typeHint);

    while (1)
    {
        if (skipIf(tokPeriod))
        {
            codegen->deref();
            dotIdentifier(getIdentifier());
        }

        else if (skipIf(tokLSquare))
        {
            codegen->deref();
            expression(NULL);
            if (skipIf(tokRange))
            {
                if (token == tokRSquare)
                    codegen->loadConst(defVoid, variant());
                else
                    expression(codegen->getTopType());
                codegen->loadSubvec();
            }
            else
                codegen->loadContainerElem();
            expect(tokRSquare, "]");
        }

        else if (skipIf(tokLParen))
        {
            Type* type = codegen->getTopType();
            if (type->isFuncPtr())
            {
                actualArgs(PFuncPtr(type));
                codegen->call(PFuncPtr(type));    // May throw evoidfunc()
            }
            else
                error("Invalid function call");
        }

        else
            break;
    }

    if (isAt || refTypeHint)
        codegen->mkref();
    else
        codegen->deref();
}


void Compiler::factor(Type* typeHint)
{
    bool isNeg = skipIf(tokMinus);

    designator(typeHint);

    if (skipIf(tokQuestion))
        codegen->nonEmpty();
    if (isNeg)
        codegen->arithmUnary(opNeg);
    if (skipIf(tokAs))
    {
        Type* type = getTypeValue(/* true */);
        // TODO: default value in parens?
        codegen->explicitCast(type);
    }
    if (skipIf(tokIs))
        codegen->isType(getTypeValue(/* true */));
}


void Compiler::concatExpr(Container* contType)
{
    factor(contType);
    if (skipIf(tokCat))
    {
        Type* top = codegen->getTopType();
        if (top->isAnyVec())
            if (contType)
                codegen->implicitCast(contType);
            else
                contType = PContainer(top);
        else
            contType = codegen->elemToVec(contType);
        do
        {
            factor(contType);
            if (codegen->tryImplicitCast(contType))
                codegen->cat();
            else
                codegen->elemCat();
        }
        while (skipIf(tokCat));
    }
}


void Compiler::term()
{
    concatExpr(NULL);
    while (token == tokMul || token == tokDiv || token == tokMod)
    {
        OpCode op = token == tokMul ? opMul
            : token == tokDiv ? opDiv : opMod;
        next();
        factor(NULL);
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


void Compiler::relation()
{
    arithmExpr();
    if (skipIf(tokIn))
    {
        arithmExpr();
        Type* right = codegen->getTopType();
        if (right->isTypeRef())
            codegen->inBounds();
        else if (right->isAnyCont())
            codegen->inCont();
        else if (right->isRange())
            codegen->inRange();
        else if (right->isAnyOrd() && skipIf(tokRange))
        {
            arithmExpr();
            codegen->inRange2();
        }
        else
            error("Operator 'in' expects container, numeric range, or ordinal type ref");
    }
    else if (token >= tokEqual && token <= tokGreaterEq)
    {
        OpCode op = OpCode(opEqual + int(token - tokEqual));
        next();
        arithmExpr();
        codegen->cmp(op);
    }
}


void Compiler::notLevel()
{
    bool isNot = skipIf(tokNot);
    relation();
    if (isNot)
        codegen->_not();
}


void Compiler::andLevel()
{
    notLevel();
    while (token == tokShl || token == tokShr || token == tokAnd)
    {
        Type* type = codegen->getTopType();
        if (type->isBool() && skipIf(tokAnd))
        {
            memint offs = codegen->boolJumpForward(opJumpAnd);
            andLevel();
            codegen->resolveJump(offs);
            break;
        }
        else // if (type->isInt())
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
    while (token == tokOr || token == tokXor)
    {
        Type* type = codegen->getTopType();
        // TODO: boolean XOR? Beautiful thing, but not absolutely necessary
        if (type->isBool() && skipIf(tokOr))
        {
            memint offs = codegen->boolJumpForward(opJumpOr);
            orLevel();
            codegen->resolveJump(offs);
            break;
        }
        else // if (type->isInt())
        {
            OpCode op = token == tokOr ? opBitOr : opBitXor;
            next();
            andLevel();
            codegen->arithmBinary(op);
        }
    }
}


void Compiler::caseValue(Type* ctlType)
{
    expression(ctlType);
    if (skipIf(tokRange))
    {
        expression(ctlType);
        codegen->caseInRange();
    }
    else
        codegen->caseCmp();
    if (skipIf(tokComma))
    {
        memint offs = codegen->boolJumpForward(opJumpOr);
        caseValue(ctlType);
        codegen->resolveJump(offs);
    }
}


void Compiler::expression(Type* expectType)
{
    // Some tricks to shorten the path whenever possible:
    if (expectType == NULL || expectType->isBool())
        orLevel();
    else if (expectType->isAnyCont())
        // expectType will propagate all the way down to vectorCtor()/dictCtor():
        concatExpr(PContainer(expectType));
    else if (expectType->isReference() || expectType->isAnyState())
        designator(expectType);
    else
        arithmExpr();
    if (expectType)
        codegen->implicitCast(expectType);
}


Type* Compiler::getConstValue(Type* expectType, variant& result)
{
    CodeSeg constCode(NULL);
    CodeGen constCodeGen(constCode, module, state, true);
    CodeGen* prevCodeGen = exchange(codegen, &constCodeGen);
    Type* resultType = NULL;
    try
    {
        // We don't pass expectType here because we may have a subrange type 
        // expression, in which case expression() below evaluates to an Ordinal 
        expression(expectType == NULL || expectType->isTypeRef() ? NULL : expectType);

        if (skipIf(tokRange))
        {
            expression(codegen->getTopType());
            codegen->mkRange();
            resultType = constCodeGen.runConstExpr(constStack, result);
            if (expectType && !expectType->isTypeRef())
                error("Subrange type not expected here");
            result = state->registerType(PRange(resultType)->elem->createSubrange(result._range()));
            resultType = defTypeRef;
        }
        else
        {
            if (expectType && expectType->isTypeRef())
                codegen->implicitCast(defTypeRef, "Type mismatch in const expression");
            else if (codegen->getTopType()->isFuncPtr())
                codegen->implicitCast(defTypeRef, "Invalid use of function in const expression");
            resultType = constCodeGen.runConstExpr(constStack, result);
        }
    }
    catch(exception&)
    {
        codegen = prevCodeGen;
        throw;
    }

    codegen = prevCodeGen;
    return resultType;
}


Type* Compiler::getTypeValue()
{
    variant result;
    getConstValue(defTypeRef, result);
    return cast<Type*>(result._rtobj());
}

