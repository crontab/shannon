
#include <stdio.h>

#include "str.h"
#include "except.h"
#include "langobj.h"


// --- VIRTUAL MACHINE ----------------------------------------------------- //

#ifdef SINGLE_THREADED
#  define VM_STATIC static
#else
#  define VM_STATIC
#endif


enum OpCode
{
    opEnd,          // []
    opNop,          // []
    
    // opLoadZero,     // []                   +1
    // opLoadLargeZero,// []                   +1
    opLoadInt,      // [int]                +1
    opLoadLarge,    // [large]            +2/1
    opLoadChar,     // [int]                +1
    opLoadFalse,    // []                   +1
    opLoadTrue,     // []                   +1
    opLoadNull,     // []                   +1
    opLoadNullStr,  // []                   +1
    opLoadStr,      // [str-data-ptr]       +1
    opLoadTypeRef,  // [ShType*]            +1
    
    opCmpInt,       // []               -2  +1
    opCmpIntLarge,  // []               -2  +1
    opCmpLarge,     // []               -2  +1
    opCmpLargeInt,  // []               -2  +1
    opCmpStr,       // []               -2  +1
    opCmpStrChr,    // []               -2  +1
    opCmpChrStr,    // []               -2  +1
    opCmpIntZero,   // []               -1  +1
    // opCmpLargeZero, // []               -1  +1
    // opCmpStrNull,   // []               -1  +1

    // TODO: opCmpInt0, opCmpLarge0, opStrIsNull

    // compare the stack top with 0 and replace it with a bool value
    opEQ,           // []               -1  +1
    opLT,           // []               -1  +1
    opLE,           // []               -1  +1
    opGE,           // []               -1  +1
    opGT,           // []               -1  +1
    opNE,           // []               -1  +1
    
    // binary
    opMkSubrange,   // []               -2  +1

    // typecasts
    opLargeToInt,   // []               -1  +1
    opIntToLarge,   // []               -1  +1

    // TODO: linenum, rangecheck opcodes
    
    opInv = -1,
    
    opCmpFirst = opEQ
};


union VmQuant
{
    OpCode op_;
    int int_;
    ptr ptr_;
#ifdef PTR64
    large large_;
#endif
};


typedef twins<ShType*> ShTypePair;


class VmStack: public noncopyable
{
    PodStack<VmQuant> stack;

public:
    VmStack(): stack()        { }
    int size() const          { return stack.size(); }
    bool empty() const        { return stack.empty(); }
    void clear()              { return stack.clear(); }
    VmQuant& push()           { return stack.push(); }
    VmQuant  pop()            { return stack.pop(); }
    void  pushInt(int v)      { push().int_ = v; }
    void  pushPtr(ptr v)      { push().ptr_ = v; }
    int   popInt()            { return pop().int_; }
    ptr   popPtr()            { return pop().ptr_; }
    int   topInt() const      { return stack.top().int_; }
    ptr   topPtr() const      { return stack.top().ptr_; }
    int&  topIntRef()         { return stack.top().int_; }

#ifdef PTR64
    void   pushLarge(large v) { push().large_ = v; }
    large  popLarge()         { return pop().large_; }
    large& topLarge()         { return stack.top().large_; }
    large& topLargeRef()      { return stack.top().large_; }
#else
    void  pushLarge(large v)
        { push().int_ = int(v); push().int_ = int(v >> 32); }
    large popLarge()
        { int hi = popInt(); return largerec(popInt(), hi); }
    large topLarge()
        { return (large(stack.at(-1).int_) << 32) | unsigned(stack.at(-2).int_); }
#endif
};


class VmCode: public noncopyable
{
protected:
    struct EmulStackInfo: ShValue
    {
        int opIndex;
        bool isValue;
        EmulStackInfo(const ShValue& iValue, int iOpIndex);
        EmulStackInfo(ShType* iType, int iOpIndex);
    };

    PodArray<VmQuant> code;
    PodStack<EmulStackInfo> emulStack;
    ShScope* compilationScope;
    int lastOpIndex;

    void emulPush(const ShValue& v);
    void emulPush(ShType* v);
    ShType* emulPopType()           { return emulStack.pop().type; }
    EmulStackInfo& emulTop()        { return emulStack.top(); }
    void emulPop()                  { emulStack.pop(); }

    void genOp(OpCode o)            { code.add().op_ = o; }
    void genInt(int v)              { code.add().int_ = v; }
    void genPtr(ptr v)              { code.add().ptr_ = v; }
    void genCmpOp(OpCode op, OpCode cmp);
    int  updateLastOpIndex()
        { int t = lastOpIndex; lastOpIndex = code.size(); return t; }
    void verifyClean();

#ifdef PTR64
    void genLarge(large v)  { code.add().large_ = v; }
#else
    void genLarge(large v)  { genInt(int(v)); genInt(int(v >> 32)); }
#endif

    VM_STATIC void runtimeError(int code, const char*);
    VM_STATIC void run(VmQuant* p);

public:
    VmCode(ShScope* iCompilationScope);
    
    void genLoadConst(const ShValue&);
    void genLoadTypeRef(ShType*);
    void genMkSubrange();
    void genComparison(OpCode);
    void genStaticCast(ShType*);
    void endGeneration();
    
    ShValue runConstExpr();
    ShType* runTypeExpr();
    ShType* topType() const  { return emulStack.top().type; }
};


// ------------------------------------------------------------------------- //


class ENoContext: public Exception
{
public:
    virtual string what() const { return "No execution context"; }
};


VmCode::EmulStackInfo::EmulStackInfo(const ShValue& iValue, int iOpIndex)
    : ShValue(iValue), opIndex(iOpIndex), isValue(true)  { }

VmCode::EmulStackInfo::EmulStackInfo(ShType* iType, int iOpIndex)
    : ShValue(iType), opIndex(iOpIndex), isValue(false)  { }


VmCode::VmCode(ShScope* iCompilationScope)
    : code(), compilationScope(iCompilationScope), lastOpIndex(0)  { }


#ifdef SINGLE_THREADED

VmStack vmStack;

#endif


void VmCode::runtimeError(int code, const char* msg)
{
    fatal(RUNTIME_FIRST + code, msg);
}


static int compareInt(int a, int b)
    { if (a < b) return -1; else if (a == b) return 0; return 1; }

static int compareLarge(large a, large b)
    { if (a < b) return -1; else if (a == b) return 0; return 1; }

static int compareStr(ptr a, ptr b)
    { return PTR_TO_STRING(a).compare(PTR_TO_STRING(b)); }

static int compareStrChr(ptr a, int b)
    { return PTR_TO_STRING(a).compare(string(char(b))); }

static int compareChrStr(int a, ptr b)
    { return string(char(a)).compare(PTR_TO_STRING(b)); }


void VmCode::run(VmQuant* p)
{
    while (1)
    {
        switch ((p++)->op_)
        {
        case opEnd: return;
        case opNop: break;

        // --- LOADERS ----------------------------------------------------- //
        // case opLoadZero: vmStack.pushInt(0); break;
        // case opLoadLargeZero: vmStack.pushLarge(0); break;
        case opLoadInt: vmStack.pushInt((p++)->int_); break;
#ifdef PTR64
        case opLoadLarge: vmStack.pushLarge((p++)->large_); break;
#else
        case opLoadLarge: vmStack.pushInt((p++)->int_); vmStack.pushInt((p++)->int_); break;
#endif
        case opLoadChar: vmStack.pushInt((p++)->int_); break;
        case opLoadFalse: vmStack.pushInt(0); break;
        case opLoadTrue: vmStack.pushInt(1); break;
        case opLoadNull: vmStack.pushInt(0); break;
        case opLoadNullStr: vmStack.pushPtr(emptystr); break;
        case opLoadStr: vmStack.pushPtr((p++)->ptr_); break;
        case opLoadTypeRef: vmStack.pushPtr((p++)->ptr_); break;

        // --- COMPARISONS ------------------------------------------------- //
        case opCmpInt:
            {
                int r = vmStack.popInt();
                int& t = vmStack.topIntRef();
                t = compareInt(t, r);
            }
            break;
        case opCmpIntLarge:
            {
                large r = vmStack.popLarge();
                int& t = vmStack.topIntRef();
                t = compareLarge(t, r);
            }
            break;
        case opCmpLarge:
            {
                large r = vmStack.popLarge();
                large l = vmStack.popLarge();
                vmStack.pushInt(compareLarge(l, r));
            }
            break;
        case opCmpLargeInt:
            {
                int r = vmStack.popInt();
                large l = vmStack.popLarge();
                vmStack.pushInt(compareLarge(l, r));
            }
            break;
        case opCmpStr:
            {
                ptr r = vmStack.popPtr();
                ptr l = vmStack.popPtr();
                vmStack.pushInt(compareStr(l, r));
            }
            break;
        case opCmpStrChr:
            {
                int r = vmStack.popInt();
                ptr l = vmStack.popPtr();
                vmStack.pushInt(compareStrChr(l, r));
            }
            break;
        case opCmpChrStr:
            {
                ptr r = vmStack.popPtr();
                int& t = vmStack.topIntRef();
                t = compareChrStr(t, r);
            }
            break;
        case opEQ: { int& t = vmStack.topIntRef(); t = t == 0; } break;
        case opLT: { int& t = vmStack.topIntRef(); t = t < 0; } break;
        case opLE: { int& t = vmStack.topIntRef(); t = t <= 0; } break;
        case opGE: { int& t = vmStack.topIntRef(); t = t >= 0; } break;
        case opGT: { int& t = vmStack.topIntRef(); t = t > 0; } break;
        case opNE: { int& t = vmStack.topIntRef(); t = t != 0; } break;

        // --- BINARY OPERATORS -------------------------------------------- //
#ifdef PTR64
        case opMkSubrange:
            {
                large hi = large(vmStack.popInt()) << 32;
                vmStack.pushLarge(unsigned(vmStack.popInt()) | hi);
            }
            break;
#else
        case opMkSubrange: /* two ints become a subrange, haha! */ break;
#endif

        case opLargeToInt: vmStack.pushInt(vmStack.popLarge()); break;
        case opIntToLarge: vmStack.pushLarge(vmStack.popInt()); break;

        default: fatal(CRIT_FIRST + 50, ("[VM] Unknown opcode " + itostring((--p)->op_, 16, 8, '0')).c_str());
        }
    }
}


void VmCode::verifyClean()
{
    if (!emulStack.empty())
        fatal(CRIT_FIRST + 52, "[VM] Emulation stack in undefined state");
    if (!vmStack.empty())
        fatal(CRIT_FIRST + 53, "[VM] Stack in undefined state");
}


ShValue VmCode::runConstExpr()
{
    endGeneration();
    run(&code._at(0));
    ShType* type = emulPopType();
    if (type->isLarge())
        return ShValue(type, vmStack.popLarge());
    if (type->isPointer())
        return ShValue(type, vmStack.popPtr());
    else
        return ShValue(type, vmStack.popInt());
    verifyClean();
}


ShType* VmCode::runTypeExpr()
{
    endGeneration();
    EmulStackInfo& e = emulTop();
    ShType* type = e.type;
    if (e.type->isTypeRef() && e.isValue)
        type = (ShType*)e.value.ptr_;
    emulPop();
    verifyClean();
    return type;
}


void VmCode::genCmpOp(OpCode op, OpCode cmp)
{
    genOp(op);
    genOp(cmp);
}


void VmCode::emulPush(const ShValue& v)
{
    emulStack.push(EmulStackInfo(v, code.size()));
}


void VmCode::emulPush(ShType* t)
{
    emulStack.push(EmulStackInfo(t, code.size()));
}


void VmCode::genLoadConst(const ShValue& v)
{
    updateLastOpIndex();
    emulPush(v);
    if (v.type->isOrdinal())
    {
        if (v.type->isBool())
            genOp(v.value.int_ ? opLoadTrue : opLoadFalse);
        else if (v.type->isChar())
        {
            genOp(opLoadChar);
            genInt(v.value.int_);
        }
        else if (((ShOrdinal*)v.type)->isLarge())
        {
            genOp(opLoadLarge);
            genLarge(v.value.large_);
        }
        else
        {
            genOp(opLoadInt);
            genInt(v.value.int_);
        }
    }
    else if (v.type->isString())
    {
        const string& s = PTR_TO_STRING(v.value.ptr_);
        if (s.empty())
            genOp(opLoadNullStr);
        else
        {
            genOp(opLoadStr);
            genPtr(ptr(s.c_bytes()));
        }
    }
    else if (v.type->isVoid())
        genOp(opLoadNull);
    else
        throw EInternal(50, "Unknown type in VmCode::genLoadConst()");
}


void VmCode::genLoadTypeRef(ShType* type)
{
    updateLastOpIndex();
    emulPush(ShValue(queenBee->defaultTypeRef, type));
    genOp(opLoadTypeRef);
    genPtr(type);
}


void VmCode::genMkSubrange()
{
    updateLastOpIndex();
    emulPop();
    ShType* type = emulPopType();
    if (!type->isOrdinal())
        throw EInternal(51, "Ordinal type expected");
    emulPush(((ShOrdinal*)type)->deriveRangeType(compilationScope));
    genOp(opMkSubrange);
}


void VmCode::genComparison(OpCode cmp)
{
    updateLastOpIndex();

    OpCode op = opInv;
    ShType* right = emulPopType();
    ShType* left = emulPopType();

    bool leftStr = left->isString();
    bool rightStr = right->isString();
    if (leftStr)
    {
        if (right->isChar())
            op = opCmpStrChr;
        else if (rightStr)
            op = opCmpStr;
    }
    else if (rightStr && left->isChar())
    {
        op = opCmpChrStr;
    }

    else if (left->isOrdinal() && right->isOrdinal())
    {
        // TODO: check if one of the operands is 0
        static OpCode alchemy[2][2] =
            {{opCmpInt, opCmpIntLarge}, {opCmpLargeInt, opCmpLarge}};
        op = alchemy[left->isLarge()][right->isLarge()];
    }

    if (op == opInv)
        throw EInternal(52, "Invalid operand types");

    emulPush(queenBee->defaultBool);
    genCmpOp(op, cmp);
}


void VmCode::genStaticCast(ShType* type)
{
    ShType* fromType = emulPopType();
    emulPush(type);
    bool isDstLarge = type->isLarge();
    bool isSrcLarge = fromType->isLarge();
    if (isSrcLarge && !isDstLarge)
        genOp(opLargeToInt);
    else if (!isSrcLarge && isDstLarge)
        genOp(opIntToLarge);
}


void VmCode::endGeneration()
{
    genOp(opEnd);
}


// ------------------------------------------------------------------------- //
// --- HIS MAJESTY THE COMPILER -------------------------------------------- //
// ------------------------------------------------------------------------- //


string typeVsType(ShType* a, ShType* b)
{
    return a->getDefinitionQ() + " vs. " + b->getDefinitionQ();
}


// --- EXPRESSION ---------------------------------------------------------- //

/*
    <nested-expr>, <typecast>, <ident>, <int>, <string>, <char>
    <array-sel>, <fifo-sel>, <function-call>, <mute>
    -, not
    *, /, div, mod, and, shl, shr, as
    +, –, or, xor
    ..
    ==, <>, != <, >, <=, >=, in, is
*/

ShBase* ShModule::getQualifiedName()
{
    // qualified-name ::= { ident "." } ident
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
    if (parser.token == tokLParen)
    {
        parser.next();
        parseExpr(code);
        parser.skip(tokRParen, ")");
    }

    // numeric literal
    else if (parser.token == tokIntValue)
    {
        large value = parser.intValue;
        ShInteger* type = queenBee->defaultInt->contains(value) ?
            queenBee->defaultInt : queenBee->defaultLarge;
        parser.next();
        code.genLoadConst(ShValue(type, parser.intValue));
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
    
    else if (parser.token == tokTypeOf)
    {
        parser.next();
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
}


void ShModule::parseDesignator(VmCode& code)
{
    parseAtom(code);
}


void ShModule::parseFactor(VmCode& code)
{
    parseDesignator(code);
}


void ShModule::parseTerm(VmCode& code)
{
    parseFactor(code);
}


void ShModule::parseSimpleExpr(VmCode& code)
{
    parseTerm(code);
}


void ShModule::parseSubrange(VmCode& code)
{
    parseSimpleExpr(code);
    if (parser.token == tokRange)
    {
        // Check bounds for left < right maybe? Or maybe not.
        ShType* left = code.topType();
        parser.next();
        parseSimpleExpr(code);
        ShType* right = code.topType();
        if (!left->isOrdinal() || !right->isOrdinal())
            error("Only ordinal types are allowed in subranges");
        if (!left->isCompatibleWith(right))
            error("Left and right values of a subrange must be compatible");
        if (left->isLarge() || right->isLarge())
            error("Large subrange bounds are not supported");
        code.genMkSubrange();
    }
}


void ShModule::parseExpr(VmCode& code)
{
    parseSubrange(code);
    while (parser.token >= tokCmpFirst && parser.token <= tokCmpLast)
    {
        OpCode op = OpCode(opCmpFirst + int(parser.token - tokCmpFirst));
        ShType* leftType = code.topType();
        parser.next();
        parseSubrange(code);
        ShType* rightType = code.topType();
        if (!leftType->isCompatibleWith(rightType))
            error("Type mismatch in comparison: " + typeVsType(leftType, rightType));
        code.genComparison(op);
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
        && !((ShOrdinal*)typeHint)->contains(result.value.large_))
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
    // array-derivator ::= "[" [ type ] "]"
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
            type = ((ShOrdinal*)type)->deriveRangeType(currentScope);
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


void ShModule::parseTypeDef()
{
    ShType* type = getType();
    string ident = parser.getIdent();
    type = getDerivators(type);
    addObject(new ShTypeAlias(ident, type));
}


void ShModule::parseVarConstDef(bool isVar)
{
    string ident;
    ShType* type = getTypeOrNewIdent(&ident);
    if (type != NULL)  // if not auto, derivators are possible after the ident
    {
        ident = parser.getIdent();
        type = getDerivators(type);
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

