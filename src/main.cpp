
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "charset.h"
#include "str.h"
#include "baseobj.h"


// ------------------------------------------------------------------------ //
// --- TEXT FILE READER --------------------------------------------------- //
// ------------------------------------------------------------------------ //

#define INTEXT_BUFSIZE 8192
#define TAB_SIZE 8

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
    
    void error(int code) throw(ESysError);
    void validateBuffer();
    void doSkipEol();
    void token(const charset& chars, string& result, bool skip);

public:
    InText(const string& filename);
    ~InText();
    
    char preview();
    char get();
    void skipEol();
    string token(const charset& chars) throw(ESysError);
    void skip(const charset& chars) throw(ESysError);
};


InText::InText(const string& ifilename)
    : filename(ifilename), fd(0), bufsize(0), bufpos(0), linenum(0),
      indent(0), newline(true), eof(false)
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
                case '\t': if (newline) indent = ((indent / TAB_SIZE) + 1) * TAB_SIZE; break;
                case '\n': doSkipEol(); break;
                case ' ': if (newline) indent++; break;
                default: newline = false; break; // freeze indent calculation
            }
            p++;
        }
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


// ------------------------------------------------------------------------ //
// --- FIFO --------------------------------------------------------------- //
// ------------------------------------------------------------------------ //


#define FIFO_CHUNK_COUNT 16
#define FIFO_CHUNK_SIZE (sizeof(quant) * FIFO_CHUNK_COUNT)

extern int fifochunkalloc;


class FifoChunk
{
public:
    char* data;

    FifoChunk()
    {
#ifdef DEBUG
        fifochunkalloc++;
#endif
        data = (char*)memalloc(FIFO_CHUNK_SIZE);
    }

    FifoChunk(const FifoChunk& f)
    {
#ifdef DEBUG
        fifochunkalloc++;
#endif
        data = (char*)memalloc(FIFO_CHUNK_SIZE);
        memcpy(data, f.data, FIFO_CHUNK_SIZE);
    }

    ~FifoChunk()
    {
        memfree(data);
#ifdef DEBUG
        fifochunkalloc--;
#endif
    }

    void operator= (const FifoChunk& f)
    {
        memcpy(data, f.data, FIFO_CHUNK_SIZE);
    }
};


class fifoimpl: private Array<FifoChunk>
{
protected:
    short left, right;

    void* _at(int) const;

public:
    void push(const void*, int);
    int  pull(void*, int);
    void* preview() const   { return at(0); }
    int size();
};


void* fifoimpl::_at(int i) const
{
    i += left;
    return Array<FifoChunk>::_at(i / FIFO_CHUNK_SIZE).data
        + i % FIFO_CHUNK_SIZE;
}


int fifoimpl::size()
{
    int chunks = Array<FifoChunk>::size();
    if (chunks == 0)
        return 0;
    return (chunks - 1) * FIFO_CHUNK_SIZE + left - right;
}



int fifochunkalloc = 0;



class _AtExit
{
public:
    ~_AtExit()
    {
        if (Base::objCount != 0)
            fprintf(stderr, "Internal: objCount = %d\n", Base::objCount);
        if (stralloc != 0)
            fprintf(stderr, "Internal: stralloc = %d\n", stralloc);
        if (fifochunkalloc != 0)
            fprintf(stderr, "Internal: fifochunkalloc = %d\n", fifochunkalloc);
    }
} _atexit;


int main()
{
    {
        Array<int> a;
        a.add(1);
        a.add(2);
        a.add(3);
        Array<int> b(a);
        a.del(2);
        a.clear();
    }
    {
        fifoimpl f;
    }
    return 0;
}

