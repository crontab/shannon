#ifndef __SOURCE_H
#define __SOURCE_H


#include "common.h"
#include "charset.h"
#include "runtime.h"

#include <stack>


struct EParser: public emessage
{
    EParser(const str& ifilename, int ilinenum, const str& msg);
};



enum Token
{
    tokUndefined = -1,
    // Blocks: for the compiler, these tokens are transparent wrt to C-style
    // vs. Python-style modes
    tokBlockBegin, tokBlockEnd, tokSep, tokIndent,
    tokEof,
    tokIdent, tokIntValue, tokStrValue,

    tokConst, tokDef,
    tokEnum, tokEcho, tokAssert, tokBegin, tokIf, tokElif, tokElse,
    tokWhile, tokBreak, tokContinue, tokCase, tokReturn, tokExit,
    
    // Term level
    tokMul, tokDiv, tokMod,
    // Arithm level
    tokPlus, tokMinus,
    // Cat level (simple expr)
    tokCat,
    // Rel level: the order should be in sync with comparison opcodes
    tokEqual, tokNotEq, tokLessThan, tokLessEq, tokGreaterThan, tokGreaterEq,
    // NOT level
    tokNot,
    // AND level
    tokAnd, tokShl, tokShr,
    // OR level
    tokOr, tokXor,
    
    tokIn, tokIs, tokAs,

    // Special chars and sequences
    tokComma, tokPeriod, tokRange,
    tokLSquare, tokRSquare, tokLParen, tokRParen, /* tokLCurly, tokRCurly, */
    tokAssign, tokExclam,
    
    // Aliases; don't define new consts after this
    tokLAngle = tokLessThan, tokRAngle = tokGreaterThan,
    tokCmpFirst = tokEqual, tokCmpLast = tokNotEq
};


class Parser
{
protected:
    str fileName;
    objptr<fifo_intf> input;
    bool newLine;
    std::stack<int> indentStack;
    int linenum;
    int indent;
    bool singleLineBlock; // if a: b = c
    int curlyLevel;

    str errorLocation() const;
    void parseStringLiteral();
    void skipMultilineComment();
    void skipSinglelineComment();

public:
    Token token;
    str strValue;
    uinteger intValue;
    
    Parser(const str&, fifo_intf*);
    ~Parser();
    
    Token next();

    bool isAssignment()
            { return token == tokAssign; }
    void error(const str& msg);
    void errorWithLoc(const str& msg);
    void error(const char*);
    void errorWithLoc(const char*);
    void skipSep();
    void skip(Token tok, const char* errName);
    bool skipIf(Token tok)
            { if (token == tok) { next(); return true; } return false; }
    void skipBlockBegin();
    void skipBlockEnd();
    str getIdentifier();
    
    str getFileName() const { return fileName; }
    int getLineNum() const { return linenum; }
    int getIndent() const { return indent; }
    
    void skipEol();
};


str extractFileName(str filepath);
str mkPrintable(char c);
str mkPrintable(const str&);

// Exposed for unit tests
extern const charset wsChars;
extern const charset identFirst;
extern const charset identRest;
extern const charset digits;
extern const charset hexDigits;
extern const charset printableChars;


#endif

