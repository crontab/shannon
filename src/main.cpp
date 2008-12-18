
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "charset.h"
#include "str.h"
#include "array.h"
#include "baseobj.h"


// ------------------------------------------------------------------------ //
// --- TEXT FILE READER --------------------------------------------------- //
// ------------------------------------------------------------------------ //

#define INTEXT_BUFSIZE 8192
#define DEFAULT_TAB_SIZE 8

class charset;

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


InText::InText(const string& ifilename)
    : filename(ifilename), fd(-1), bufsize(0), bufpos(0), linenum(0),
      indent(0), newline(true), eof(false), tabsize(DEFAULT_TAB_SIZE)
{
}


InText::~InText()
{
    if (fd >= 0)
    {
        close(fd);
        eof = true;
        fd = -1;
    }
}


void InText::error(int code) throw(ESysError)
{
    eof = true;
    throw ESysError(code);
}


void InText::validateBuffer()
{
    if (!eof && fd < 0)
    {
        fd = open(filename.c_str(), O_RDONLY);
        if (fd < 0)
            error(errno);
        linenum = 1;
    }
    if (!eof && bufpos == bufsize)
    {
        int result = read(fd, buf, INTEXT_BUFSIZE);
        if (result < 0)
            error(errno);
        bufpos = 0;
        bufsize = result;
        eof = result == 0;
    }
}


char InText::preview()
{
    if (bufpos == bufsize)
        validateBuffer();
    if (eof)
        return 0;
    return buf[bufpos];
}


char InText::get()
{
    if (bufpos == bufsize)
        validateBuffer();
    if (eof)
        return 0;
    assert(bufpos < bufsize);
    return buf[bufpos++];
}


void InText::doSkipEol()
{
    newline = true;
    linenum++;
    indent = 0;
}


void InText::skipEol()
{
    char c = preview();
    if (c == '\r')
    {
        get();
        c = preview();
    }
    if (c == '\n')
    {
        get();
        doSkipEol();
    }
}


bool InText::getEol()
{
    char c = preview();
    return eof || c == '\r' || c == '\n';
}


void InText::token(const charset& chars, string& result, bool noresult)
{
    do
    {
        if (bufpos == bufsize)
            validateBuffer();
        if (eof)
            return;
        assert(bufpos < bufsize);
        const char* b = buf + bufpos;
        register const char* p = b;
        register const char* e = buf + bufsize;
        while (p < e && chars[*p])
        {
            switch (*p)
            {
                case '\t': if (newline) indent = ((indent / tabsize) + 1) * tabsize; break;
                case '\n': doSkipEol(); break;
                case ' ': if (newline) indent++; break;
                default: newline = false; break; // freeze indent calculation
            }
            p++;
        }
        bufpos += p - b;
        if (!noresult && p > b)
            result.append(b, p - b);
    }
    while (bufpos == bufsize);
}


string InText::token(const charset& chars) throw(ESysError)
{
    string result;
    token(chars, result, false);
    return result;
}


void InText::skip(const charset& chars) throw(ESysError)
{
    string result;
    token(chars, result, true);
}


void InText::skipTo(char c)
{
    do
    {
        if (bufpos == bufsize)
            validateBuffer();
        if (eof)
            return;
        const char* b = buf + bufpos;
        const char* e = buf + bufsize;
        const char* p = (const char*)memchr(b, c, e - b);
        if (p != NULL)
        {
            bufpos += p - b;
            return;
        }
    }
    while (bufpos == bufsize);
}


void InText::skipLine()
{
    skipTo('\n');
    skipEol();
}



class _AtExit
{
public:
    ~_AtExit()
    {
        if (Base::objCount != 0)
            fprintf(stderr, "Internal: objCount = %d\n", Base::objCount);
        if (stralloc != 0)
            fprintf(stderr, "Internal: stralloc = %d\n", stralloc);
        if (fifoChunkAlloc != 0)
            fprintf(stderr, "Internal: fifoChunkAlloc = %d\n", fifoChunkAlloc);
    }
} _atexit;



int main()
{
    const charset idchars = "A-Za-z_0-9";
    const charset specials = "`!\"$%^&*()_+=:@;'#<>?,./|\\~-~~";
    const charset wschars = "\t ";

    InText in("tests/intext.txt");
    try
    {
        assert('T' == in.preview());
        assert("Thunder" == in.token(idchars));
        assert(0 == in.getIndent());
        assert("," == in.token(specials));
        assert(" " == in.token(wschars));
        in.skipLine();
        assert(' ' == in.preview());
        assert(2 == in.getLinenum());
        in.skip(wschars);
        assert(4 == in.getIndent());
    }
    catch (Exception& e)
    {
        printf("Exception: %s\n", e.what().c_str());
        assert(false);
    }
    return 0;
}

