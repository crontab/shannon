#ifndef __SOURCE_H
#define __SOURCE_H


#include <stack>

#include "charset.h"
#include "common.h"
#include "variant.h"


#define INFILE_BUFSIZE 8192
#define DEFAULT_TAB_SIZE 8


class InText: public object
{
protected:
    char* buffer;
    int   bufsize;
    int   bufpos;
    int   linenum;
    int   column;
    bool  eof;
    int   tabsize;

    void error(int code) throw(esyserr);
    virtual void validateBuffer() = 0;
    void doSkipEol();
    void skipTo(char c);
    void token(const charset& chars, str& result, bool skip);

public:
    InText();
    virtual ~InText();
    
    virtual str getFileName() = 0;
    int  getLineNum()       { return linenum; }
    int  getColumn()        { return column; }
    bool getEof()           { return eof; }
    bool getEol();
    bool isEolChar(char c)  { return c == '\r' || c == '\n'; }
    char preview();
    char get();
    bool getIf(char);
    void skipEol();
    str token(const charset& chars);
    void skip(const charset& chars);
};



class InFile: public InText
{
protected:
    str  filename;
    int  fd;

    virtual void validateBuffer();

public:
    InFile(const str& filename);
    virtual ~InFile();
    virtual str getFileName();
};


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
    objptr<InText> input;
    bool blankLine;
    std::stack<int> indentStack;
    int linenum;
    bool singleLineBlock; // if a: b = c

    str errorLocation() const;
    void parseStringLiteral();
    void skipMultilineComment();
    void skipSinglelineComment();

public:
    Token token;
    str strValue;
    uinteger intValue;
    
    Parser(InText*);
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
    
    str getFileName() const { return input->getFileName(); }
    int getLineNum() const { return linenum; }
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

