
#include "vm.h"
#include "compiler.h"


Compiler::Compiler(Context& c, ModuleDef& mod, fifo* f) throw()
    : Parser(f), context(c), moduleDef(mod)  { }


Compiler::~Compiler() throw()
    { }


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

    // If not found there, then look it up in used modules; search backwards
    if (sym == NULL)
    {
        objvec<ModuleVar> uses = moduleDef.module->uses;
        for (memint i = uses.size(); i--, sym == NULL; )
        {
            moduleVar = uses[i];
            sym = moduleVar->getModuleType()->find(ident);
        }
    }

    if (sym == NULL)
        throw EUnknownIdent(ident);

    codegen->loadSymbol(moduleVar, sym);
}


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
            moduleDef.module->registerString(value);
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
    atom();
}


void Compiler::factor()
{
    bool isNeg = skipIf(tokMinus);
    designator();
    if (isNeg)
        codegen->arithmUnary(opNeg);
/*
    // TODO: 
    else if (token == tokWildcard)
    {
        // anonymous type spec
        Type* type = codegen->getLastTypeRef();
        if (type != NULL)
        {
            next();
            codegen->loadTypeRef(getTypeDerivators(type));
        }
    }
*/
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


void Compiler::expression(Type* resultType, const char* errmsg)
{
    // TODO: ?
    arithmExpr();
    if (resultType != NULL)
        codegen->implicitCast(resultType, errmsg ? errmsg : "Expression type mismatch");
}


void Compiler::subexpression()
{
    // TODO: vector constructor
    expression();
}


Type* Compiler::getTypeDerivators(Type* type)
{
    // TODO:
    return type;
}


Type* Compiler::getConstValue(Type* resultType, variant& result)
{
    CodeSeg constCode(NULL);
    CodeGen constCodeGen(constCode);
    CodeGen* prevCodeGen = exchange(codegen, &constCodeGen);
    expression();
    resultType = constCodeGen.runConstExpr(resultType, result);
    codegen = prevCodeGen;
    return resultType;
}


Type* Compiler::getTypeValue()
{
    variant result;
    getConstValue(defTypeRef, result);
    // TODO: range
    // TODO: enum
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
        undoIdent(ident);
    }
    type = getTypeValue();
    ident = getIdentifier();
    next();
    type = getTypeDerivators(type);
ICantBelieveIUsedAGotoStatement:
    expect(tokAssign, "'='");
    return type;
}


void Compiler::definition()
{
    // definition ::= 'def' [ type-expr ] ident { type-derivator } [ '=' expr ]
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
        else
            assignment();
*/
    }
}


void Compiler::module()
{
    // The system module is always added implicitly
    moduleDef.module->addUses(queenBee);
    // Start parsing and code generation
    CodeGen mainCodeGen(*moduleDef.getCodeSeg());
    codegen = &mainCodeGen;
    blockScope = NULL;
    scope = state = moduleDef.getStateType();

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
    moduleDef.setComplete();

//    if (options.vmListing)
//    {
//        outtext f(NULL, remove_filename_ext(getFileName()) + ".lst");
//        mainModule.listing(f);
//    }
}

