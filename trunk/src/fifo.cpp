

#include "runtime.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


// --- fifo ---------------------------------------------------------------- //


#ifdef DEBUG
mem memfifo::CHUNK_SIZE = sizeof(variant) * 16;
#endif


fifo::fifo(Type* rt, bool is_char)
    : object(rt), _char(is_char)  { }
fifo::~fifo()
    { }

void fifo::_empty_err()                { throw emessage("FIFO empty"); }
void fifo::_wronly_err()               { throw emessage("FIFO is write-only"); }
void fifo::_rdonly_err()               { throw emessage("FIFO is read-only"); }
void fifo::_fifo_type_err()            { fatal(0x1002, "FIFO type mismatch"); }
const char* fifo::get_tail()           { _wronly_err(); return NULL; }
const char* fifo::get_tail(mem*)       { _wronly_err(); return NULL; }
void fifo::deq_bytes(mem)              { _wronly_err(); }
variant* fifo::enq_var()               { _rdonly_err(); return NULL; }
mem fifo::enq_chars(const char*, mem)  { _rdonly_err(); return 0; }
bool fifo::empty() const               { _rdonly_err(); return true; }
void fifo::flush()                     { }


void fifo::_req_non_empty() const
{
    if (empty())
        _empty_err();
}


void fifo::_req_non_empty(bool ch) const
{
    _req(ch);
    if (empty())
        _empty_err();
}


int fifo::preview()
{
    _req(true);
    const char* p = get_tail();
    if (p == NULL)
        return -1;
    return *p;
}


uchar fifo::get()
{
    int c = preview();
    if (c == -1)
        _empty_err();
    deq_bytes(1);
    return c;
}


bool fifo::get_if(char c)
{
    int d = preview();
    if (d != -1 && d == c)
    {
        deq_bytes(1);
        return true;
    }
    return false;
}


bool fifo::eol()
{
    _req(true);
    const char* p = get_tail();
    if (p == NULL)
        return true;
    return *p == '\r' || *p == '\n';
}


void fifo::skip_eol()
{
    // Support all 3 models: DOS, UNIX and MacOS
    int c = preview();
    if (c == '\r')
    {
        get();
        c = preview();
    }
    if (c == '\n')
        get();
}


int fifo::skip_indent()
{
    static const charset ws = " \t";
    int result = 0;
    str s = deq(ws);
    for (str::const_iterator p = s.begin(); p != s.end(); p++)
        switch (*p)
        {
        case ' ': result++; break;
        case '\t': result = ((result / TAB_SIZE) + 1) * TAB_SIZE; break;
        }
    return result;
}


void fifo::var_eat()
{
    if (is_char_fifo())
        get();
    else
    {
        _req_non_empty();
        ((variant*)get_tail())->~variant();
        deq_bytes(sizeof(variant));
    }
}


void fifo::var_preview(variant& v)
{
    if (empty())
        v.clear();
    else if (is_char_fifo())
        v = *get_tail();
    else
        v = *(variant*)get_tail();
}


void fifo::var_deq(variant& v)
{
    if (is_char_fifo())
        v = get();
    else
    {
        _req_non_empty();
        v.clear();
        memcpy((char*)&v, get_tail(), sizeof(variant));
        deq_bytes(sizeof(variant));
    }
}


void fifo::var_enq(const variant& v)
{
    if (is_char_fifo())
    {
        if (v.is_str())
            enq(v.as_str());
        else
            enq(v.as_char());
    }
    else
        ::new(enq_var()) variant(v);
}


str fifo::deq(mem count)
{
    _req_non_empty(true);
    str result;
    while (count > 0)
    {
        mem avail;
        const char* p = get_tail(&avail);
        if (p == NULL)
            break;
        if (count < avail)
            avail = count;
        result.append(p, avail);
        deq_bytes(avail);
        if (count == CHAR_SOME)
            break;
        count -= avail;
    }
    return result;
}


void fifo::_token(const charset& chars, str* result)
{
    _req(true);
    while (1)
    {
        mem avail;
        const char* b = get_tail(&avail);
        if (b == NULL)
            break;
        const char* p = b;
        const char* e = b + avail;
        while (p < e && chars[*p])
            p++;
        mem count = p - b;
        if (count == 0)
            break;
        if (result != NULL)
            result->append(b, count);
        deq_bytes(count);
        if (count < avail)
            break;
    }
}


str fifo::line()
{
    static charset linechars = ~charset("\r\n");
    str result;
    _token(linechars, &result);
    skip_eol();
    return result;
}


void fifo::enq(const char* s)  { if (s != NULL) enq(s, strlen(s)); }
void fifo::enq(const str& s)   { enq_chars(s.data(), s.size()); }
void fifo::enq(char c)         { enq_chars(&c, 1); }
void fifo::enq(uchar c)        { enq_chars((char*)&c, 1); }
void fifo::enq(long long i)    { enq(to_string(i)); }


// --- fifo ---------------------------------------------------------------- //


memfifo::memfifo(Type* rt, bool ch)
    : fifo(rt, ch), head(NULL), tail(NULL), head_offs(0), tail_offs(0)  { }


memfifo::~memfifo()                     { clear(); }
inline const char* memfifo::get_tail()  { return tail->data + tail_offs; }
inline bool memfifo::empty() const      { return tail == NULL; }
inline variant* memfifo::enq_var()      { _req(false); return (variant*)enq_space(sizeof(variant)); }


void memfifo::clear()
{
    // TODO: also define fifos for POD variant types for faster destruction
    if (is_char_fifo())
    {
        while (tail != NULL)
        {
#ifdef DEBUG
            head_offs = tail_offs = CHUNK_SIZE;
#endif
            deq_chunk();
        }
    }
    else
    {
        while (tail != NULL)
        {
            ((variant*)get_tail())->~variant();
            deq_bytes(sizeof(variant));
        }
    }
}


void memfifo::deq_chunk()
{
    assert(tail != NULL && head != NULL);
    chunk* c = tail;
    tail = tail->next;
    delete c;
    if (tail == NULL)
    {
        assert(head_offs == tail_offs);
        head = NULL;
        head_offs = tail_offs = 0;
    }
    else
    {
        assert(tail_offs == CHUNK_SIZE);
        tail_offs = 0;
    }
}


void memfifo::enq_chunk()
{
    chunk* c = new chunk();
    if (head == NULL)
    {
        assert(tail == NULL && head_offs == 0);
        head = tail = c;
    }
    else
    {
        assert(head_offs == CHUNK_SIZE);
        head->next = c;
        head = c;
        head_offs = 0;
    }
}


const char* memfifo::get_tail(mem* count)
{
    if (tail == NULL)
    {
        *count = 0;
        return NULL;
    }
    if (tail == head)
        *count = head_offs - tail_offs;
    else
        *count = CHUNK_SIZE - tail_offs;
    assert(*count <= CHUNK_SIZE);
    return tail->data + tail_offs;
}


void memfifo::deq_bytes(mem count)
{
    tail_offs += count;
    assert(tail != NULL && tail_offs <= ((tail == head) ? head_offs : CHUNK_SIZE));
    if (tail_offs == ((tail == head) ? head_offs : CHUNK_SIZE))
        deq_chunk();
}


mem memfifo::enq_avail()
{
    if (head == NULL || head_offs == CHUNK_SIZE)
        return CHUNK_SIZE;
    return CHUNK_SIZE - head_offs;
}


char* memfifo::enq_space(mem count)
{
    if (head == NULL || head_offs == CHUNK_SIZE)
        enq_chunk();
    assert(count <= CHUNK_SIZE - head_offs);
    char* result = head->data + head_offs;
    head_offs += count;
    return result;
}


mem memfifo::enq_chars(const char* p, mem count)
{
    _req(true);
    mem save_count = count;
    while (count > 0)
    {
        mem avail = enq_avail();
        if (count < avail)
            avail = count;
        memcpy(enq_space(avail), p, avail);
        count -= avail;
        p += avail;
    }
    return save_count;
}


// --- buffifo ------------------------------------------------------------- //


buffifo::buffifo(Type* rt, bool is_char)
  : fifo(rt, is_char), buffer(NULL), bufsize(0), bufhead(0), buftail(0)  { }

buffifo::~buffifo()  { }
bool buffifo::empty() const { _wronly_err(); return true; }
void buffifo::flush() { _rdonly_err(); }


const char* buffifo::get_tail()
{
    assert(buftail <= bufhead && bufhead <= bufsize);
    if (buftail == bufhead && empty())
        return NULL;
    assert(bufhead > buftail);
    return buffer + buftail;
}


const char* buffifo::get_tail(mem* count)
{
    assert(buftail <= bufhead && bufhead <= bufsize);
    if (buftail == bufhead && empty())
    {
        *count = 0;
        return NULL;
    }
    *count = bufhead - buftail;
    assert(*count >= (is_char_fifo() ? 1 : sizeof(variant)));
    return buffer + buftail;
}


void buffifo::deq_bytes(mem count)
{
    assert(buftail <= bufhead && bufhead <= bufsize);
    assert(count <= bufhead - buftail);
    buftail += count;
}


variant* buffifo::enq_var()
{
    _req(false);
    assert(buftail <= bufhead && bufhead <= bufsize);
    if (bufhead + sizeof(variant) > bufsize)
        flush();
    assert(bufhead + sizeof(variant) <= bufsize);
    variant* result = (variant*)(buffer + bufhead);
    bufhead += sizeof(variant);
    return result;
}


mem buffifo::enq_avail()
{
    assert(buftail <= bufhead && bufhead <= bufsize);
    if (bufhead == bufsize)
        flush();
    assert(bufhead < bufsize);
    return bufsize - bufhead;
}


char* buffifo::enq_space(mem count)
{
    assert(buftail <= bufhead && bufhead <= bufsize);
    assert(count <= bufsize - bufhead);
    char* result = buffer + bufhead;
    bufhead += count;
    return result;
}


mem buffifo::enq_chars(const char* p, mem count)
{
    _req(true);
    mem save_count = count;
    while (count > 0)
    {
        mem avail = enq_avail();
        if (count < avail)
            avail = count;
        memcpy(enq_space(avail), p, avail);
        count -= avail;
        p += avail;
    }
    return save_count;
}


// --- strfifo ------------------------------------------------------------- //


strfifo::strfifo(Type* rt): buffifo(rt, true), string() {}
strfifo::~strfifo() { }


strfifo::strfifo(Type* rt, const str& s)
    : buffifo(rt, true), string(s)
{
    buffer = (char*)s.data();
    bufhead = bufsize = s.size();
}


void strfifo::clear()
{
    string.clear();
    buffer = NULL;
    buftail = bufhead = bufsize = 0;
}


bool strfifo::empty() const
{
    if (buftail == bufhead)
    {
        if (!string.empty())
            ((strfifo*)this)->clear();
        return true;
    }
    return false;
}


void strfifo::flush()
{
    assert(bufhead == bufsize);
    string.resize(string.size() + memfifo::CHUNK_SIZE);
    buffer = (char*)string.data();
    bufsize += memfifo::CHUNK_SIZE;
}


str strfifo::all() const
{
    if (string.empty() || buftail == bufhead)
        return null_str;
    return string.substr(buftail, bufhead - buftail);
}


// --- intext -------------------------------------------------------------- //


intext::intext(Type* rt, const str& fn)
    : buffifo(rt, true), file_name(fn), _fd(-1), _eof(false)  { }
intext::~intext()
    { if (_fd > 2) ::close(_fd); }
void intext::error(int code)
    { _eof = true; throw esyserr(code, file_name); }


void intext::doopen()
{
    _fd = ::open(file_name.c_str(), O_RDONLY | O_LARGEFILE);
    if (_fd < 0)
        error(errno);
    bufsize = bufhead = buftail = 0;
}


void intext::doread()
{
    filebuf.resize(intext::BUF_SIZE);
    buffer = (char*)filebuf.data();
    int result = ::read(_fd, buffer, intext::BUF_SIZE);
    if (result < 0)
        error(errno);
    buftail = 0;
    bufsize = bufhead = result;
    _eof = result == 0;
}


bool intext::empty() const
{
    if (_eof)
        return true;
    if (_fd < 0)
        ((intext*)this)->doopen();
    if (buftail == bufhead)
        ((intext*)this)->doread();
    return _eof;
}


// --- outtext -------------------------------------------------------------- //


outtext::outtext(Type* rt, const str& fn)
    : buffifo(rt, true), file_name(fn), _fd(-1), _err(false)
{
    filebuf.resize(outtext::BUF_SIZE);
    buffer = (char*)filebuf.data();
    bufsize = outtext::BUF_SIZE;
}

outtext::~outtext()
{
    try
    {
        flush();
    }
    catch (exception&)
    {
    }
    if (_fd > 2)
    {
        ::close(_fd);
    }
}


void outtext::error(int code)
    { _err = true; throw esyserr(code, file_name); }


void outtext::flush()
{
    if (_err)
        return;
    if (bufhead > 0)
    {
        if (_fd < 0)
        {
            _fd = ::open(file_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE, 0644);
            if (_fd < 0)
                error(errno);
        }
        int ret = ::write(_fd, buffer, bufhead);
        if (ret < 0)
            error(errno);
        bufhead = 0;
    }
}


// --- stdfile ------------------------------------------------------------- //


stdfile::stdfile(int infd, int outfd)
    : intext(NULL, "<std>"), _ofd(outfd)
{
    _fd = infd;
    if (infd == -1)
        _eof = true;

    pincrement(&refcount);  // prevent auto pointers from freeing this object,
                            // as it is supposed to be static
#ifdef DEBUG
    object::alloc--;        // compensate static objects
#endif
}


stdfile::~stdfile()                                { pdecrement(&refcount); }
mem stdfile::enq_chars(const char* p, mem count)   { return ::write(_ofd, p, count); }


stdfile sio(STDIN_FILENO, STDOUT_FILENO);
stdfile serr(-1, STDERR_FILENO);


