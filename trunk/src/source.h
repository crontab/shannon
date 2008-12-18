#ifndef __SOURCE_H
#define __SOURCE_H

#ifndef __STR_H
#include "str.h"
#endif

#ifndef __EXCEPT_H
#include "except.h"
#endif

#ifndef __CHARSET_H
#include "charset.h"
#endif


#define INTEXT_BUFSIZE 8192
#define DEFAULT_TAB_SIZE 8


class InText
{
    string filename;
    int  fd;
    char buf[INTEXT_BUFSIZE];
    int  bufsize;
    int  bufpos;
    int  linenum;
    int  indent;
    bool newline;
    bool eof;
    int tabsize;
    
    void error(int code) throw(ESysError);
    void validateBuffer();
    void doSkipEol();
    void token(const charset& chars, string& result, bool skip);

public:
    InText(const string& filename);
    ~InText();
    
    int  getIndent()  { return indent; }
    int  getLinenum() { return linenum; }
    bool getEof()     { return eof; }
    bool getEol();
    char preview();
    char get();
    void skipEol();
    void skipTo(char c);
    void skipLine();
    string token(const charset& chars) throw(ESysError);
    void skip(const charset& chars) throw(ESysError);
};


#endif

