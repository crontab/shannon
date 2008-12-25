#ifndef __SOURCE_H
#define __SOURCE_H

#include "str.h"
#include "except.h"
#include "charset.h"
#include "contain.h"
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


class EParser: public EMessage
{
protected:
    string filename;
    int linenum;
public:
    EParser(const string& ifilename, int ilinenum, const string& msg);
    virtual ~EParser();
    virtual string what() const;
};


class ENotFound: public EParser
{
    string entry;
public:
    ENotFound(const string& ifilename, int ilinenum, const string& ientry);
    virtual ~ENotFound();
    const string& getEntry() const  { return entry; }
};





enum Token
{
    tokUndefined = -1,
    tokBegin, tokEnd, tokSep, // these will depend on C-style vs. Python-style modes in the future
    tokEof,
    tokIdent, tokIntValue, tokStrValue,
    // keywords
    tokModule, tokConst, tokDef, tokVar, tokTypeOf,
    
    // the order in this group is important: it's in sync with OpComparison
    tokEqual, tokLessThan, tokLessEq, tokGreaterEq, tokGreaterThan, tokNotEq,
    
    // special chars and sequences
    tokIn, tokIs,
    tokComma, tokPeriod, tokRange, tokDiv, tokMul,
    tokLSquare, tokRSquare, tokLParen, tokRParen, /* tokLCurly, tokRCurly, */
    tokAssign,
    
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
    int getLineNum() { return linenum; }

public:
    bool singleLineBlock; // if a: b = c
    Token token;
    string strValue;
    ularge intValue;
    
    Parser(const string& filename);
    ~Parser();
    
    Token next();
    Token nextBegin();
    Token nextEnd();

    void error(const string& msg);
    void errorWithLoc(const string& msg);
    void error(const char*);
    void errorWithLoc(const char*);
    void errorNotFound(const string& ident);
    void skipSep();
    void skip(Token tok, const char* errName);
    bool skipIf(Token tok)
            { if (token == tok) { next(); return true; } return false; }
    string getIdent();
    
//    int indentLevel()  { return indentStack.top(); }
};


string extractFileName(string filepath);
string mkPrintable(char c);
string mkPrintable(const string&);

#endif

