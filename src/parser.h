#ifndef __PARSER_H
#define __PARSER_H

#include "common.h"
#include "charset.h"
#include "runtime.h"


struct EParser: public emessage
{
    EParser(const str& ifilename, int ilinenum, const str& msg) throw();
    ~EParser() throw();
};


enum Token
{
    tokUndefined = -1,
    tokBlockBegin, tokBlockEnd, tokSingleBlock, tokSep,
    tokEof,
    tokIdent, tokPrevIdent, tokIntValue, tokStrValue,

    tokConst, tokDef, tokVar,
    tokDump, tokAssert, tokBegin, tokIf, tokElif, tokElse,
    tokWhile, tokBreak, tokContinue, tokCase, tokReturn, tokExit,
    tokTypeOf, tokEnum,

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
    tokAssign,

    // Aliases; don't define new consts after this
    tokLAngle = tokLessThan, tokRAngle = tokGreaterThan,
    tokCmpFirst = tokEqual, tokCmpLast = tokGreaterEq,
    tokWildcard = tokMul
};


class Parser: noncopyable
{
protected:
    objptr<fifo> input;
    int linenum;

    str prevIdent; // undoIdent()
    Token saveToken;

    str errorLocation() const;
    void parseStringLiteral();
    void skipMultilineComment();
    void skipSinglelineComment();
    void skipWs();
    void skipEol();

public:
    Token token;
    str strValue;
    uinteger intValue;

    Parser(fifo*) throw();
    ~Parser() throw();

    Token next();
    void undoIdent(const str& ident);
    void redoIdent();
    const str& getPrevIdent()
            { return prevIdent; }
    void error(const str& msg);
    void errorWithLoc(const str& msg);
    void error(const char*);
    void errorWithLoc(const char*);
    void skipSep();
    void expect(Token tok, const char* errName);
    bool skipIf(Token tok)
            { if (token == tok) { next(); return true; } return false; }
    str getIdentifier();

    str getFileName() const { return input->get_name(); }
    int getLineNum() const { return linenum; }
};


bool isValidIdent(const str&);

#endif // __PARSER_H
