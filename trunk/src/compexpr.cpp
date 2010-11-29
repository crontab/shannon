
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
            Type* argType = getTypeValue(true);
            str ident;
            if (token == tokIdent)
            {
                ident = getIdentifier();
                argType = getTypeDerivators(argType);
            }
            proto->addFormalArg(ident, argType);
        }
        while (skipIf(tokComma));
        expectRParen();
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
            Type* indexType = getTypeValue(false);
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


void Compiler::builtin(Builtin* b)
{
    b->compileFunc(this, b);
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
                codegen->loadMember(sym);
            }
            return;
        }
    }

    throw EUnknownIdent(ident);
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
    expectLParen();
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
    expectRParen();
}


void Compiler::actualArgs(FuncPtr* proto)
{
    if (!proto->isVoidFunc())
        codegen->loadEmptyConst(proto->returnType);
    for (memint i = 0; i < proto->formalArgs.size(); i++)
    {
        if (i > 0)
            expect(tokComma, "','");
        FormalArg* arg = proto->formalArgs[i];
        expression(arg->type);
    }
    expectRParen();
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
        expression(typeHint);
        expectRParen();
    }

    else if (skipIf(tokLSquare))
        vectorCtor(typeHint);

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
            codegen->loadMember(getIdentifier());
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
            if (!type->isFuncPtr())
                error("Invalid function call");
            actualArgs(PFuncPtr(type));
            codegen->call(PFuncPtr(type));    // May throw evoidfunc()
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
    bool isQ = skipIf(tokQuestion);

    designator(typeHint);

    if (isQ)
        codegen->nonEmpty();
    if (isNeg)
        codegen->arithmUnary(opNeg);
    if (skipIf(tokAs))
    {
        Type* type = getTypeValue(true);
        // TODO: default value in parens?
        codegen->explicitCast(type);
    }
    if (skipIf(tokIs))
        codegen->isType(getTypeValue(true));
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
    if (token == tokIdent)  // Enumeration?
    {
        str ident = strValue;
        if (next() == tokComma)
        {
            result = getEnumeration(ident);
            return defTypeRef;
        }
        undoIdent(ident);
    }

    CodeSeg constCode(NULL);
    CodeGen constCodeGen(constCode, module, state, true);
    CodeGen* prevCodeGen = exchange(codegen, &constCodeGen);
    Type* resultType = NULL;
    try
    {
        // We don't pass defTypeRef as an expected type because we may have a
        // range type expression, in which case expression() below evaluates
        // to an Ordinal value.
        expression(expectType == NULL || expectType->isTypeRef() ? NULL : expectType);

        if (skipIf(tokRange))
        {
            expression(codegen->getTopType());
            codegen->mkRange();
        }

        if (codegen->getTopType()->isReference())
            error("References not allowed in const expressions");

        resultType = constCodeGen.runConstExpr(result);
        codegen = prevCodeGen;

        if (resultType->isRange())
        {
            result = state->registerType(PRange(resultType)->elem->createSubrange(result._range()));
            resultType = defTypeRef;
        }

        if (expectType && expectType->isTypeRef() && !resultType->isTypeRef())
            error("Type reference expected");
    }
    catch(exception&)
    {
        codegen = prevCodeGen;
        throw;
    }
    return resultType;
}


Type* Compiler::getTypeValue(bool atomic)
{
    if (atomic)
    {
        designator(defTypeRef);
        return codegen->undoTypeRef();
    }
    else
    {
        variant result;
        getConstValue(defTypeRef, result);
        return cast<Type*>(result._rtobj());
    }
}

