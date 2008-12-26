
#include <stdlib.h>
#include <stdio.h>

#include "str.h"
#include "except.h"
#include "langobj.h"
#include "vm.h"


// ------------------------------------------------------------------------- //
// --- HIS MAJESTY THE COMPILER -------------------------------------------- //
// ------------------------------------------------------------------------- //


string typeVsType(ShType* a, ShType* b)
{
    return a->getDefinitionQ() + " vs. " + b->getDefinitionQ();
}


// --- EXPRESSION ---------------------------------------------------------- //

/*
    <nested-expr>, <typecast>, <ident>, <number>, <string>, <char>, true, false, null
    <array-sel>, <fifo-sel>, <function-call>, <mute>
    -, not
    *, /, mod, and, shl, shr, as
    +, –, or, xor
    ..
    ==, <>, != <, >, <=, >=, in, is
*/

ShBase* ShModule::getQualifiedName()
{
    string ident = parser.getIdent();
    ShBase* obj = currentScope->deepFind(ident);
    if (obj == NULL)
        errorNotFound(ident);
    string errIdent = ident;
    while (parser.token == tokPeriod)
    {
        if (!obj->isScope())
            return obj;
        ShScope* scope = (ShScope*)obj;
        parser.next(); // "."
        ident = parser.getIdent();
        errIdent += '.' + ident; // this is important for the hack in getTypeOrNewIdent()
        obj = scope->find(ident);
        if (obj == NULL)
            errorNotFound(errIdent);
    }
    return obj;
}


void ShModule::parseAtom(VmCode& code)
{
    static int myFunnyZeroValue = 0; // are you happy now, GCC?

    if (parser.skipIf(tokLParen))
    {
        parseExpr(code);
        parser.skip(tokRParen, ")");
    }

    // numeric literal
    else if (parser.token == tokIntValue)
    {
        large value = parser.intValue; // intValue is unsigned int
        parser.next();
        if (!queenBee->defaultInt->contains(value))
            error("Value out of range (use the 'L' suffix for large consts)");
        code.genLoadConst(ShValue(queenBee->defaultInt, value));
    }

    else if (parser.token == tokLargeValue)
    {
        ularge value = parser.largeValue; // largeValue is unsigned int
        parser.next();
        code.genLoadConst(ShValue(queenBee->defaultLarge, value));
    }

    // string or char literal
    else if (parser.token == tokStrValue)
    {
        if (parser.strValue.size() == 1)
        {
            int value = (unsigned)parser.strValue[0];
            parser.next();
            code.genLoadConst(ShValue(queenBee->defaultChar, value));
        }
        else
        {
            const string s = registerString(parser.strValue);
            parser.next();
            code.genLoadConst(ShValue(queenBee->defaultStr, s));
        }
    }

    // identifier
    else if (parser.token == tokIdent)
    {
        ShBase* obj = getQualifiedName();
        bool isAlias = obj->isTypeAlias();
        // symbolic constant
        if (obj->isConstant())
            code.genLoadConst(((ShConstant*)obj)->value);

        // type or type alias: static cast
        // only typeid(expr) is allowed for function-style typecasts
        else if (obj->isType() || isAlias)
        {
            ShType* type = isAlias ? ((ShTypeAlias*)obj)->base : (ShType*)obj;
            if (parser.token == tokLParen)
            {
                parser.next();
                parseExpr(code);
                parser.skip(tokRParen, ")");
                ShType* fromType = code.topType();
                if (!fromType->canStaticCastTo(type))
                    error("Can't do static typecast from " + fromType->getDefinitionQ()
                        + " to " + type->getDefinitionQ());
                code.genStaticCast(type);
            }
            else
                code.genLoadTypeRef(getDerivators(type));
        }
        // TODO: vars, funcs
        else
            notImpl();
    }
    
    else if (parser.skipIf(tokTypeOf))
    {
        parser.skip(tokLParen, "(");
        ShType* type;
        {
            VmCode tcode(currentScope);
            parseExpr(tcode);
            type = tcode.runTypeExpr();
        }
        parser.skip(tokRParen, ")");
        code.genLoadTypeRef(type);
    }

    else if (parser.skipIf(tokTrue))
        code.genLoadConst(ShValue(queenBee->defaultBool, 1));

    else if (parser.skipIf(tokFalse))
        code.genLoadConst(ShValue(queenBee->defaultBool, myFunnyZeroValue));
    
    else if (parser.skipIf(tokNull))
        code.genLoadConst(ShValue(queenBee->defaultVoid, myFunnyZeroValue));
    
    else
        errorWithLoc("Expression syntax");
}


void ShModule::parseDesignator(VmCode& code)
{
    parseAtom(code);
}


void ShModule::parseFactor(VmCode& code)
{
    // TODO: boolean NOT
    bool isNeg = parser.token == tokMinus;
    if (isNeg)
        parser.next();
    parseDesignator(code);
    if (isNeg)
    {
        ShType* type = code.topType();
        if (!type->isInt())
            error("Invalid operand for arithmetic negation");
        code.genUnArithm(opNeg, PInteger(type));
    }
}


ShInteger* ShModule::arithmResultType(ShInteger* left, ShInteger* right)
{
    if (PInteger(left)->isLargeInt() != PInteger(right)->isLargeInt())
        error("Mixing int and large: typecast needed (or 'L' with numbers)");
    ShInteger* resultType = PInteger(left);
    if (PInteger(right)->rangeIsGreaterOrEqual(PInteger(left)))
        resultType = PInteger(right);
    return resultType;
}


void ShModule::parseTerm(VmCode& code)
{
    parseFactor(code);
    while (parser.token >= tokMul && parser.token <= tokShr)
    {
        Token tok = parser.token;
        parser.next();
        ShType* left = code.topType();
        parseFactor(code);
        ShType* right = code.topType();
        if (left->isInt() && right->isInt())
        {
            if ((tok == tokShl || tok == tokShr) && PInteger(right)->isLargeInt())
                error("Right operand of a bit shift can not be large");
            OpCode op;
            switch (tok)
            {
                case tokMul: op = opMul; break;
                case tokDiv: op = opDiv; break;
                case tokMod: op = opMod; break;
                case tokAnd: op = opBitAnd; break;
                case tokShl: op = opBitShl; break;
                case tokShr: op = opBitShr; break;
                default: op = opInv;
            }
            code.genBinArithm(op, arithmResultType(PInteger(left), PInteger(right)));
        }
        
        // TODO: boolean ops: short evaluation (jumps)
        
        else
            error("Invalid operands for arithmetic operator");
    }
}


void ShModule::parseSimpleExpr(VmCode& code)
{
    parseTerm(code);
    while (parser.token >= tokPlus && parser.token <= tokXor)
    {
        Token tok = parser.token;
        parser.next();
        ShType* left = code.topType();
        parseTerm(code);
        ShType* right = code.topType();

        // Arithmetic
        if (left->isInt() && right->isInt())
        {
            OpCode op;
            switch (tok)
            {
                case tokPlus: op = opAdd; break;
                case tokMinus: op = opSub; break;
                case tokOr: op = opBitOr; break;
                case tokXor: op = opBitXor; break;
                default: op = opInv;
            }
            code.genBinArithm(op, arithmResultType(PInteger(left), PInteger(right)));
        }

        // TODO: boolean ops: short evaluation (jumps)
        
        // TODO: string/vector ops
        else
            error("Invalid operands for arithmetic operator");
    }
}


void ShModule::parseRelExpr(VmCode& code)
{
    parseSimpleExpr(code);
    if (parser.token >= tokCmpFirst && parser.token <= tokCmpLast)
    {
        OpCode op = OpCode(opCmpFirst + int(parser.token - tokCmpFirst));
        ShType* left = code.topType();
        parser.next();
        parseSimpleExpr(code);
        ShType* right = code.topType();
        if (!left->isCompatibleWith(right))
            error("Type mismatch in comparison: " + typeVsType(left, right));
        code.genComparison(op);
    }
}


void ShModule::parseSubrange(VmCode& code)
{
    parseRelExpr(code);
    if (parser.token == tokRange)
    {
        // Check bounds for left < right maybe? Or maybe not.
        ShType* left = code.topType();
        parser.next();
        parseRelExpr(code);
        ShType* right = code.topType();
        if (!left->isOrdinal() || !right->isOrdinal())
            error("Only ordinal types are allowed in subranges");
        if (!left->isCompatibleWith(right))
            error("Left and right values of a subrange must be compatible");
        if (POrdinal(left)->isLargeInt() || POrdinal(right)->isLargeInt())
            error("Large subrange bounds are not supported");
        code.genMkSubrange();
    }
}


ShValue ShModule::getConstExpr(ShType* typeHint)
{
    VmCode code(currentScope);
    parseExpr(code);
    ShValue result = code.runConstExpr();

    if (typeHint == NULL)
        typeHint = result.type;

    if (!typeHint->canAssign(result.type))
        error("Type mismatch in constant expression: " + typeVsType(typeHint, result.type));

    if (typeHint->isOrdinal() && result.type->isOrdinal()
        && !POrdinal(typeHint)->contains(result.value.large_))
            error("Value out of range");

    else if (typeHint->isString() && result.type->isChar())
        result = ShValue(queenBee->defaultStr,
            registerString(string(char(result.value.int_))));

    else if (result.type->isRange() && result.rangeMin() >= result.rangeMax())
        error("Invalid range");

    return result;
}


// --- TYPES --------------------------------------------------------------- //


ShType* ShModule::getDerivators(ShType* type)
{
    if (parser.token == tokLSquare)
    {
        parser.next();
        if (parser.token == tokRSquare)
        {
            type = type->deriveVectorType(currentScope);
            parser.next();
        }
        else if (parser.token == tokRange)
        {
            parser.next();
            parser.skip(tokRSquare, "]");
            if (!type->isOrdinal())
                error("Ranges apply only to ordinal types");
            type = POrdinal(type)->deriveRangeType(currentScope);
        }
        else
        {
            ShType* indexType = getType();
            parser.skip(tokRSquare, "]");
            if (!indexType->canBeArrayIndex())
                error(indexType->getDefinition() + " can't be used as array index");
            type = type->deriveArrayType(indexType, currentScope);
        }
        type = getDerivators(type);
    }
    return type;
}


ShType* ShModule::getType()
{
    ShType* type = NULL;
    ShBase* obj = getQualifiedName();
    if (obj->isTypeAlias())
        type = ((ShTypeAlias*)obj)->base;
    else if (obj->isType())
        type = (ShType*)obj;
    if (type == NULL)
        errorWithLoc("Expected type specifier");
    return getDerivators(type);
}


ShType* ShModule::getTypeOrNewIdent(string* ident)
{
    ident->clear();
    
    // remember the ident in case we get ENotFound so that we can signal
    // the caller this might be a declaration, not a type spec.
    if (parser.token == tokIdent)
        *ident = parser.strValue;   

    try
    {
        ShValue value = getConstExpr(NULL);
        if (value.type->isTypeRef())
            return (ShType*)value.value.ptr_;
        else if (value.type->isRange())
            return ((ShRange*)value.type)->base->deriveOrdinalFromRange(value, currentScope);
        else
            // TODO: give a better error message in case ident was a known one.
            // How? What if it was an expression?
            errorWithLoc("Type specification or new identifier expected");
    }
    catch (ENotFound& e)
    {
        // if this is a more complicated expression, just re-throw the exception
        // othrwise return NULL and thus indicate this was a new ident
        if (e.getEntry() != *ident)
            throw;
    }
    catch(EInvalidSubrange& e)
    {
        error(e.what());
    }
    return NULL;
}


// --- STATEMENTS/DEFINITIONS ---------------------------------------------- //


ShEnum* ShModule::parseEnumType()
{
    ShEnum* type = new ShEnum();
    currentScope->addAnonType(type);
    parser.skip(tokLParen, "(");
    while (1)
    {
        string ident = parser.getIdent();
        ShConstant* value = new ShConstant(ident, ShValue(type, type->nextValue()));
        addObject(value);
        type->registerConst(value);
        if (parser.skipIf(tokRParen))
            break;
        parser.skip(tokComma, ")");
    }
    type->finish();
    return type;
}


void ShModule::parseTypeDef()
{
    string ident;
    ShType* type;
    if (parser.skipIf(tokEnum))
    {
        ident = parser.getIdent();
        type = parseEnumType();
    }
    else
    {
        type = getType();
        ident = parser.getIdent();
        type = getDerivators(type);
    }
    addObject(new ShTypeAlias(ident, type));
}


void ShModule::parseVarConstDef(bool isVar)
{
    string ident;
    ShType* type = NULL;

    if (parser.skipIf(tokEnum))
    {
        ident = parser.getIdent();
        type = parseEnumType();
    }
    else
    {
        type = getTypeOrNewIdent(&ident);
        if (type != NULL)  // if not auto, derivators are possible after the ident
        {
            ident = parser.getIdent();
            type = getDerivators(type);
        }
    }

    parser.skip(tokAssign, "=");
    ShValue value = getConstExpr(type);
    if (type == NULL) // auto
        type = value.type;
    else
        value.type = type;
    if (isVar)
        addObject(new ShVariable(ident, type)); // TODO: initializer
    else
        addObject(new ShConstant(ident, value));
}


void ShModule::compile()
{
    try
    {
        currentScope = this;
        
        parser.next();
        
        if (parser.token == tokModule)
        {
            parser.next();
            string modName = parser.getIdent();
            if (strcasecmp(modName.c_str(), name.c_str()) != 0)
                error("Module name mismatch");
            setNamePleaseThisIsBadIKnow(modName);
            parser.skipSep();
        }

        while (parser.token != tokEof)
        {
            if (parser.skipIf(tokDef))
                parseTypeDef();
            else if (parser.skipIf(tokConst))
                parseVarConstDef(false);
            else if (parser.skipIf(tokVar))
                parseVarConstDef(true);
            else
                errorWithLoc("Expected definition or statement");
            parser.skipSep();
        }

        compiled = true;

#ifdef DEBUG
        queenBee->dump("");
        dump("");
#endif

    }
    catch(Exception& e)
    {
        fprintf(stderr, "%s\n", e.what().c_str());
    }
}


// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //


class _AtExit
{
public:
    ~_AtExit()
    {
        if (Base::objCount != 0)
            fprintf(stderr, "Internal: objCount = %d\n", Base::objCount);
        if (stralloc != 0)
            fprintf(stderr, "Internal: stralloc = %d\n", stralloc);
        if (FifoChunk::chunkCount != 0)
            fprintf(stderr, "Internal: chunkCount = %d\n", FifoChunk::chunkCount);
#ifdef SINGLE_THREADED
        vmStack.clear();
#endif
        if (stackimpl::stackAlloc != 0)
            fprintf(stderr, "Internal: stackAlloc = %d\n", stackimpl::stackAlloc);
    }
} _atexit;




int main()
{
    try
    {
        initLangObjs();
        
#ifdef XCODE
        ShModule module("../../src/z.sn");
#else
        ShModule module("z.sn");
#endif
        module.compile();
        
        doneLangObjs();
    }
    catch (Exception& e)
    {
        fprintf(stderr, "\n*** Exception: %s\n", e.what().c_str());
    }

    return 0;
}

