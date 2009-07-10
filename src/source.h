#ifndef __SOURCE_H
#define __SOURCE_H


#include <stack>

#include "charset.h"
#include "common.h"
#include "variant.h"
#include "fifo.h"


struct EParser: public emessage
{
    EParser(const str& ifilename, int ilinenum, const str& msg);
};


struct ENotFound: public EParser
{
    ENotFound(const str& ifilename, int ilinenum, const str& ientry);
};



enum Token
{
    tokUndefined = -1,
    tokBlockBegin, tokBlockEnd, tokSep, tokIndent, // these will depend on C-style vs. Python-style modes in the future
    tokEof,
    tokIdent, tokIntValue, tokStrValue,

    tokModule, tokConst, tokDef, tokVar, tokTypeOf, tokTrue, tokFalse, tokNull,
    tokEnum, tokEcho, tokAssert, tokSizeOf, tokBegin, tokIf, tokElif, tokElse,
    tokWhile, tokBreak, tokContinue, tokCase, tokReturn,
    
    // term level
    tokMul, tokDiv, tokMod,
    // arithm level
    tokPlus, tokMinus,
    // cat level (simple expr)
    tokCat,
    // Rel level: the order in this group is important: it's in sync with OpComparison
    tokEqual, tokLessThan, tokLessEq, tokGreaterEq, tokGreaterThan, tokNotEq,
    // NOT level
    tokNot,
    // AND level
    tokAnd, tokShl, tokShr,
    // OR level
    tokOr, tokXor,
    
    tokIn, tokIs, tokAs,

    // special chars and sequences
    tokComma, tokPeriod, tokRange,
    tokLSquare, tokRSquare, tokLParen, tokRParen, /* tokLCurly, tokRCurly, */
    tokAssign, tokExclam,
    
    // aliases; don't define new consts after this group
    tokLAngle = tokLessThan, tokRAngle = tokGreaterThan,
    tokCmpFirst = tokEqual, tokCmpLast = tokNotEq
};


enum SyntaxMode { syntaxIndent, syntaxCurly };


class Parser
{
protected:
    str fileName;
    objptr<fifo_intf> input;
    bool blankLine;
    std::stack<int> indentStack;
    int linenum;
    int indent;
    bool singleLineBlock; // if a: b = c

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
    void errorNotFound(const str& ident);
    void skipSep();
    void skip(Token tok, const char* errName);
    bool skipIf(Token tok)
            { if (token == tok) { next(); return true; } return false; }
    void skipBlockBegin();
    void skipBlockEnd();
    str getIdent();
    
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

