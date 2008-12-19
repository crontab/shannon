#ifndef __SOURCE_H
#define __SOURCE_H

#include "str.h"
#include "except.h"
#include "charset.h"


#define INFILE_BUFSIZE 8192
#define DEFAULT_TAB_SIZE 8


class InText
{
protected:
    char* buffer;
    int   bufsize;
    int   bufpos;
    int   linenum;
    int   indent;
    bool  newline;
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
    int  getLinenum()       { return linenum; }
    int  getIndent()        { return indent; }
    void setIndent(int i)   { indent = i; }
    int  getNewLine()       { return newline; }
    void resetNewLine()     { newline = false; }
    bool getEof()           { return eof; }
    bool getEol();
    bool isEolChar(char c)  { return c == '\r' || c == '\n'; }
    char preview();
    char get();
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


#endif

