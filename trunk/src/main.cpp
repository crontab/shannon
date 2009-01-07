
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


struct CompilerOptions
{
    bool enableEcho;
    bool enableAssert;
    bool linenumInfo;

    CompilerOptions()
        : enableEcho(true), enableAssert(true), linenumInfo(false)  { }
};


class ShCompiler: public Base
{
    struct LoopInfo
    {
        // jump target for 'continue'
        offs continueTarget;
        // unresolved forward jumps produced by 'break'
        PodStack<offs> breakJumps;
        // nested symbol scopes to be finalized in case of 'break' or 'continue'
        PodStack<ShSymScope*> symScopes;
    };
    
    struct FunctionInfo
    {
        // unresolved forward jumps produced by 'return'
        PodStack<offs> returnJumps;
        // nested symbol scopes to be finalized in case of 'return'
        PodStack<ShSymScope*> symScopes;
    };

    Parser& parser;
    ShModule& module;

    VmCodeGen mainCodeGen;
    VmCodeGen nullCode;
    CompilerOptions options;

    ShSymScope* currentSymbolScope; // for symbols only; replaced for every nested block
    ShStateBase* currentStateScope; // module or state
    LoopInfo* topLoop;
    FunctionInfo* topFunction;
    VmCodeGen* codegen;

    VmCodeGen* replaceCodeGen(VmCodeGen* c)
            { VmCodeGen* t = codegen; codegen = c; return t; }
    LoopInfo* replaceTopLoop(LoopInfo* l)
            { LoopInfo* t = topLoop; topLoop = l; return t; }
    FunctionInfo* replaceTopFunction(FunctionInfo* f)
            { FunctionInfo* t = topFunction; topFunction = f; return t; }

    void error(const string& msg)           { parser.error(msg); }
    void error(EDuplicate& e);
    void error(const char* msg)             { parser.error(msg); }
    void errorWithLoc(const string& msg)    { parser.errorWithLoc(msg); }
    void errorWithLoc(const char* msg)      { parser.errorWithLoc(msg); }
    void errorNotFound(const string& msg)   { parser.errorNotFound(msg); }
    void notImpl()                          { error("Feature not implemented"); }

    ShBase* getQualifiedName();
    ShType* getDerivators(ShType*);
    ShType* getTypeOrNewIdent(string* strToken);
    ShType* getTypeExpr(bool anyObj);
    ShType* parseCompoundCtor();
    ShType* parseIfFunc();
    ShType* parseStaticCast(ShType* toType);
    ShType* parseFunctionCall(ShFunction*);
    ShType* parseAtom();
    ShType* parseDesignator(bool isLValue);
    ShInteger* arithmResultType(ShInteger* left, ShInteger* right);
    ShType* parseFactor();
    ShType* parseTerm();
    ShType* parseArithmExpr();
    ShType* parseSimpleExpr();
    ShType* parseRelExpr();
    ShType* parseNotLevel();
    ShType* parseAndLevel();
    ShType* parseOrLevel();
    ShType* parseSubrange();
    ShType* parseBoolExpr()                 { return parseOrLevel(); }
    ShType* parseExpr()                     { return parseSubrange(); }
    ShType* parseExpr(ShType* resultType);
    void getConstExpr(ShType* typeHint, ShValue& result, bool allowRange);

    void parseModuleHeader();
    ShEnum* parseEnumType(const string&);
    void parseTypeDef();
    void parseVarConstDef(bool isVar);
    void parseEcho(VmCodeGen*);
    void parseAssert(VmCodeGen*);
    void parseOtherStatement();
    void parseIf(Token);
    void parseWhile();
    void parseBreakCont(bool);
    void parseCase();
    void parseBlock();
    void enterBlock();
    void parseFunctionBody(ShFunction* funcType);

public:
    ShCompiler(Parser& iParser, ShModule& iModule);
    bool compile();
};


ShCompiler::ShCompiler(Parser& iParser, ShModule& iModule)
    : parser(iParser), module(iModule), mainCodeGen(&module),
      nullCode(&module), currentSymbolScope(NULL), 
      currentStateScope(NULL), topLoop(NULL), topFunction(NULL),
      codegen(&mainCodeGen)  { }


string typeVsType(ShType* a, ShType* b)
{
    return a->getDefinitionQ() + " vs. " + b->getDefinitionQ();
}


// --- EXPRESSION ---------------------------------------------------------- //

/*
    <nested-expr>, <typecast>, <ident>, <number>, <string>, <char>,
        true, false, null, compound-ctor
    <array-sel>, <function-call>, <mute>
    -
    *, /, mod, as
    +, –
    |
    ==, <>, != <, >, <=, >=, in, is
    not
    and
    or, xor
    ..
*/


void ShCompiler::error(EDuplicate& e)
{
    parser.error("'" + e.entry + "' is already defined within this scope");
}


// TODO: execute constant code as much as possible (in VmCodeGen probably)

ShBase* ShCompiler::getQualifiedName()
{
    string ident = parser.getIdent();
    ShBase* obj = currentSymbolScope->deepFind(ident);
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
        errIdent += '.' + ident;
        obj = scope->find(ident);
        if (obj == NULL)
            errorNotFound(errIdent);
    }
    return obj;
}


ShType* ShCompiler::getTypeExpr(bool anyObj)
{
    VmCodeGen tcode(NULL);
    VmCodeGen* saveCodeGen = replaceCodeGen(&tcode);
    parseExpr();
    ShType* type = tcode.runTypeExpr(anyObj);
    replaceCodeGen(saveCodeGen);
    return type;
}


ShType* ShCompiler::parseIfFunc()
{
    parser.skip(tokLParen, "(");
    if (!parseExpr(queenBee->defaultBool)->isBool())
        error("Boolean expression expected as first argument to if()");
    parser.skip(tokComma, ",");
    offs jumpFalseOffs = codegen->genForwardBoolJump(opJumpFalse);

    ShType* trueType = parseExpr();
    if (trueType->storageModel == stoVoid)
        error("Invalid argument type for the 'true' branch of if()");
    parser.skip(tokComma, ",");
    codegen->genPop();
    offs jumpOutOffs = codegen->genForwardJump();

    codegen->genResolveJump(jumpFalseOffs);
    ShType* falseType = parseExpr(trueType);
    if (!trueType->equals(falseType))
        error("Types of 'true' and 'false' branches for if() don't match");

    codegen->genResolveJump(jumpOutOffs);
    parser.skip(tokRParen, ")");
    return codegen->genTopType();
}


ShType* ShCompiler::parseAtom()
{
    if (parser.skipIf(tokLParen))
    {
        parseExpr();
        parser.skip(tokRParen, ")");
    }

    // numeric literal
    else if (parser.token == tokIntValue)
    {
        large value = parser.intValue; // intValue is unsigned int
        parser.next();
        if (!queenBee->defaultInt->contains(value))
            error("Value out of range (use the 'L' suffix for large consts)");
        codegen->genLoadIntConst(queenBee->defaultInt, value);
    }

    else if (parser.token == tokLargeValue)
    {
        ularge value = parser.largeValue; // largeValue is unsigned int
        parser.next();
        codegen->genLoadLargeConst(queenBee->defaultLarge, value);
    }

    // string or char literal
    else if (parser.token == tokStrValue)
    {
        if (parser.strValue.size() == 1)
        {
            int value = (unsigned)parser.strValue[0];
            parser.next();
            codegen->genLoadIntConst(queenBee->defaultChar, value);
        }
        else
        {
            string s = module.registerString(parser.strValue);
            parser.next();
            codegen->genLoadVecConst(queenBee->defaultStr, s.c_bytes());
        }
    }

    // identifier
    else if (parser.token == tokIdent)
    {
        ShBase* obj = getQualifiedName();
        if (obj->isDefinition())
            codegen->genLoadConst(PDefinition(obj)->value.type,
                PDefinition(obj)->value.value);

        else if (obj->isVariable())
            // this opcode can be undone or modified later depending on 
            // whether this is an assignment or just R-value
            codegen->genLoadVarRef(PVariable(obj));
        
        else
            errorWithLoc("Error in expression");
    }

    // typeof(...)
    else if (parser.skipIf(tokTypeOf))
    {
        // TODO: currently only works with const expressions, however,
        // it should be possible to get a type of a variable or an array item,
        // and finally a dynamic type of a state. The question is how. The 
        // problem of 'is' and 'as' is probably related.
        parser.skip(tokLParen, "(");
        codegen->genLoadTypeRef(getTypeExpr(true));
        parser.skip(tokRParen, ")");
    }

    // sizeof(...)
    else if (parser.skipIf(tokSizeOf))
    {
        parser.skip(tokLParen, "(");
        ShType* type = getTypeExpr(true);
        parser.skip(tokRParen, ")");
        // TODO: actual sizes for states (maybe also vectors/arrays? or len() is enough?)
        codegen->genLoadIntConst(queenBee->defaultInt, type->staticSize);
    }
    
    // true/false/null
    else if (parser.skipIf(tokTrue))
        codegen->genLoadIntConst(queenBee->defaultBool, 1);
    else if (parser.skipIf(tokFalse))
        codegen->genLoadIntConst(queenBee->defaultBool, 0);
//    else if (parser.skipIf(tokNull))
//        codegen->genLoadNull();

    // compound ctor (currently only vector)
    else if (parser.skipIf(tokLSquare))
        parseCompoundCtor();
    
    else if (parser.skipIf(tokIf))
        parseIfFunc();

    // TODO: anonymous enum

    else
        errorWithLoc("Expression syntax");

    return codegen->genTopType();
}


ShType* ShCompiler::parseStaticCast(ShType* toType)
{
    codegen->resultTypeHint = toType;
    parseExpr();
    parser.skip(tokRParen, ")");
    ShType* fromType = codegen->genTopType();
    if (fromType->isOrdinal() && toType->isString())
        codegen->genIntToStr();
    else if (fromType->canStaticCastTo(toType))
        codegen->genStaticCast(toType);
    else
        error("Can't do static typecast from " + fromType->getDefinitionQ()
            + " to " + toType->getDefinitionQ());
    return toType;
}


ShType* ShCompiler::parseFunctionCall(ShFunction* func)
{
    for (int i = 0; i < func->args.size(); i++)
    {
        ShType* argType = func->args[i]->type;
        ShType* exprType = parseExpr(argType);
        if (!argType->canAssign(exprType))
            error("Type mismatch for argument #" + itostring(i + 1)
                + " in call to " + func->name);
    }
    parser.skip(tokRParen, ")");
    codegen->genCall(func);
    return func->returnVar->type;
}


ShType* ShCompiler::parseDesignator(bool isLValue)
{
    ShType* type = parseAtom();

    if (type->isReference())
    {
        if (parser.isAssignment())
        {
            if (!isLValue)
                error("Misplaced assignment in expression");
            return type;
        }
        else
            type = codegen->genDerefVar();
    }

    else if (type->isTypeRef() && codegen->genTopIsValue())
    {
        ShType* refType = codegen->genUndoTypeRef();
        if (parser.skipIf(tokLParen))
        {
            if (refType->isFunction())
                type = parseFunctionCall(PFunction(refType));
            else
                type = parseStaticCast(refType);
        }
        else
            type = codegen->genLoadTypeRef(getDerivators(refType));
    }

    return type;
}


ShType* ShCompiler::parseFactor()
{
    bool isNeg = parser.skipIf(tokMinus);
    ShType* resultType = parseDesignator(false);
    if (isNeg)
    {
        resultType = codegen->genTopType();
        if (!resultType->isInt())
            error("Invalid operand for arithmetic negation");
        codegen->genUnArithm(opNeg, PInteger(resultType));
    }
    return resultType;
}


ShInteger* ShCompiler::arithmResultType(ShInteger* left, ShInteger* right)
{
    if (left->isLargeInt() != right->isLargeInt())
        error("Mixing int and large: typecast needed (or 'L' with numbers)");
    ShInteger* resultType = PInteger(left);
    if (PInteger(right)->rangeIsGreaterOrEqual(PInteger(left)))
        resultType = PInteger(right);
    return resultType;
}


ShType* ShCompiler::parseTerm()
{
    ShType* left = parseFactor();
    while (parser.token == tokMul || parser.token == tokDiv || parser.token == tokMod)
    {
        Token tok = parser.token;
        parser.next();
        ShType* right = parseFactor();
        if (left->isInt() && right->isInt())
        {
            left = arithmResultType(PInteger(left), PInteger(right));
            codegen->genBinArithm(tok == tokMul ? opMul
                : tok == tokDiv ? opDiv : opMod, PInteger(left));
        }
        else
            error("Invalid operands for arithmetic operator");
    }
    return left;
}


ShType* ShCompiler::parseArithmExpr()
{
    ShType* left = parseTerm();
    while (parser.token == tokPlus || parser.token == tokMinus)
    {
        Token tok = parser.token;
        parser.next();
        ShType* right = parseTerm();
        if (left->isInt() && right->isInt())
        {
            left = arithmResultType(PInteger(left), PInteger(right));
            codegen->genBinArithm(tok == tokPlus ? opAdd : opSub,
                PInteger(left));
        }
        else
            error("Invalid operands for arithmetic operator");
    }
    return left;
}


ShType* ShCompiler::parseSimpleExpr()
{
    ShType* left = parseArithmExpr();
    if (parser.token == tokCat)
    {
        parser.next();

        offs tmpOffset = 0;
        if (left->isVector())
            tmpOffset = codegen->genCopyToTempVec();
        else if (left->canBeArrayElement())
        {
            left = left->deriveVectorType();
            tmpOffset = codegen->genElemToVec(PVector(left));
        }
        else
            error("Invalid vector element type");
        do
        {
            ShType* right = parseArithmExpr();
            if (left->equals(right))
                codegen->genVecCat(tmpOffset);
            else if (PVector(left)->elementEquals(right))
                codegen->genVecElemCat(tmpOffset);
            else
                error("Operands of vector concatenation are incompatible");
        }
        while (parser.skipIf(tokCat));
    }
    return left;
}


ShType* ShCompiler::parseCompoundCtor()
{
    ShType* typeHint = codegen->resultTypeHint;
    if (typeHint != NULL && !typeHint->isVector())
        typeHint = NULL; // let the assignment parser decide
    if (parser.skipIf(tokRSquare))
    {
        codegen->genLoadVecConst(queenBee->defaultEmptyVec, emptystr);
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
            ShType* gotType = parseExpr(elemType);
            if (elemType == NULL)
                elemType = gotType;
            if (!elemType->canAssign(gotType))
                errorWithLoc("Type mismatch in vector constructor");
            if (vecType == NULL) // first item?
            {
                vecType = elemType->deriveVectorType();
                tmpOffset = codegen->genElemToVec(vecType);
            }
            else
                codegen->genVecElemCat(tmpOffset);
            if (parser.skipIf(tokRSquare))
                break;
            parser.skip(tokComma, "]");
        }
        return vecType;
    }
}


ShType* ShCompiler::parseRelExpr()
{
    ShType* left = parseSimpleExpr();
    if (parser.token >= tokCmpFirst && parser.token <= tokCmpLast)
    {
        OpCode op = OpCode(opCmpFirst + int(parser.token - tokCmpFirst));
        parser.next();
        ShType* right = parseSimpleExpr();
        if (left->canCompareWith(right)
                || ((op == opEQ || op == opNE) && left->canCheckEq(right)))
        {
            codegen->genComparison(op);
            left = codegen->genTopType();
        }
        else
            error("Type mismatch in comparison: " + typeVsType(left, right));
    }
    return left;
}


ShType* ShCompiler::parseNotLevel()
{
    bool isNot = parser.skipIf(tokNot);
    ShType* type = parseRelExpr();
    if (isNot)
    {
        if (type->isInt())
            codegen->genBitNot(PInteger(type));
        else if (type->isBool())
            codegen->genBoolNot();
        else
            error("Boolean or integer expression expected after 'not'");
    }
    return type;
}


ShType* ShCompiler::parseAndLevel()
{
    ShType* left = parseNotLevel();
    if (left->isBool())
    {
        if (parser.skipIf(tokAnd))
        {
            offs saveOffset = codegen->genForwardBoolJump(opJumpAnd);
            ShType* right = parseAndLevel();
            if (!right->isBool())
                error("Boolean expression expected after 'and'");
            codegen->genResolveJump(saveOffset);
        }
    }
    else if (left->isInt())
    {
        while (parser.token == tokShl || parser.token == tokShr || parser.token == tokAnd)
        {
            Token tok = parser.token;
            parser.next();
            ShType* right = parseNotLevel();
            if (right->isInt())
            {
                if ((tok == tokShl || tok == tokShr) && right->isLargeInt())
                    error("Right operand of a bit shift can not be large");
                left = arithmResultType(PInteger(left), PInteger(right));
                codegen->genBinArithm(tok == tokShl ? opBitShl
                    : tok == tokShr ? opBitShr : opBitAnd, PInteger(left));
            }
            else
                error("Invalid operands for bitwise operator");
        }
    }
    return left;
}


ShType* ShCompiler::parseOrLevel()
{
    ShType* left = parseAndLevel();
    if (left->isBool())
    {
        if (parser.skipIf(tokOr))
        {
            offs saveOffset = codegen->genForwardBoolJump(opJumpOr);
            ShType* right = parseOrLevel();
            if (!right->isBool())
                error("Boolean expression expected after 'or'");
            codegen->genResolveJump(saveOffset);
        }
        else if (parser.skipIf(tokXor))
        {
            ShType* right = parseOrLevel();
            if (!right->isBool())
                error("Boolean expression expected after 'xor'");
            codegen->genBoolXor();
        }
    }
    else if (left->isInt())
    {
        while (parser.token == tokOr || parser.token == tokXor)
        {
            Token tok = parser.token;
            parser.next();
            ShType* right = parseAndLevel();
            if (right->isInt())
            {
                left = arithmResultType(PInteger(left), PInteger(right));
                codegen->genBinArithm(tok == tokOr ? opBitOr : opBitXor,
                    PInteger(left));
            }
            else
                error("Invalid operands for bitwise operator");
        }
    }
    return left;
}


ShType* ShCompiler::parseSubrange()
{
    ShType* left = parseOrLevel();
    if (parser.token == tokRange)
    {
        // Check bounds for left < right maybe? Or maybe not.
        parser.next();
        ShType* right = parseOrLevel();
        if (!left->isOrdinal() || !right->isOrdinal())
            error("Only ordinal types are allowed in subranges");
        if (!left->equals(right))
            error("Left and right values of a subrange must be compatible");
        if (left->isLargeInt() || right->isLargeInt())
            error("Large subrange bounds are not supported");
        codegen->genMkSubrange();
        left = codegen->genTopType();
    }
    return left;
}


ShType* ShCompiler::parseExpr(ShType* typeHint)
{
    codegen->resultTypeHint = typeHint;
    ShType* topType = parseExpr();

    // see if this is an elem-to-vector assignment
    if (typeHint != NULL)
    {
        if (typeHint->isVector() && typeHint->canAssign(topType)
                && PVector(typeHint)->elementEquals(topType))
            codegen->genElemToVec(PVector(typeHint));

        // ordinal typecast, if necessary, so that a constant has a proper type
        else if (typeHint->isOrdinal() && !typeHint->equals(topType)
                && topType->canStaticCastTo(typeHint))
            codegen->genStaticCast(typeHint);
    }

    return topType;
}


void ShCompiler::getConstExpr(ShType* typeHint, ShValue& result, bool allowRange)
{
    VmCodeGen tcode(NULL);
    VmCodeGen* saveCodeGen = replaceCodeGen(&tcode);

    parseExpr(typeHint);
    tcode.runConstExpr(result);

    if (result.type == NULL)
        error("Expression can't be evaluated at compile time");

    if (typeHint == NULL)
        typeHint = result.type;
    else
    {
         // empty vectors are always of void type, so simply pass the hint type
        if (typeHint->isVector() && result.type->isEmptyVec())
            result.type = typeHint;

        // some parts of the compiler allow compatible ranges on the right (see
        // parseCase() for example)
        if (allowRange && result.type->isRange()
            && typeHint->canAssign(PRange(result.type)->base))
                typeHint = result.type;
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

    replaceCodeGen(saveCodeGen);
}


// --- TYPES --------------------------------------------------------------- //


ShType* ShCompiler::getDerivators(ShType* type)
{
    // vectors/arrays/sets
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

    // functions
    else if (parser.skipIf(tokLParen))
    {
        if (currentSymbolScope != &module)
            error("Functions/states can be defined only at module level");
        ShFunction* funcType = new ShFunction(type, currentSymbolScope);
        codegen->hostScope->addAnonType(funcType);
        if (!parser.skipIf(tokRParen))
        {
            while (1)
            {
                ShType* argType = getTypeExpr(false);
                funcType->addArgument(parser.getIdent(), argType);
                if (parser.skipIf(tokRParen))
                    break;
                parser.skip(tokComma, ")");
            }
        }
        funcType->finishArguments();
        parseFunctionBody(funcType);
        type = funcType;
    }
    return type;
}


ShType* ShCompiler::getTypeOrNewIdent(string* ident)
{
    ident->clear();
    
    // Try to guess if this is a new ident or a type spec
    if (parser.token == tokIdent)
    {
        *ident = parser.strValue;
        ShBase* obj = currentSymbolScope->deepFind(*ident);
        // In one of the previous attempts to implement this guessing we were calling
        // full expression parsing just to catch any exceptions and analyze them.
        // Unfortunately excpetion throwing/catching in C++ is too expensive, so we'd
        // better do something rather heuristic here:
        if (obj == NULL || !obj->isDefinition())
        {
            parser.next();
            return NULL;
        }
    }
    
    return getTypeExpr(false);
}


// --- STATEMENTS/DEFINITIONS ---------------------------------------------- //


void ShCompiler::parseModuleHeader()
{
    string modName = parser.getIdent();
    if (strcasecmp(modName.c_str(), module.name.c_str()) != 0)
        error("Module name mismatch");
    module.setNamePleaseThisIsWrongIKnow(modName);
    parser.skipSep();
}


ShEnum* ShCompiler::parseEnumType(const string& enumName)
{
    ShEnum* type = new ShEnum(enumName); // the name is for diag. purposes
    codegen->hostScope->addAnonType(type);
    parser.skip(tokLParen, "(");
    while (1)
    {
        string ident = parser.getIdent();
        int nextVal = type->nextValue();
        if (nextVal == 256)
            error("Enum constant has just hit the ceilinig, man.");
        ShDefinition* value = new ShDefinition(ident, type, nextVal);
        codegen->hostScope->addDefinition(value, currentSymbolScope);
        type->registerConst(value);
        if (parser.skipIf(tokRParen))
            break;
        parser.skip(tokComma, ")");
    }
    type->finish();
    return type;
}


void ShCompiler::parseTypeDef()
{
    string ident;
    ShType* type;
    if (parser.skipIf(tokEnum))
    {
        ident = parser.getIdent();
        type = parseEnumType(ident);
    }
    else
    {
        type = getTypeExpr(false);
        if (type == NULL)
            errorWithLoc("Type specification expected");
        ident = parser.getIdent();
        type = getDerivators(type);
    }
    codegen->hostScope->addTypeAlias(ident, type, currentSymbolScope);
    if (!type->isFunction())
        parser.skipSep();
}


void ShCompiler::parseVarConstDef(bool isVar)
{
    string ident;
    ShType* type = NULL;

    if (parser.skipIf(tokEnum))
    {
        ident = parser.getIdent();
        type = parseEnumType("");
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
        ShType* exprType = parseExpr(type);
        parser.skipSep();
        if (type == NULL) // auto
            type = exprType;
        else if (!type->canAssign(exprType))
            error("Type mismatch in variable initialization: " + typeVsType(type, exprType));
        ShVariable* var = codegen->hostScope->addVariable(ident, type,
            currentSymbolScope, codegen);
        codegen->genInitVar(var);
    }
    else
    {
        ShValue value;
        getConstExpr(type, value, false);
        parser.skipSep();
        if (type == NULL) // auto
            type = value.type;
        codegen->hostScope->addDefinition(new ShDefinition(ident, value),
            currentSymbolScope);
    }
}


void ShCompiler::parseEcho(VmCodeGen* tcode)
{
    VmCodeGen* saveCodeGen = replaceCodeGen(tcode);
    if (parser.token != tokSep)
    {
        while (1)
        {
            parseExpr();
            codegen->genEcho();
            if (parser.skipIf(tokComma))
                codegen->genOther(opEchoSp);
            else
                break;
        }
    }
    codegen->genOther(opEchoLn);
    replaceCodeGen(saveCodeGen);
    parser.skipSep();
}


void ShCompiler::parseAssert(VmCodeGen* tcode)
{
    VmCodeGen* saveCodeGen = replaceCodeGen(tcode);
    ShType* type = parseExpr();
    if (!type->isBool())
        error("Boolean expression expected for assertion");
    codegen->genAssert(parser);
    replaceCodeGen(saveCodeGen);
    parser.skipSep();
}


void ShCompiler::parseOtherStatement()
{
    ShType* type = parseDesignator(true);
    if (codegen->genTopIsFuncCall())
    {
        codegen->genPopValue();
    }
    else if (parser.skipIf(tokAssign))
    {
        if (!type->isReference())
            error("L-value expected in assignment");
        ShVariable* var = codegen->genUndoVar();
        ShType* exprType = parseExpr(PReference(type)->base);
        if (!PReference(type)->base->canAssign(exprType))
            error("Type mismatch in assignment: " + typeVsType(PReference(type)->base, exprType));
        codegen->genStoreVar(var);
    }
    else
        error("Definition or statement expected");
    parser.skipSep();
}


void ShCompiler::parseIf(Token tok)
{
    offs endJump = -1;
    if (tok != tokElse)
    {
        ShType* type = parseExpr(queenBee->defaultBool);
        if (!type->isBool())
            error("Boolean expression expected");
        endJump = codegen->genForwardJump(opJumpFalse);
    }

    enterBlock();

    if (tok != tokElse && (parser.token == tokElse || parser.token == tokElif))
    {
        offs t = codegen->genForwardJump(opJump);
        codegen->genResolveJump(endJump);
        endJump = t;
        Token saveTok = parser.token;
        parser.next();
        parseIf(saveTok);
    }

    if (endJump != -1)
        codegen->genResolveJump(endJump);
}


void ShCompiler::parseWhile()
{
    LoopInfo thisLoop;
    LoopInfo* saveTopLoop = replaceTopLoop(&thisLoop);

    thisLoop.continueTarget = codegen->genOffset();
    ShType* type = parseExpr(queenBee->defaultBool);
    if (!type->isBool())
        error("Boolean expression expected");
    offs endJump = codegen->genForwardJump(opJumpFalse);

    enterBlock();
    codegen->genJump(thisLoop.continueTarget);

    while (!thisLoop.breakJumps.empty())
        codegen->genResolveJump(thisLoop.breakJumps.pop());
    codegen->genResolveJump(endJump);
    replaceTopLoop(saveTopLoop);
}


void ShCompiler::parseBreakCont(bool isBreak)
{
    if (topLoop == NULL)
        error(string(isBreak ? "'break'" : "'continue'") + " not inside loop");
    for (int i = -1; i >= -topLoop->symScopes.size(); i--)
        topLoop->symScopes.at(i)->finalizeVars(codegen);
    if (isBreak)
        topLoop->breakJumps.push(codegen->genForwardJump(opJump));
    else
        codegen->genJump(topLoop->continueTarget);
}


void ShCompiler::parseCase()
{
    ShType* caseCtlType = parseExpr();
    if (!caseCtlType->isOrdinal() && !caseCtlType->isString() && !caseCtlType->isTypeRef())
        error("Case control expression must be ordinal, string or typeref");
    if (caseCtlType->isLargeInt())
        error("Large ints are not supported with the 'case' operator");
    parser.skipBlockBegin();

    PodStack<offs> endJumps;
    offs falseJump = -1;
    ShValue value;
    do
    {
        if (falseJump != -1)
            codegen->genResolveJump(falseJump);
        PodStack<offs> trueJumps;
        getConstExpr(caseCtlType, value, true);
        value.registerConst(module);
        while (parser.skipIf(tokComma))
        {
            trueJumps.push(codegen->genCase(value, opJumpTrue));
            getConstExpr(caseCtlType, value, true);
            value.registerConst(module);
        }
        falseJump = codegen->genCase(value, opJumpFalse);
        while (!trueJumps.empty())
            codegen->genResolveJump(trueJumps.pop());
        enterBlock();
        if (parser.token != tokBlockEnd) // avoid unnecessary jumps
            endJumps.push(codegen->genForwardJump());
    }
    while (parser.token != tokBlockEnd && parser.token != tokElse);
    
    if (parser.skipIf(tokElse))
    {
        if (falseJump != -1)
            codegen->genResolveJump(falseJump);
        enterBlock();
    }

    parser.skipBlockEnd();

    while (!endJumps.empty())
        codegen->genResolveJump(endJumps.pop());
    codegen->genPopValue();
}


void ShCompiler::parseBlock()
{
    while (!parser.skipIf(tokBlockEnd))
    {
        if (options.linenumInfo)
            codegen->genLinenum(parser);

        if (parser.skipIf(tokSep))
            ;
        else if (parser.skipIf(tokDef))
            parseTypeDef();
        else if (parser.skipIf(tokConst))
            parseVarConstDef(false);
        else if (parser.skipIf(tokVar))
            parseVarConstDef(true);
        else if (parser.skipIf(tokEcho))
            parseEcho(options.enableEcho ? codegen : &nullCode);
        else if (parser.skipIf(tokAssert))
            parseAssert(options.enableAssert ? codegen : &nullCode);
        else if (parser.skipIf(tokBegin))
            enterBlock();
        else if (parser.skipIf(tokIf))
            parseIf(tokIf);
        else if (parser.skipIf(tokWhile))
            parseWhile();
        else if (parser.token == tokElse)
            error("Misplaced 'else'");
        else if (parser.token == tokElif)
            error("Misplaced 'elif'");
        else if (parser.skipIf(tokBreak))
            parseBreakCont(true);
        else if (parser.skipIf(tokContinue))
            parseBreakCont(false);
        else if (parser.skipIf(tokCase))
            parseCase();
        else
            parseOtherStatement();
        codegen->genFinalizeTemps();
    }
}


void ShCompiler::enterBlock()
{
    if (codegen->hostScope->isLocalScope())
    {
        // The tempSymScope object is temporary; the real objects (types, vars,
        // etc) are registered with the outer scope: either module or state. 
        // Through the temp symbol scope however, we know which vars to 
        // finalize when exiting the block.
        ShSymScope tempSymScope("", typeSymScope, currentSymbolScope);
        currentSymbolScope = &tempSymScope;
        if (topLoop != NULL)
            topLoop->symScopes.push(currentSymbolScope);
        parser.skipBlockBegin();
        parseBlock();
        if (topLoop != NULL)
            if (topLoop->symScopes.pop() != currentSymbolScope)
                internal(150);
        currentSymbolScope = currentSymbolScope->parent;
        tempSymScope.finalizeVars(codegen);
    }
    else
    {
        // We replace the current "host" scope if it's a module-static or a 
        // state-static one with a local scope, so that objects defined in any 
        // nested block are allocated on the stack rather than statically. This
        // can happen only at top-level in modules and states, but not ordinary
        // functions.
        if (currentStateScope->localScope.parent != codegen->hostScope)
            internal(151);
        codegen->hostScope = &currentStateScope->localScope;
        enterBlock();
        codegen->hostScope = (ShScope*)codegen->hostScope->parent;
    }
}


void ShCompiler::parseFunctionBody(ShFunction* funcType)
{
    if (funcType->parent != codegen->hostScope)
        internal(153);
    if (topLoop != NULL)
        internal(154);
    if (topFunction != NULL)
        internal(155);
    VmCodeGen tcode(&funcType->localScope);
    VmCodeGen* saveCodeGen = replaceCodeGen(&tcode);
    enterBlock();
    funcType->code = tcode.getCodeSeg();
    replaceCodeGen(saveCodeGen);
}


// ------------------------------------------------------------------------- //


bool ShCompiler::compile()
{
    try
    {
        codegen = &mainCodeGen;
        currentStateScope = &module;
        currentSymbolScope = &module;

        parser.next();
        if (parser.skipIf(tokModule))
            parseModuleHeader();

        parseBlock();

        parser.skip(tokEof, "<EOF>");

        currentSymbolScope->finalizeVars(codegen);
    }
    catch (EDuplicate& e)
    {
        error(e);
    }
    catch(EInvalidSubrange& e)
    {
        error(e.what());
    }
    catch(ENoContext& e)
    {
        error(e.what());
    }

#ifdef DEBUG
    queenBee->dump("");
    module.dump("");
#endif

    module.codeseg = mainCodeGen.getCodeSeg();

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
#ifdef XCODE
        string filename = "/Users/hovik/Projects/Shannon/src/z.sn";
#else
        string filename = "z.sn";
#endif

    try
    {
        initLangObjs();

        Parser parser(filename);
        ShModule module(extractFileName(filename));
        module.registerString(filename);
        bool compiled = false;

        {
            ShCompiler compiler(parser, module);
            compiled = compiler.compile();
        }

        if (compiled)
        {
            // TODO: exec mains for all used modules
            module.execute();
        }
        else
            return 1;

    }
    catch (Exception& e)
    {
        fprintf(stderr, "%s\n", e.what().c_str());
        return 2;
    }
    catch (int)
    {
        // run-time error
        return 3;
    }

    if (!stk.empty())
        fatal(CRIT_FIRST + 54, "[VM] Stack in undefined state after execution");

    return 0;
}

