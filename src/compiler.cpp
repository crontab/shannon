
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
            Type* indexType = getTypeValue(false);
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
    // Go up the current scope hierarchy within the module
    Scope* sc = scope;
    do
    {
        // TODO: implement loading from outer scopes
        Symbol* sym = sc->find(ident);
        if (sym)
        {
            codegen->loadSymbol(sym);
            return;
        }
        sc = sc->outer;
    }
    while (sc != NULL);

    // Look up in used modules; search backwards
    for (memint i = module.uses.size(); i--; )
    {
        SelfVar* m = module.uses[i];
        Symbol* sym = m->getModuleType()->find(ident);
        if (sym)
        {
            if (codegen->isCompileTime())
                codegen->loadSymbol(sym);
            else
            {
                memint offs = codegen->getCurrentOffs();
                codegen->loadVariable(m);
                codegen->loadMember(sym, offs);
            }
            return;
        }
    }

    throw EUnknownIdent(ident);
}


void Compiler::vectorCtor(Type* typeHint)
{
    if (typeHint && !typeHint->isAnyVec())
        error("Vector constructor not expected here");
    Container* type = PContainer(typeHint);
    if (skipIf(tokRSquare))
    {
        codegen->loadEmptyCont(type ? type : queenBee->defNullCont);
        return;
    }
    runtimeExpr(type ? type->elem : NULL);
    type = codegen->elemToVec(type);
    while (skipIf(tokComma))
    {
        runtimeExpr(type->elem);
        codegen->elemCat();
    }
    expect(tokRSquare, "]");
}


void Compiler::dictCtor(Type* typeHint)
{
    if (typeHint && !typeHint->isAnySet() && !typeHint->isAnyDict())
        error("Set/dict constructor not expected here");
    Container* type = PContainer(typeHint);
    if (skipIf(tokRCurly))
    {
        codegen->loadEmptyCont(type ? type : queenBee->defNullCont);
        return;
    }

    runtimeExpr(type ? type->index : NULL);

    // Dictionary
    if (skipIf(tokAssign))
    {
        runtimeExpr(type ? type->elem : NULL);
        type = codegen->pairToDict();
        while (skipIf(tokComma))
        {
            runtimeExpr(type->index);
            codegen->checkDictKey();
            expect(tokAssign, "=");
            runtimeExpr(type->elem);
            codegen->dictAddPair();
        }
    }

    // Set
    else
    {
        if (skipIf(tokRange))
        {
            runtimeExpr(type ? type->index : NULL);
            type = codegen->rangeToSet();
        }
        else
            type = codegen->elemToSet();
        while (skipIf(tokComma))
        {
            runtimeExpr(type->index);
            if (skipIf(tokRange))
            {
                codegen->checkRangeLeft();
                runtimeExpr(type->index);
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
    <nested-expr>  <ident>  <number>  <string>  <char>  <type-spec>
    @ <array-sel>  <member-sel>  <function-call>  ^
    unary-  #  as  is  ?
    |
    *  /  mod
    +  –
    ==  <>  <  >  <=  >=  in
    not
    and
    or  xor
    range, enum
*/


void Compiler::atom(Type* typeHint)
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
        if (codegen->isCompileTime())
            constExpr(typeHint);
        else
            runtimeExpr(typeHint);
        expect(tokRParen, "')'");
    }

    else if (skipIf(tokLSquare))
        vectorCtor(typeHint);

    else if (skipIf(tokLCurly))
        dictCtor(typeHint);
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


void Compiler::designator(Type* typeHint)
{
    // TODO: function calls
    memint undoOffs = codegen->getCurrentOffs();
    bool isAt = skipIf(tokAt);
    Type* refTypeHint = typeHint && typeHint->isReference() ? PReference(typeHint)->to : NULL;

    atom(refTypeHint ? refTypeHint : typeHint);

    while (1)
    {
        if (skipIf(tokPeriod))
        {
            // undoOffs is needed when the member is a constant, in which case
            // all previous loads should be discarded - they are not needed to
            // load a constant
            codegen->deref();
            codegen->loadMember(getIdentifier(), undoOffs);
            next();
        }

        else if (skipIf(tokLSquare))
        {
            codegen->deref();
            runtimeExpr(NULL);
            expect(tokRSquare, "]");
            codegen->loadContainerElem();
        }

        else if (skipIf(tokCaret))
        {
            // Note that ^ as a type derivator is handled earlier in getTypeDerivators()
            notimpl(); // makes sense only on states to create a copy
            // if (!codegen->deref())
            //     error("Dereference (^) on a non-reference value");
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
    bool isLen = skipIf(tokSharp);

    memint undoOffs = codegen->getCurrentOffs();
    designator(typeHint);

    if (isLen)
        codegen->length();
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
    {
        bool isnot = skipIf(tokNot);
        Type* type = getTypeValue(true);
        // TODO: "is not..."
        codegen->isType(type, isnot, undoOffs);
    }
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
    // TODO: operator 'in'
    arithmExpr();
    if (token >= tokEqual && token <= tokGreaterEq)
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


void Compiler::runtimeExpr(Type* expectType)
{
    if (expectType == NULL || expectType->isBool())
        orLevel();
    else if (expectType->isAnyCont())
        concatExpr(PContainer(expectType));
    else if (expectType->isReference())
        designator(expectType);
    else
        arithmExpr();
        
    if (expectType)
        codegen->implicitCast(expectType);
}


void Compiler::constExpr(Type* expectType)
{
    // TODO: const expression (e.g. const [1, 2, 3] or const 0..10)
    assert(codegen->isCompileTime());
    if (token == tokIdent)  // Enumeration maybe?
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
        runtimeExpr(expectType);
        if (skipIf(tokRange))  // Subrange
        {
            runtimeExpr(NULL);
            codegen->createSubrangeType();
        }
    }
    if (expectType)
        codegen->implicitCast(expectType);
}


// ------------------------------------------------------------------------- //


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
    state->addDefinition(ident, type, value);
    skipSep();
}


void Compiler::variable()
{
    // TODO: const variables
    str ident;
    Type* type = getTypeAndIdent(ident);
    runtimeExpr(type);
    if (type == NULL)
        type = codegen->getTopType();
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
        runtimeExpr(NULL);
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
            runtimeExpr(NULL);
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
    // TODO: call, pipe, etc
    memint stkLevel = codegen->getStackLevel();
    designator(NULL);
    if (skipIf(tokAssign))
    {
        str storerCode = codegen->lvalue();
        runtimeExpr(codegen->getTopType());
        if (!isSep())
            error("Statement syntax");
        codegen->assignment(storerCode);
    }
    if (isSep() && codegen->getStackLevel() != stkLevel)
        error("Unused value in statement");
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

