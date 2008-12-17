
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


#define FIFO_CHUNK_SIZE int(sizeof(quant) * 16)


extern int fifochunkalloc;


struct FifoChunk
{
    char* data;
    FifoChunk();
    FifoChunk(const FifoChunk& f);
    ~FifoChunk();
};


class fifoimpl: private Array<FifoChunk>
{
public:
    short left, right;

    fifoimpl();
    fifoimpl(const fifoimpl&);
    ~fifoimpl();
    void operator= (const fifoimpl&);

    void* _at(int) const;
    FifoChunk& _chunkat(int i) const  { return Array<FifoChunk>::_at(i); }
    int chunks() const                { return Array<FifoChunk>::size(); }

    void push(const char*, int);
    int  pull(char*, int);
    int  size() const;
    void* at(int i) const
    {
#ifdef DEBUG
        if (unsigned(i) >= unsigned(size()))
            idxerror();
#endif
        return _at(i);
    }
};


template <class T>
class PodFifo: private fifoimpl
{
public:
    PodFifo(): fifoimpl()  { }
    PodFifo(const PodFifo& f): fifoimpl(f)  { }
    ~PodFifo()  { }
};


// ------------------------------------------------------------------------ //


FifoChunk::FifoChunk()
{
#ifdef DEBUG
    fifochunkalloc++;
#endif
    data = (char*)memalloc(FIFO_CHUNK_SIZE);
}

FifoChunk::FifoChunk(const FifoChunk& f)
{
#ifdef DEBUG
    fifochunkalloc++;
#endif
    data = (char*)memalloc(FIFO_CHUNK_SIZE);
    memcpy(data, f.data, FIFO_CHUNK_SIZE);
}

FifoChunk::~FifoChunk()
{
    memfree(data);
#ifdef DEBUG
    fifochunkalloc--;
#endif
}


fifoimpl::fifoimpl()
    : Array<FifoChunk>(), left(0), right(0)  { }


fifoimpl::fifoimpl(const fifoimpl& f)
    : Array<FifoChunk>(f), left(f.left), right(f.right)  { }


fifoimpl::~fifoimpl()
{
}


void fifoimpl::operator= (const fifoimpl& f)
{
    clear();
    Array<FifoChunk>::operator= (f);
    left = f.left;
    right = f.right;
}


void* fifoimpl::_at(int i) const
{
    i += left;
    return Array<FifoChunk>::_at(i / FIFO_CHUNK_SIZE).data
        + i % FIFO_CHUNK_SIZE;
}


int fifoimpl::size() const
{
    return empty() ? 0 : (chunks() - 1) * FIFO_CHUNK_SIZE - left + right;
}


void fifoimpl::push(const char* data, int datasize)
{
    if (datasize > 0 && empty())
    {
        Array<FifoChunk>::add();
        right = 0;
    }
    while (datasize > 0)
    {
        int len = imin(FIFO_CHUNK_SIZE - right, datasize);
        memcpy(_chunkat(chunks() - 1).data + right, data, len);
        right += len;
        datasize -= len;
        if (datasize == 0)
            return;
        data += len;
        Array<FifoChunk>::add();
        right = 0;
    }
}


int fifoimpl::pull(char* data, int datasize)
{
    int result = 0;
    while (datasize > 0 && !empty())
    {
        int curright = chunks() == 1 ? right : FIFO_CHUNK_SIZE;
        int len = imin(curright - left, datasize);
        memcpy(data, _chunkat(0).data + left, len);
        left += len;
        if (left == curright)
        {
            Array<FifoChunk>::del(0);
            left = 0;
        }
        datasize -= len;
        data += len;
        result += len;
    }
    return result;
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
        string s = "abcd";
        f.push(s.c_bytes(), s.size());
        s = "efg";
        f.push(s.c_bytes(), s.size());
        printf("chunk alloc: %d\n", fifochunkalloc);

        fifoimpl g(f);
        printf("chunk alloc: %d\n", fifochunkalloc);

        char buf[256];
        int len = f.pull(buf, 6);
        buf[len] = 0;
        printf("pulled: %s\n", buf);
        printf("chunk alloc: %d\n", fifochunkalloc);
    }
    printf("chunk alloc: %d\n", fifochunkalloc);
    return 0;
}

