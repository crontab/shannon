
#include "vm.h"
#include "compiler.h"


Compiler::Compiler(Context& c, ModuleInst& mod, fifo* f) throw()
    : Parser(f), context(c), moduleInst(mod)  { }


Compiler::~Compiler() throw()
    { }


void Compiler::enumeration(const str& firstIdent)
{
    Enumeration* enumType = state->registerType(new Enumeration());
    enumType->addValue(state, firstIdent);
    while (skipIf(tokComma))
    {
        if (token == tokRParen) // allow trailing comma
            break;
        enumType->addValue(state, getIdentifier());
        next();
    }
    expect(tokRParen, "')'");
    codegen->loadTypeRef(enumType);
}


Type* Compiler::getTypeDerivators(Type* type)
{
    if (skipIf(tokLSquare))
    {
        if (token == tokRSquare)
            type = type->deriveVec();
        else if (skipIf(tokRange))
            type = type->deriveSet();
        else
        {
            Type* indexType = getTypeValue();
            if (indexType->isNone())
                type = type->deriveVec();
            else
                type = type->deriveContainer(indexType);
        }
        state->registerType(type);
        expect(tokRSquare, "']'");
    }

    else if (skipIf(tokNotEq)) // <>
        type = state->registerType(type->deriveFifo());

    else
        return type;

    // TODO: function derivator

    return getTypeDerivators(type);
}


void Compiler::identifier(const str& ident)
{
    Scope* sc = scope;
    Symbol* sym;
    ModuleVar* moduleVar = NULL;

    // Go up the current scope hierarchy within the module
    do
    {
        sym = scope->find(ident);
        sc = sc->outer;
    }
    while (sym == NULL && sc != NULL);

    // If not found there, then look it up in used modules; look up module names
    // themselves, too; search backwards
    if (sym == NULL)
    {
        objvec<ModuleVar> uses = moduleInst.module->uses;
        for (memint i = uses.size(); i--, sym == NULL; )
        {
            moduleVar = uses[i];
            if (ident == moduleVar->name)
            {
                sym = moduleVar;
                moduleVar = NULL;
            }
            else
                sym = moduleVar->getModuleType()->find(ident);
        }
    }

    if (sym == NULL)
        throw EUnknownIdent(ident);

    codegen->loadSymbol(moduleVar, sym);
}


// --- EXPRESSION ---------------------------------------------------------- //

/*
    1. <nested-expr>  <ident>  <number>  <string>  <char>  <compound-ctor>
    2. <array-sel>  <member-sel>  <function-call>
    3. unary-  <type-derivators>
    4. as  is
    5. *  /  mod
    6. +  –
    7. |
    8. ==  <>  !=  <  >  <=  >=  in
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
            moduleInst.module->registerString(value);
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
        if (token == tokIdent)
        {
            str ident = strValue;
            if (next() == tokComma)
                enumeration(ident);
            else
            {
                undoIdent(ident);
                goto ICantBelieveIUsedAGotoStatementShameShame;
            }
        }
        else
        {
ICantBelieveIUsedAGotoStatementShameShame:
            expression();
            expect(tokRParen, "')'");
        }
    }
/*
    // TODO:
    else if (skipIf(tokLSquare))
        compoundCtor(NULL);

    else if (skipIf(tokIf))
        ifFunction();

    else if (skipIf(tokTypeOf))
        typeOf();
*/
    else
        errorWithLoc("Expression syntax");
}


void Compiler::designator()
{
    // TODO: qualifiers, container element selectors, function calls
    atom();
}


void Compiler::factor()
{
    bool isNeg = skipIf(tokMinus);
    designator();
    if (isNeg)
        codegen->arithmUnary(opNeg);
    else if (token == tokLSquare || token == tokNotEq)
    {
        Type* type = codegen->tryUndoTypeRef();
        if (type != NULL)
            codegen->loadTypeRef(getTypeDerivators(type));
    }
}


void Compiler::term()
{
    factor();
    while (token == tokMul || token == tokDiv || token == tokMod)
    {
        OpCode op = token == tokMul ? opMul
                : token == tokDiv ? opDiv : opMod;
        next();
        factor();
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


Type* Compiler::getConstValue(Type* expectType, variant& result)
{
    CodeSeg constCode(NULL);
    CodeGen constCodeGen(constCode);
    CodeGen* prevCodeGen = exchange(codegen, &constCodeGen);
    expression();
    Type* resultType = constCodeGen.runConstExpr(expectType, result);
    codegen = prevCodeGen;
    if (resultType->isAnyOrd() && token == tokRange)
    {
        next();
        variant right;
        getConstValue(resultType, right);
        result = state->registerType(
            POrdinal(resultType)->createSubrange(result._ord(), right._ord()));
        resultType = defTypeRef;
        if (expectType != NULL && !expectType->isTypeRef())
            error("Subrange type specifier is not expected here");
    }
    return resultType;
}


Type* Compiler::getTypeValue()
{
    // TODO: make this faster?
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
        // TODO: see if the type specifier is a single ident and parse it
        // immediately here
        undoIdent(ident);
    }
    type = getTypeValue();
    ident = getIdentifier();
    next();
    type = getTypeDerivators(type);
ICantBelieveIUsedAGotoStatement:
    return type;
}


void Compiler::definition()
{
    // definition ::= 'def' [ const-expr ] ident { type-derivator } [ '=' const-expr ]
    str ident;
    // TODO: typedef-style definition ?
    Type* type = getTypeAndIdent(ident);
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


void Compiler::assignment()
{
    notimpl();
}


void Compiler::statementList()
{
    while (!skipIf(tokBlockEnd))
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
        else if (skipIf(tokAssert))
            assertion();
        else if (skipIf(tokBlockBegin))
            block();
        else if (skipIf(tokBegin))
        {
            skipBlockBegin();
            block();
        }
*/
        else
            assignment();
    }
}


void Compiler::module()
{
    // The system module is always added implicitly
    moduleInst.module->addUses(context.queenBeeInst->name, context.queenBeeInst->module);
    // Start parsing and code generation
    CodeGen mainCodeGen(*moduleInst.module->codeseg);
    codegen = &mainCodeGen;
    blockScope = NULL;
    scope = state = moduleInst.module;

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
    moduleInst.setComplete();

//    if (options.vmListing)
//    {
//        outtext f(NULL, remove_filename_ext(getFileName()) + ".lst");
//        mainModule.listing(f);
//    }
}

