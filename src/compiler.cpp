
#include "vm.h"
#include "compiler.h"


Compiler::Compiler(Context& c, Module& mod, buffifo* f)
    : Parser(f), context(c), module(mod)  { }


Compiler::~Compiler()
    { }


void Compiler::enumeration()
{
    Enumeration* enumType = state->registerType(new Enumeration());
    do
    {
        enumType->addValue(state, getIdentifier());
        next();
    }
    while (skipIf(tokComma));
    codegen->loadTypeRef(enumType);
}


void Compiler::subrange()
{
    // TODO: a more optimal compilation. Subrange is usually compiled within a const
    // context, and we call two more const evaluations here for left and right bounds.
    variant left, right;
    Type* type = getConstValue(NULL, left);
    if (!type->isAnyOrd())
        error("Ordinal type expected in subrange");
    expect(tokRange, "'..'");
    getConstValue(type, right); // will ensure compatibility with 'left'
    codegen->loadTypeRef(state->registerType(
            POrdinal(type)->createSubrange(left._ord(), right._ord())));
}


Type* Compiler::getTypeDerivators(Type* type)
{
    if (skipIf(tokLSquare))
    {
        if (token == tokRSquare)
            type = type->deriveVec(state);
        else if (skipIf(tokRange))
            type = type->deriveSet(state);
        else
        {
            Type* indexType = getTypeValue();
            if (indexType->isNone())
                type = type->deriveVec(state);
            else
                type = type->deriveContainer(state, indexType);
        }
        expect(tokRSquare, "']'");
    }

    else if (skipIf(tokNotEq)) // <>
        type = type->deriveFifo(state);

    // TODO: function derivator
    // else if (token == tokWildcard)

    else if (skipIf(tokCaret)) // ^
    {
        if (type->isReference())
            error("Double reference");
        if (!type->isDerefable())
            error("Reference can not be derived from this type");
        type = type->getRefType();
    }

    else
        return type;

    return getTypeDerivators(type);
}


void Compiler::identifier(const str& ident)
{
    Scope* sc = scope;
    Symbol* sym;
    Variable* moduleVar = NULL;

    // Go up the current scope hierarchy within the module
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
        moduleVar = module.uses[i];
        sym = moduleVar->getModuleType()->find(ident);
    }

    if (sym == NULL)
        throw EUnknownIdent(ident);

    codegen->loadSymbol(moduleVar, sym);
}


void Compiler::vectorCtor()
{
    // Note: no automatic dereference here
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


// --- EXPRESSION ---------------------------------------------------------- //

/*
    1. <nested-expr>  <ident>  <number>  <string>  <char>  <compound-ctor>  <type-spec>
    2. <array-sel>  <member-sel>  <function-call>  ^
    3. unary-
    4. as  is
    5. *  /  mod
    6. +  –
    7. |
    8. ==  <>  <  >  <=  >=  in
    9. not
    10. and
    11. or  xor
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
/*
    // TODO: 
    else if (skipIf(tokLCurly))
        dictCtor(NULL);

    else if (skipIf(tokIf))
        ifFunction();

    else if (skipIf(tokTypeOf))
        typeOf();
*/
    else if (skipIf(tokSub))
        subrange();

    else if (skipIf(tokEnum))
        enumeration();

    else
        errorWithLoc("Expression syntax");

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
    // TODO: qualifiers, container element selectors, function calls
    // TODO: assignment
    atom();
    while (1)
    {
        if (skipIf(tokPeriod))
        {
            codegen->deref();
            codegen->loadMember(getIdentifier());
            next();
        }
        
        else if (skipIf(tokLSquare))
        {
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
    designator();
    if (isNeg)
    {
        codegen->deref();
        codegen->arithmUnary(opNeg);
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
    if (skipIf(tokCat))
    {
        codegen->deref();  // !
        if (!codegen->getTopType()->isVec())
            codegen->elemToVec();
        do
        {
            arithmExpr();
            codegen->deref();
            if (!codegen->getTopType()->isVec())
                codegen->elemCat();
            else
                codegen->cat();
        }
        while (skipIf(tokCat));
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
        if (type->isBool() && token == tokAnd)
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
        if (type->isBool() && token == tokOr)
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


inline void Compiler::expression()
    { orLevel(); }


void Compiler::expression(Type* expectType)
{
    expression();
    codegen->implicitCast(expectType);
}


// ------------------------------------------------------------------------- //


Type* Compiler::getConstValue(Type* expectType, variant& result)
{
    CodeSeg constCode(NULL);
    CodeGen constCodeGen(constCode, state);
    CodeGen* prevCodeGen = exchange(codegen, &constCodeGen);
    if (expectType && expectType->isTypeRef())
        atom();  // Take a shorter path
    else
        expression();
    codegen->deref();
    Type* resultType = constCodeGen.runConstExpr(expectType, result);
    codegen = prevCodeGen;
    return resultType;
}


Type* Compiler::getTypeValue()
{
    variant result;
    getConstValue(defTypeRef, result);
    return cast<Type*>(result._rtobj());
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
    return type;
}


void Compiler::definition()
{
    // definition ::= 'def' [ const-expr ] ident { type-derivator } [ '=' const-expr ]
    str ident;
    Type* type = getTypeAndIdent(ident);
    // TODO: if ref type, take the original type?
    expect(tokAssign, "'='");
    variant value;
    Type* valueType = getConstValue(type, value);
    if (type == NULL)
        type = valueType;
    if (type->isAnyOrd() && !POrdinal(type)->isInRange(value.as_ord()))
        error("Constant out of range");
    state->addDefinition(ident, type, value);
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
        codegen->assertion(s, module.filePath, ln);
    }
    else
    {
        memint offs = codegen->beginDiscardable();
        next();
        expression(queenBee->defBool);
        codegen->popValue();
        codegen->endDiscardable(offs);
    }
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
        do
        {
            memint offs = codegen->beginDiscardable();
            next();
            expression();
            codegen->popValue();
            codegen->endDiscardable(offs);
        }
        while (token == tokComma);
    skipSep();
}


void Compiler::otherStatement()
{
    // TODO: assignment, call, pipe, etc
    notimpl();
    skipSep();
}


void Compiler::statementList()
{
    while (!skipIfBlockEnd())
    {
        if (skipIf(tokSep))
            ;
        else if (skipIf(tokDef))
            definition();
/*
        else if (skipIf(tokVar))
            variable();
        else if (skipIf(tokDump))
            echo();
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
    CodeGen mainCodeGen(*module.codeseg, &module);
    codegen = &mainCodeGen;
    blockScope = NULL;
    scope = state = &module;

    try
    {
        next();
        statementList();
        expect(tokEof, "End of file");
    }
    catch (EDuplicate& e)
        { error("'" + e.ident + "' is already defined within this scope"); }
    catch (EUnknownIdent& e)
        { error("'" + e.ident + "' is unknown in this context"); }
    catch (EParser& e)
        { throw; }    // comes with file name and line no. already
    catch (exception& e)
        { error(e.what()); }

    mainCodeGen.end();
    module.setComplete();

//    if (options.vmListing)
//    {
//        outtext f(NULL, remove_filename_ext(getFileName()) + ".lst");
//        mainModule.listing(f);
//    }
}

