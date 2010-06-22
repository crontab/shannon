
#include "vm.h"
#include "compiler.h"


Compiler::Compiler(Context& c, Module& mod, buffifo* f)
    : Parser(f), context(c), module(mod)  { }


Compiler::~Compiler()
    { }


void Compiler::enumeration(const str& firstIdent)
{
    Enumeration* enumType = state->registerType(new Enumeration());
    enumType->addValue(state, firstIdent);
    expect(tokComma, ",");
    do
    {
        enumType->addValue(state, getIdentifier());
        next();
    }
    while (skipIf(tokComma));
    codegen->loadTypeRef(enumType);
}


Type* Compiler::getTypeDerivators(Type* type)
{
    // TODO: do this in opposite order
    if (skipIf(tokLSquare))
    {
        if (skipIf(tokRSquare))
            return getTypeDerivators(type)->deriveVec(state);
        else if (skipIf(tokRange))
        {
            expect(tokRSquare, "]");
            return getTypeDerivators(type)->deriveSet(state);
        }
        else
        {
            Type* indexType = getTypeValue();
            expect(tokRSquare, "]");
            if (indexType->isVoid())
                return getTypeDerivators(type)->deriveVec(state);
            else
                return getTypeDerivators(type)->deriveContainer(state, indexType);
        }
    }

    else if (skipIf(tokNotEq)) // <>
        return getTypeDerivators(type)->deriveFifo(state);

    // TODO: function derivator
    // else if (token == tokWildcard)

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


void Compiler::identifier(const str& ident)
{
    Symbol* sym;

    // Go up the current scope hierarchy within the module
    Scope* sc = scope;
    do
    {
        sym = sc->find(ident);
        if (sym)
            break;
        sc = sc->outer;
    }
    while (sc != NULL);

    // If not found there, then look it up in used modules; search backwards
    for (memint i = module.uses.size(); i-- && sym == NULL; )
    {
        SelfVar* m = module.uses[i];
        sym = m->getModuleType()->find(ident);
    }

    if (sym == NULL)
        throw EUnknownIdent(ident);

    codegen->loadSymbol(sym);
}


void Compiler::vectorCtor()
{
    if (skipIf(tokRSquare))
    {
        codegen->loadEmptyCont(queenBee->defNullCont);
        return;
    }
    expression();
    codegen->elemToVec();
    while (skipIf(tokComma))
    {
        expression();
        codegen->elemCat();
    }
    expect(tokRSquare, "]");
}


void Compiler::dictCtor()
{
    if (skipIf(tokRCurly))
    {
        codegen->loadEmptyCont(queenBee->defNullCont);
        return;
    }

    runtimeExpr();
    codegen->deref();  // keys are always values

    // Dictionary
    if (skipIf(tokAssign))
    {
        expression();
        codegen->pairToDict();
        while (skipIf(tokComma))
        {
            expression();
            codegen->checkDictKey();
            expect(tokAssign, "=");
            expression();
            codegen->dictAddPair();
        }
    }

    // Set
    else
    {
        if (skipIf(tokRange))
        {
            runtimeExpr();
            codegen->rangeToSet();
        }
        else
            codegen->elemToSet();
        while (skipIf(tokComma))
        {
            runtimeExpr();
            if (skipIf(tokRange))
            {
                codegen->checkRangeLeft();
                runtimeExpr();
                codegen->setAddRange();
            }
            else
                codegen->setAddElem();
        }
    }

    expect(tokRCurly, "}");
}


// --- EXPRESSION ---------------------------------------------------------- //

/*
    1. <nested-expr>  <ident>  <number>  <string>  <char>  <compound-ctor>  <type-spec>
    2. <array-sel>  <member-sel>  <function-call>  ^
    3. unary-  #  as  is  ?
    5. *  /  mod
    6. +  –
    7. |
    8. ==  <>  <  >  <=  >=  in
    9. not
    10. and
    11. or  xor
    12. (const) range, enum
*/


void Compiler::atom()
{
    if (token == tokPrevIdent)  // from partial (typeless) definition
    {
        identifier(getPrevIdent());
        redoIdent();
    }

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
            module.registerString(value);
            codegen->loadConst(queenBee->defStr, value);
        }
        next();
    }

    else if (token == tokIdent)
    {
        identifier(strValue);
        next();
    }

    else if (skipIf(tokLParen))
    {
        expression();
        expect(tokRParen, "')'");
    }

    else if (skipIf(tokLSquare))
        vectorCtor();

    else if (skipIf(tokLCurly))
        dictCtor();
/*
    // TODO: 
    else if (skipIf(tokIf))
        ifFunction();

    else if (skipIf(tokTypeOf))
        typeOf();
*/
    else
        error("Expression syntax");

    if (token == tokLSquare || token == tokNotEq || token == tokWildcard
        || token == tokCaret)
    {
        Type* type = codegen->tryUndoTypeRef();
        if (type != NULL)
            codegen->loadTypeRef(getTypeDerivators(type));
    }
}


void Compiler::designator()
{
    // TODO: qualifiers, function calls
    // TODO: assignment
    atom();
    while (1)
    {
        if (skipIf(tokPeriod))
        {
            // TODO: see if it's a definition and discard all preceding code
            codegen->deref();
            codegen->loadMember(getIdentifier());
            next();
        }

        else if (skipIf(tokLSquare))
        {
            codegen->deref();
            expression();
            codegen->deref();  // keys are always values
            expect(tokRSquare, "]");
            codegen->loadContainerElem();
        }

        else if (skipIf(tokCaret))
        {
            // Note that ^ as a type derivator is handled earlier in getTypeDerivators()
            if (!codegen->deref())
                error("Dereference (^) on a non-reference value");
        }
        else
            break;
    }
}


void Compiler::factor()
{
    bool isNeg = skipIf(tokMinus);
    bool isLen = skipIf(tokSharp);

    designator();

    if (isLen)
    {
        codegen->deref();
        codegen->length();
    }
    if (isNeg)
    {
        codegen->deref();
        codegen->arithmUnary(opNeg);
    }
    if (skipIf(tokQuestion))
    {
        codegen->deref();
        codegen->nonEmpty();
    }
    // TODO: as, is
}


void Compiler::term()
{
    factor();
    while (token == tokMul || token == tokDiv || token == tokMod)
    {
        codegen->deref();
        OpCode op = token == tokMul ? opMul
            : token == tokDiv ? opDiv : opMod;
        next();
        factor();
        codegen->deref();
        codegen->arithmBinary(op);
    }
}


void Compiler::arithmExpr()
{
    term();
    while (token == tokPlus || token == tokMinus)
    {
        codegen->deref();
        OpCode op = token == tokPlus ? opAdd : opSub;
        next();
        term();
        codegen->deref();
        codegen->arithmBinary(op);
    }
}


void Compiler::simpleExpr()
{
    arithmExpr();
    if (token == tokCat)
    {
        Container* contType = NULL;
        // The trick here is to ignore any null containers in the expression,
        // and correctly figure out the container type at the same time.
        while (1)
        {
            codegen->deref();
            Type* top = codegen->getTopType();
            if (top->isNullCont())
                codegen->undoLastLoad();
            else if (contType == NULL)  // first non-null element, container type unknown yet
            {
                if (top->isAnyVec())
                    contType = PContainer(top);
                else
                    contType = codegen->elemToVec();
            }
            else // non-null element, container type known
            {
                if (top->canAssignTo(contType))  // compatible vector? then concatenate as vector
                    codegen->cat();
                else
                    codegen->elemCat();
            }
            if (!skipIf(tokCat))  // first iteration will return tokCat
                break;
            arithmExpr();
        }
    }
}


void Compiler::relation()
{
    // TODO: operator 'in'
    simpleExpr();
    if (token >= tokEqual && token <= tokGreaterEq)
    {
        codegen->deref();
        OpCode op = OpCode(opEqual + int(token - tokEqual));
        next();
        simpleExpr();
        codegen->deref();
        codegen->cmp(op);
    }
}


void Compiler::notLevel()
{
    bool isNot = skipIf(tokNot);
    relation();
    if (isNot)
    {
        codegen->deref();
        codegen->_not();
    }
}


void Compiler::andLevel()
{
    notLevel();
    while (token == tokShl || token == tokShr || token == tokAnd)
    {
        codegen->deref();
        Type* type = codegen->getTopType();
        if (type->isBool() && skipIf(tokAnd))
        {
            memint offs = codegen->boolJumpForward(opJumpAnd);
            andLevel();
            codegen->deref();
            codegen->resolveJump(offs);
            break;
        }
        else // if (type->isInt())
        {
            OpCode op = token == tokShl ? opBitShl
                    : token == tokShr ? opBitShr : opBitAnd;
            next();
            notLevel();
            codegen->deref();
            codegen->arithmBinary(op);
        }
    }
}


void Compiler::orLevel()
{
    andLevel();
    while (token == tokOr || token == tokXor)
    {
        codegen->deref();
        Type* type = codegen->getTopType();
        // TODO: boolean XOR? Beautiful thing, but not absolutely necessary
        if (type->isBool() && skipIf(tokOr))
        {
            memint offs = codegen->boolJumpForward(opJumpOr);
            orLevel();
            codegen->deref();
            codegen->resolveJump(offs);
            break;
        }
        else // if (type->isInt())
        {
            OpCode op = token == tokOr ? opBitOr : opBitXor;
            next();
            andLevel();
            codegen->deref();
            codegen->arithmBinary(op);
        }
    }
}


void Compiler::expression()
{
    // TODO: const expression (e.g. const [1, 2, 3] or const 0..10)
    if (!codegen->isCompileTime())
        runtimeExpr();
    else if (token == tokIdent)  // Enumeration maybe?
    {
        str ident = strValue;
        if (next() != tokComma)
        {
            undoIdent(ident);
            goto ICouldHaveDoneThisWithoutGoto;
        }
        enumeration(ident);
    }
    else
    {
ICouldHaveDoneThisWithoutGoto:
        runtimeExpr();
        if (skipIf(tokRange))  // Subrange
        {
            runtimeExpr();
            codegen->createSubrangeType();
        }
    }
}


// ------------------------------------------------------------------------- //


Type* Compiler::getConstValue(Type* expectType, variant& result)
{
    CodeSeg constCode(NULL);
    CodeGen constCodeGen(constCode, state, true);
    CodeGen* prevCodeGen = exchange(codegen, &constCodeGen);
    expression();
    if (codegen->getTopType()->isReference())
        error("References not allowed in const expressions");
    Type* resultType = constCodeGen.runConstExpr(expectType, result);
    codegen = prevCodeGen;
    return resultType;
}


Type* Compiler::getTypeValue()
{
    variant result;
    getConstValue(defTypeRef, result);
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
    type = getTypeValue();
    ident = getIdentifier();
    next();
ICantBelieveIUsedAGotoStatement:
    expect(tokAssign, "'='");
    return type;
}


void Compiler::definition()
{
    str ident;
    Type* type = getTypeAndIdent(ident);
    variant value;
    Type* valueType = getConstValue(type, value);
    if (type == NULL)
        type = valueType;
    if (type->isAnyOrd() && !POrdinal(type)->isInRange(value.as_ord()))
        error("Constant out of range");
    state->addDefinition(ident, type, value);
    skipSep();
}


void Compiler::variable()
{
    // TODO: const variables
    str ident;
    Type* type = getTypeAndIdent(ident);
    runtimeExpr();
    if (type == NULL)
        type = codegen->getTopType();
    else
    {
        Type* exprType = codegen->getTopType();
        // Automatic mkref is allowed only when initializing the var,
        // otherwise '^' must be used.
        if (type->isReference() && !exprType->isReference())
            codegen->mkref();
        codegen->implicitCast(type);
    }
    if (type->isNullCont())
        error("Type undefined (null container)");
    if (blockScope != NULL)
    {
        LocalVar* var = blockScope->addLocalVar(ident, type);
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
        expression();
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
            expression();
            str s = endRecording();
            module.registerString(s);
            codegen->dumpVar(s);
        }
        while (token == tokComma);
    else
        skipToSep();
    skipSep();
}


void Compiler::otherStatement()
{
    // TODO: assignment, call, pipe, etc
    memint stkLevel = codegen->getStackLevel();
    codegen->beginLValue();
    designator();
    str storerCode = codegen->endLValue(token == tokAssign);
    if (skipIf(tokAssign))
    {
        runtimeExpr();
        if (!isSep())
            error("Statement syntax");
        codegen->assignment(storerCode);
    }
    if (isSep() && codegen->getStackLevel() != stkLevel)
        error("Unused value from previous statement");
    skipSep();
}


void Compiler::statementList()
{
    while (!skipIfBlockEnd())
    {
        if (context.options.lineNumbers)
            codegen->linenum(getLineNum());

        if (skipIf(tokSep))
            ;
        else if (skipIf(tokDef))
            definition();
        else if (skipIf(tokVar))
            variable();
/*
        else if (skipIf(tokBlockBegin))
            block();
        else if (skipIf(tokBegin))
        {
            skipBlockBegin();
            block();
        }
*/
        else if (token == tokAssert)
            assertion();
        else if (token == tokDump)
            dumpVar();
        else if (eof())
            break;
        else
            otherStatement();
    }
}


void Compiler::compileModule()
{
    // The system module is always added implicitly
    module.addUses(queenBee);
    // Start parsing and code generation
    CodeGen mainCodeGen(*module.getCodeSeg(), &module, false);
    codegen = &mainCodeGen;
    blockScope = NULL;
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
            if (!strValue.empty())
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

