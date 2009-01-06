#ifndef __SOURCE_H
#define __SOURCE_H

#include "common.h"
#include "baseobj.h"


#define INFILE_BUFSIZE 8192
#define DEFAULT_TAB_SIZE 8


class InText: public Base
{
protected:
    char* buffer;
    int   bufsize;
    int   bufpos;
    int   linenum;
    int   column;
    bool  eof;
    int   tabsize;
    
    void error(int code) throw(ESysError);
    virtual void validateBuffer() = 0;
    void doSkipEol();
    void skipTo(char c);
    void token(const charset& chars, string& result, bool skip);

public:
    InText();
    virtual ~InText();
    
    virtual string getFileName() = 0;
    int  getLineNum()       { return linenum; }
    int  getColumn()        { return column; }
    bool getEof()           { return eof; }
    bool getEol();
    bool isEolChar(char c)  { return c == '\r' || c == '\n'; }
    char preview();
    char get();
    bool getIf(char);
    void skipEol();
    void skipLine();
    string token(const charset& chars) throw(ESysError);
    void skip(const charset& chars) throw(ESysError);
};



class InFile: public InText
{
protected:
    string filename;
    int  fd;

    virtual void validateBuffer();

public:
    InFile(const string& filename);
    virtual ~InFile();
    virtual string getFileName();
};


struct EParser: public Exception
{
    EParser(const string& ifilename, int ilinenum, const string& msg);
};


struct ENotFound: public EParser
{
    ENotFound(const string& ifilename, int ilinenum, const string& ientry);
};



enum Token
{
    tokUndefined = -1,
    tokBlockBegin, tokBlockEnd, tokSep, tokIndent, // these will depend on C-style vs. Python-style modes in the future
    tokEof,
    tokIdent, tokIntValue, tokLargeValue, tokStrValue,

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
    InText* input;
    bool blankLine;
    Stack<int> indentStack;
    int linenum;

    string errorLocation() const;
    void parseStringLiteral();
    void skipMultilineComment();
    void skipSinglelineComment();

public:
    bool singleLineBlock; // if a: b = c
    Token token;
    string strValue;
    uint intValue;
    ularge largeValue;
    
    Parser(const string& filename);
    ~Parser();
    
    Token next();

    bool isAssignment()
            { return token == tokAssign; }
    void error(const string& msg);
    void errorWithLoc(const string& msg);
    void error(const char*);
    void errorWithLoc(const char*);
    void errorNotFound(const string& ident);
    void skipSep();
    void skip(Token tok, const char* errName);
    bool skipIf(Token tok)
            { if (token == tok) { next(); return true; } return false; }
    void skipBlockBegin();
    void skipBlockEnd();
    string getIdent();
    
    string getFileName() const { return input->getFileName(); }
    int getLineNum() const { return linenum; }
};


string extractFileName(string filepath);
string mkPrintable(char c);
string mkPrintable(const string&);

#endif

