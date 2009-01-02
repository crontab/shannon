
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include "common.h"
#include "langobj.h"
#include "vm.h"
#include "codegen.h"


// ------------------------------------------------------------------------- //
// --- HIS MAJESTY THE COMPILER -------------------------------------------- //
// ------------------------------------------------------------------------- //


string typeVsType(ShType* a, ShType* b)
{
    return a->getDefinitionQ() + " vs. " + b->getDefinitionQ();
}


// --- EXPRESSION ---------------------------------------------------------- //

/*
    <nested-expr>, <typecast>, <ident>, <number>, <string>, <char>,
        true, false, null, compound-ctor
    <array-sel>, <fifo-sel>, <function-call>, <mute>
    -, not
    *, /, mod, as
    +, –
    ++
    ==, <>, != <, >, <=, >=, in, is
    and
    or, xor
    ..
    ,
*/

// TODO: execute constant code as much as possible (in VmCodeGen probably)

ShBase* ShModule::getQualifiedName()
{
    string ident = parser.getIdent();
    ShBase* obj = symbolScope->deepFind(ident);
    if (obj == NULL)
        errorNotFound(ident);
    string errIdent = ident;
    while (parser.token == tokPeriod)
    {
        if (!PType(obj)->isModule())
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


ShType* ShModule::getTypeExpr(bool anyObj)
{
    VmCodeGen tcode;
    parseExpr(tcode);
    bool cantEval = false;
    ShType* type = tcode.runTypeExpr(anyObj, &cantEval);
    if (cantEval)
        error("Expression can't be evaluated at compile time");
    return type;
}


ShType* ShModule::parseIfFunc(VmCodeGen& code)
{
    parser.skip(tokLParen, "(");
    if (!parseExpr(code, queenBee->defaultBool)->isBool())
        error("Boolean expression expected as first argument to if()");
    parser.skip(tokComma, ",");
    offs jumpFalseOffs = code.genForwardBoolJump(opJumpFalse);

    ShType* trueType = parseExpr(code);
    if (trueType->storageModel == stoVoid)
        error("Invalid argument type for the 'true' branch of if()");
    parser.skip(tokComma, ",");
    code.genPop();
    offs jumpOutOffs = code.genForwardJump();

    code.genResolveJump(jumpFalseOffs);
    ShType* falseType = parseExpr(code, trueType);
    if (!trueType->equals(falseType))
        error("Types of 'true' and 'false' branches for if() don't match");

    code.genResolveJump(jumpOutOffs);
    parser.skip(tokRParen, ")");
    return code.genTopType();
}


ShType* ShModule::parseAtom(VmCodeGen& code, bool isLValue)
{
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
        code.genLoadIntConst(queenBee->defaultInt, value);
    }

    else if (parser.token == tokLargeValue)
    {
        ularge value = parser.largeValue; // largeValue is unsigned int
        parser.next();
        code.genLoadLargeConst(queenBee->defaultLarge, value);
    }

    // string or char literal
    else if (parser.token == tokStrValue)
    {
        if (parser.strValue.size() == 1)
        {
            int value = (unsigned)parser.strValue[0];
            parser.next();
            code.genLoadIntConst(queenBee->defaultChar, value);
        }
        else
        {
            string s = parser.strValue;
            parser.next();
            code.genLoadVecConst(queenBee->defaultStr, registerString(s).c_bytes());
        }
    }

    // identifier
    else if (parser.token == tokIdent)
    {
        ShBase* obj = getQualifiedName();
        if (obj->isConstant())
        {
            ShConstant* c = (ShConstant*)obj;
            if (c->value.type->isTypeRef())
            {
                ShType* refType = PType(c->value.value.ptr_);
                if (parser.skipIf(tokLParen))
                    parseStaticCast(code, refType);
                else
                    code.genLoadTypeRef(getDerivators(refType));
            }
            else
                code.genLoadConst(c->value.type, c->value.value);
        }

        else if (obj->isVariable())
        {
            if (isLValue)
                code.genLoadVarRef((ShVariable*)obj);
            else
                code.genLoadVar((ShVariable*)obj);
        }
        
        // TODO: funcs
        else
            notImpl();
    }

    // typeof(...)
    else if (parser.skipIf(tokTypeOf))
    {
        // TODO: currently only works with const expressions, however,
        // it should be possible to get a type of a variable or an array item,
        // and finally a dynamic type of a state. The question is how. The 
        // problem of 'is' and 'as' is related.
        parser.skip(tokLParen, "(");
        code.genLoadTypeRef(getTypeExpr(true));
        parser.skip(tokRParen, ")");
    }

    // sizeof(...)
    else if (parser.skipIf(tokSizeOf))
    {
        parser.skip(tokLParen, "(");
        ShType* type = getTypeExpr(true);
        parser.skip(tokRParen, ")");
        // TODO: actual sizes for states (maybe also vectors/arrays? or len() is enough?)
        code.genLoadIntConst(queenBee->defaultInt, type->staticSize);
    }
    
    // true/false/null
    else if (parser.skipIf(tokTrue))
        code.genLoadIntConst(queenBee->defaultBool, 1);
    else if (parser.skipIf(tokFalse))
        code.genLoadIntConst(queenBee->defaultBool, 0);
//    else if (parser.skipIf(tokNull))
//        code.genLoadNull();

    // compound ctor (currently only vector)
    else if (parser.skipIf(tokLSquare))
        parseCompoundCtor(code);
    
    else if (!isLValue && parser.skipIf(tokIf))
        parseIfFunc(code);

    else
        errorWithLoc("Expression syntax");

    return code.genTopType();
}


ShType* ShModule::parseStaticCast(VmCodeGen& code, ShType* toType)
{
    code.resultTypeHint = toType;
    parseExpr(code);
    parser.skip(tokRParen, ")");
    ShType* fromType = code.genTopType();
    if (fromType->isOrdinal() && toType->isString())
        code.genIntToStr();
    else if (fromType->canStaticCastTo(toType))
        code.genStaticCast(toType);
    else
        error("Can't do static typecast from " + fromType->getDefinitionQ()
            + " to " + toType->getDefinitionQ());
    return toType;
}


ShType* ShModule::parseDesignator(VmCodeGen& code, bool isLValue)
{
    return parseAtom(code, isLValue);
}


ShType* ShModule::parseFactor(VmCodeGen& code)
{
    bool isNeg = parser.skipIf(tokMinus);
    ShType* resultType = parseDesignator(code, false);
    if (isNeg)
    {
        resultType = code.genTopType();
        if (!resultType->isInt())
            error("Invalid operand for arithmetic negation");
        code.genUnArithm(opNeg, PInteger(resultType));
    }
    return resultType;
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


ShType* ShModule::parseTerm(VmCodeGen& code)
{
    ShType* left = parseFactor(code);
    while (parser.token == tokMul || parser.token == tokDiv || parser.token == tokMod)
    {
        Token tok = parser.token;
        parser.next();
        ShType* right = parseFactor(code);
        if (left->isInt() && right->isInt())
        {
            left = arithmResultType(PInteger(left), PInteger(right));
            code.genBinArithm(tok == tokMul ? opMul
                : tok == tokDiv ? opDiv : opMod, PInteger(left));
        }
        else
            error("Invalid operands for arithmetic operator");
    }
    return left;
}


ShType* ShModule::parseArithmExpr(VmCodeGen& code)
{
    ShType* left = parseTerm(code);
    while (parser.token == tokPlus || parser.token == tokMinus)
    {
        Token tok = parser.token;
        parser.next();
        ShType* right = parseTerm(code);
        if (left->isInt() && right->isInt())
        {
            left = arithmResultType(PInteger(left), PInteger(right));
            code.genBinArithm(tok == tokPlus ? opAdd : opSub,
                PInteger(left));
        }
        else
            error("Invalid operands for arithmetic operator");
    }
    return left;
}


ShType* ShModule::parseSimpleExpr(VmCodeGen& code)
{
    ShType* left = parseArithmExpr(code);
    if (parser.token == tokCat)
    {
        parser.next();

        offs tmpOffset = 0;
        if (left->isVector())
            tmpOffset = code.genCopyToTempVec();
        else if (left->canBeArrayElement())
        {
            left = left->deriveVectorType();
            tmpOffset = code.genElemToVec(PVector(left));
        }
        else
            error("Invalid vector element type");
        do
        {
            ShType* right = parseArithmExpr(code);
            if (left->equals(right))
                code.genVecCat(tmpOffset);
            else if (PVector(left)->elementEquals(right))
                code.genVecElemCat(tmpOffset);
            else
                error("Operands of vector concatenation are incompatible");
        }
        while (parser.skipIf(tokCat));
    }
    return left;
}


ShType* ShModule::parseCompoundCtor(VmCodeGen& code)
{
    ShType* typeHint = code.resultTypeHint;
    if (typeHint != NULL && !typeHint->isVector())
        typeHint = NULL; // let the assignment parser decide
    if (parser.skipIf(tokRSquare))
    {
        code.genLoadVecConst(queenBee->defaultEmptyVec, emptystr);
        return queenBee->defaultEmptyVec;
    }
    else
    {
        // TODO: if we are constructing a vector with one element which is
        // to be concatenated with another vector, two tempvars are created,
        // which results in unnecessary copying of the first vector in
        // growNonPodVec(). For example:
        //   var str a[] = ['abc'] | b
        ShType* elemType = typeHint != NULL && typeHint->isVector() ?
            PVector(typeHint)->elementType : NULL;
        ShVector* vecType = NULL;
        offs tmpOffset = 0;
        while (1)
        {
            ShType* gotType = parseExpr(code, elemType);
            if (elemType == NULL)
                elemType = gotType;
            if (!elemType->canAssign(gotType))
                errorWithLoc("Type mismatch in vector constructor");
            if (vecType == NULL) // first item?
            {
                vecType = elemType->deriveVectorType();
                tmpOffset = code.genElemToVec(vecType);
            }
            else
                code.genVecElemCat(tmpOffset);
            if (parser.skipIf(tokRSquare))
                break;
            parser.skip(tokComma, "]");
        }
        return vecType;
    }
}


ShType* ShModule::parseRelExpr(VmCodeGen& code)
{
    ShType* left = parseSimpleExpr(code);
    if (parser.token >= tokCmpFirst && parser.token <= tokCmpLast)
    {
        OpCode op = OpCode(opCmpFirst + int(parser.token - tokCmpFirst));
        parser.next();
        ShType* right = parseSimpleExpr(code);
        if (left->canCompareWith(right)
                || ((op == opEQ || op == opNE) && left->canCheckEq(right)))
        {
            code.genComparison(op);
            left = code.genTopType();
        }
        else
            error("Type mismatch in comparison: " + typeVsType(left, right));
    }
    return left;
}


ShType* ShModule::parseNotLevel(VmCodeGen& code)
{
    bool isNot = parser.skipIf(tokNot);
    ShType* type = parseRelExpr(code);
    if (isNot)
    {
        if (type->isInt())
            code.genBitNot(PInteger(type));
        else if (type->isBool())
            code.genBoolNot();
        else
            error("Boolean or integer expression expected after 'not'");
    }
    return type;
}


ShType* ShModule::parseAndLevel(VmCodeGen& code)
{
    ShType* left = parseNotLevel(code);
    if (left->isBool())
    {
        if (parser.skipIf(tokAnd))
        {
            offs saveOffset = code.genForwardBoolJump(opJumpAnd);
            ShType* right = parseAndLevel(code);
            if (!right->isBool())
                error("Boolean expression expected after 'and'");
            code.genResolveJump(saveOffset);
        }
    }
    else if (left->isInt())
    {
        while (parser.token == tokShl || parser.token == tokShr || parser.token == tokAnd)
        {
            Token tok = parser.token;
            parser.next();
            ShType* right = parseNotLevel(code);
            if (right->isInt())
            {
                if ((tok == tokShl || tok == tokShr) && PInteger(right)->isLargeInt())
                    error("Right operand of a bit shift can not be large");
                left = arithmResultType(PInteger(left), PInteger(right));
                code.genBinArithm(tok == tokShl ? opBitShl
                    : tok == tokShr ? opBitShr : opBitAnd, PInteger(left));
            }
            else
                error("Invalid operands for bitwise operator");
        }
    }
    return left;
}


ShType* ShModule::parseOrLevel(VmCodeGen& code)
{
    ShType* left = parseAndLevel(code);
    if (left->isBool())
    {
        if (parser.skipIf(tokOr))
        {
            offs saveOffset = code.genForwardBoolJump(opJumpOr);
            ShType* right = parseOrLevel(code);
            if (!right->isBool())
                error("Boolean expression expected after 'or'");
            code.genResolveJump(saveOffset);
        }
        else if (parser.skipIf(tokXor))
        {
            ShType* right = parseOrLevel(code);
            if (!right->isBool())
                error("Boolean expression expected after 'xor'");
            code.genBoolXor();
        }
    }
    else if (left->isInt())
    {
        while (parser.token == tokOr || parser.token == tokXor)
        {
            Token tok = parser.token;
            parser.next();
            ShType* right = parseAndLevel(code);
            if (right->isInt())
            {
                left = arithmResultType(PInteger(left), PInteger(right));
                code.genBinArithm(tok == tokOr ? opBitOr : opBitXor,
                    PInteger(left));
            }
            else
                error("Invalid operands for bitwise operator");
        }
    }
    return left;
}


ShType* ShModule::parseSubrange(VmCodeGen& code)
{
    ShType* left = parseOrLevel(code);
    if (parser.token == tokRange)
    {
        // Check bounds for left < right maybe? Or maybe not.
        parser.next();
        ShType* right = parseOrLevel(code);
        if (!left->isOrdinal() || !right->isOrdinal())
            error("Only ordinal types are allowed in subranges");
        if (!left->equals(right))
            error("Left and right values of a subrange must be compatible");
        if (POrdinal(left)->isLargeInt() || POrdinal(right)->isLargeInt())
            error("Large subrange bounds are not supported");
        code.genMkSubrange();
        left = code.genTopType();
    }
    return left;
}


ShType* ShModule::parseExpr(VmCodeGen& code, ShType* resultType)
{
    code.resultTypeHint = resultType;
    ShType* topType = parseExpr(code);

    // see if this is an elem-to-vector assignment
    bool hintIsVec = resultType != NULL && resultType->isVector();
    if (hintIsVec && resultType->canAssign(topType) && PVector(resultType)->elementEquals(topType))
        code.genElemToVec(PVector(resultType));

    // ordinal typecast, if necessary, so that a constant has a proper type
    else if (resultType != NULL && resultType->isOrdinal()
            && !resultType->equals(topType) && topType->canStaticCastTo(resultType))
        code.genStaticCast(resultType);

    return topType;
}


void ShModule::getConstExpr(ShType* typeHint, ShValue& result)
{
    VmCodeGen code;

    parseExpr(code, typeHint);
    code.runConstExpr(result);
    
    if (result.type == NULL)
        error("Expression can't be evaluated at compile time");

    if (typeHint == NULL)
        typeHint = result.type;
    else
    {
        if (typeHint->isVector() && result.type->isEmptyVec())
            // empty vectors are always of void type, so simply pass the hint type
            result.type = typeHint;
    }

    if (!typeHint->canAssign(result.type))
        error("Type mismatch in constant expression: " + typeVsType(typeHint, result.type));

    // even without a hint a constant can be out of range of it's own type
    // e.g. char(257), so we check the range anyway:
    if (typeHint->isOrdinal() && result.type->isOrdinal()
        && !POrdinal(typeHint)->contains(result))
            error("Value out of range");

    else if (result.type->isRange() && result.rangeMin() >= result.rangeMax())
        error("Invalid range");

}


// --- TYPES --------------------------------------------------------------- //


ShType* ShModule::getDerivators(ShType* type)
{
    if (parser.skipIf(tokLSquare))
    {
        if (parser.skipIf(tokRSquare))
        {
            if (!type->canBeArrayElement())
                error("Invalid vector element type");
            type = type->deriveVectorType();
        }
        else if (parser.skipIf(tokRange))
        {
            parser.skip(tokRSquare, "]");
            if (!type->isOrdinal())
                error("Ranges apply only to ordinal types");
            type = POrdinal(type)->deriveRangeType();
        }
        else
        {
            ShType* indexType = getTypeExpr(false);
            if (indexType == NULL)
                errorWithLoc("Type specification expected");
            parser.skip(tokRSquare, "]");
            if (!indexType->canBeArrayIndex())
                error(indexType->getDefinition() + " can't be used as array index");
            type = type->deriveArrayType(indexType);
        }
        type = getDerivators(type);
    }
    return type;
}


ShType* ShModule::getTypeOrNewIdent(string* ident)
{
    ident->clear();
    
    // Try to guess if this is a new ident or a type spec
    if (parser.token == tokIdent)
    {
        *ident = parser.strValue;
        ShBase* obj = symbolScope->deepFind(*ident);
        // In one of the previous attempts to implement this guessing we were calling
        // full expression parsing just to catch any exceptions and analyze them.
        // Unfortunately excpetion throwing/catching in C++ is too expensive, so we'd
        // better do something rather heuristic here:
        if (obj == NULL || !obj->isConstant())
        {
            parser.next();
            return NULL;
        }
    }
    
    return getTypeExpr(false);
}


// --- STATEMENTS/DEFINITIONS ---------------------------------------------- //


ShEnum* ShModule::parseEnumType()
{
    ShEnum* type = new ShEnum();
    varScope->addAnonType(type);
    parser.skip(tokLParen, "(");
    while (1)
    {
        string ident = parser.getIdent();
        int nextVal = type->nextValue();
        if (nextVal == 256)
            error("Enum constant has just hit the ceilinig, man.");
        ShConstant* value = new ShConstant(ident, type, nextVal);
        varScope->addConstant(value, symbolScope);
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
        type = getTypeExpr(false);
        if (type == NULL)
            errorWithLoc("Type specification expected");
        ident = parser.getIdent();
        type = getDerivators(type);
    }
    varScope->addTypeAlias(ident, type, symbolScope);
    parser.skipSep();
}


void ShModule::parseVarConstDef(bool isVar, VmCodeGen& code)
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

    if (!parser.skipIf(tokAssign))
        errorWithLoc((isVar ? "Variable" : "Constant") + string(" initialization expected"));

    if (isVar)
    {
        ShType* exprType = parseExpr(code, type);
        if (type == NULL) // auto
            type = exprType;
        else if (!type->canAssign(exprType))
            error("Type mismatch in variable initialization");
        ShVariable* var = new ShVariable(ident, type);
        varScope->addVariable(var, symbolScope);
        code.genInitVar(var);
    }
    else
    {
        ShValue value;
        getConstExpr(type, value);
        if (type == NULL) // auto
            type = value.type;
        varScope->addConstant(new ShConstant(ident, value), symbolScope);
    }

    parser.skipSep();
}


void ShModule::parseEcho(VmCodeGen& code)
{
    if (parser.token != tokSep)
    {
        while (1)
        {
            parseExpr(code);
            code.genEcho();
            if (parser.skipIf(tokComma))
                code.genOther(opEchoSp);
            else
                break;
        }
    }
    code.genOther(opEchoLn);
    parser.skipSep();
}


void ShModule::parseAssert(VmCodeGen& code)
{
    ShType* type = parseExpr(code);
    if (!type->isBool())
        error("Boolean expression expected for assertion");
    code.genAssert(parser);
    parser.skipSep();
}


void ShModule::parseOtherStatement(VmCodeGen& code)
{
    ShType* type = parseDesignator(code, true);
    if (parser.skipIf(tokAssign))
    {
        if (!type->isReference())
            error("L-value expected in assignment");
        parseExpr(code, PReference(type)->base);
        code.genStore();
    }
    else
        error("Definition or statement expected");
    parser.skipSep();
}


VmCodeGen nullGen;


void ShModule::parseBlock(VmCodeGen& code)
{
    while (!parser.skipIf(tokBlockEnd))
    {
        if (options.linenumInfo)
            code.genLinenum(parser);

        if (parser.skipIf(tokSep))
            ;
        else if (parser.skipIf(tokDef))
            parseTypeDef();
        else if (parser.skipIf(tokConst))
            parseVarConstDef(false, code);
        else if (parser.skipIf(tokVar))
            parseVarConstDef(true, code);
        else if (parser.skipIf(tokEcho))
            parseEcho(options.enableEcho ? code : nullGen);
        else if (parser.skipIf(tokAssert))
            parseAssert(options.enableAssert ? code : nullGen);
        else if (parser.skipIf(tokBegin))
        {
            parser.skipBlockBegin();
            enterBlock(code);
        }
        else if (parser.skipIf(tokIf))
            parseIf(code, tokIf);
        else if (parser.token == tokElse)
            error("Misplaced 'else'");
        else if (parser.token == tokElif)
            error("Misplaced 'elif'");
        else
            parseOtherStatement(code);
        code.genFinalizeTemps();
    }
}


void ShModule::enterBlock(VmCodeGen& code)
{
    // The SymScope object is temporary; the real objects (types, vars, etc)
    // are registered with the outer scope: either module or state. Through
    // the temp symbol scope however, we know which vars to finalize when 
    // exiting the block.
    ShSymScope tempSymScope(symbolScope);
    symbolScope = &tempSymScope;
    parseBlock(code);
    symbolScope = tempSymScope.parent;
    tempSymScope.finalizeVars(code);
}


void ShModule::parseIf(VmCodeGen& code, Token tok)
{
    offs endJump = -1;
    if (tok != tokElse)
    {
        ShType* type = parseExpr(code, queenBee->defaultBool);
        if (!type->isBool())
            error("Boolean expression expected");
        endJump = code.genForwardJump(opJumpFalse);
    }

    parser.skipBlockBegin();
    enterBlock(code);

    if (tok != tokElse && (parser.token == tokElse || parser.token == tokElif))
    {
        offs t = code.genForwardJump(opJump);
        code.genResolveJump(endJump);
        endJump = t;
        Token saveTok = parser.token;
        parser.next();
        parseIf(code, saveTok);
    }

    if (endJump != -1)
        code.genResolveJump(endJump);
}


bool ShModule::compile()
{
    try
    {
        try
        {
            parser.next();
            
            if (parser.token == tokModule)
            {
                parser.next();
                string modName = parser.getIdent();
                if (strcasecmp(modName.c_str(), name.c_str()) != 0)
                    error("Module name mismatch");
                setNamePleaseThisIsWrongIKnow(modName);
                parser.skipSep();
            }

            varScope = &modLocalScope;
            parseBlock(*codegen);
            
            symbolScope->finalizeVars(*codegen);

            parser.skip(tokEof, "<EOF>");
        }
        catch (EDuplicate& e)
        {
            error(e);
        }
        catch(EInvalidSubrange& e)
        {
            error(e.what());
        }

        setupRuntime();

#ifdef DEBUG
        queenBee->dump("");
        dump("");
#endif

    }
    catch(Exception& e)
    {
        fprintf(stderr, "%s\n", e.what().c_str());
        nullGen.clear();
        return false;
    }

    nullGen.clear();
    return true;
}


// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //



class _AtExit
{
public:
    ~_AtExit()
    {
        doneLangObjs();

        if (Base::objCount != 0)
            fprintf(stderr, "Internal: objCount = %d\n", Base::objCount);
        if (stralloc != 0)
            fprintf(stderr, "Internal: stralloc = %d\n", stralloc);
        if (FifoChunk::chunkCount != 0)
            fprintf(stderr, "Internal: chunkCount = %d\n", FifoChunk::chunkCount);
        stk.clear();
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
        ShModule module("/Users/hovik/Projects/Shannon/src/z.sn");
#else
        ShModule module("z.sn");
#endif
        module.compile();

        if (module.compiled)
        {
            // TODO: exec mains for all used modules
            module.execute();
            if (!stk.empty())
                fatal(CRIT_FIRST + 54, "[VM] Stack in undefined state after execution");
        }
        else
            return 1;
    }
    catch (Exception& e)
    {
        fprintf(stderr, "\n*** Exception: %s\n", e.what().c_str());
        return 2;
    }
    catch (int)
    {
        // run-time error
        return 3;
    }

    return 0;
}

