#ifndef __PARSER_H
#define __PARSER_H

#include "common.h"
#include "runtime.h"


enum Token
{
    tokUndefined = -1,
    tokEof, tokSep, tokSemi,
    tokIdent, tokPrevIdent, tokIntValue, tokStrValue,

    tokConst, tokDef, tokVar,
    tokDump, tokAssert, tokBegin, tokIf, tokElif, tokElse,
    tokWhile, tokBreak, tokContinue, tokSwitch, tokCase, tokReturn, tokExit,
    tokFor,
    tokTypeOf, tokDel, tokIns, tokThis,

    // Term level
    tokMul, tokDiv, tokMod,
    // Arithm level
    tokPlus, tokMinus,
    // Cat level (simple expr)
    tokCat,
    // Rel level: the order should be in sync with comparison opcodes, except tokIn
    tokEqual, tokNotEq, tokLessThan, tokLessEq, tokGreaterThan, tokGreaterEq,
    tokIn,
    // NOT level
    tokNot,
    // AND level
    tokAnd, tokShl, tokShr,
    // OR level
    tokOr, tokXor,

    // Special chars and sequences
    tokComma, tokPeriod, tokRange, tokCaret, tokAt, tokSharp, tokQuestion, tokExclam,
    tokLSquare, tokRSquare, tokLParen, tokRParen, tokLCurly, tokRCurly, tokColon,
    tokIs, tokAs,

    tokAssign,
    // In-place operators, order is important, in sync with opAddAssign etc
    tokAddAssign, tokSubAssign, tokMulAssign, tokDivAssign, tokModAssign,
    // Other operators
    tokCatAssign, tokPush, tokPull,

    // Aliases; don't define new consts after this
    tokLAngle = tokLessThan, tokRAngle = tokGreaterThan,
    tokWildcard = tokMul,
};


class InputRecorder: public bufevent
{
    friend class Parser;
protected:
    char* buf;
    memint offs;
    memint prevpos;
    void clear();
    bool active() { return buf != NULL; }
public:
    str data;
    InputRecorder();
    void event(char* buf, memint tail, memint head);
};


class Parser: noncopyable
{
protected:
    objptr<buffifo> input;
    integer linenum;
    bool prevWasEol;

    str prevIdent; // undoIdent()
    Token saveToken;
    
    InputRecorder recorder;  // raw input recorder, for assert and dump

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

    Parser(buffifo*);
    ~Parser();

    Token next();
    bool eof() const
            { return token == tokEof; }
    void undoIdent(const str& ident);
    void redoIdent();
    const str& getPrevIdent()
            { return prevIdent; }
    void error(const str& msg);
    void error(const char*);
    void skipEos();
    void skipToEos();
    void skipWsSeps()
        { while (skipIf(tokSep)) ; }
    void expect(Token tok, const char* errName);
    bool skipIf(Token tok)
            { if (token == tok) { next(); return true; } return false; }
    bool skipIf(Token tokMin, Token tokMax)
            { if (token >= tokMin && token <= tokMax) { next(); return true; } return false; }
    void skipMultiBlockBegin(const char* errmsg);
    void skipMultiBlockEnd();
    bool isBlockEnd()
            { return token == tokRCurly; }
    str getIdentifier();

    str getFileName() const { return input->get_name(); }
    integer getLineNum() const;
    void beginRecording();
    str endRecording();
};


bool isValidIdent(const str&);

#endif // __PARSER_H
