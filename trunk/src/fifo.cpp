

#include "common.h"
#include "runtime.h"

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


// --- fifo_intf ----------------------------------------------------------- //


#ifdef DEBUG
mem fifo::CHUNK_SIZE = sizeof(variant) * 16;
#endif


fifo_intf::fifo_intf(Type* rt, bool is_char)
    : object(rt), _char(is_char)  { }
fifo_intf::~fifo_intf() { }

void fifo_intf::_empty_err()                { throw emessage("FIFO empty"); }
void fifo_intf::_wronly_err()               { throw emessage("FIFO is write-only"); }
void fifo_intf::_rdonly_err()               { throw emessage("FIFO is read-only"); }
void fifo_intf::_fifo_type_err()            { fatal(0x1002, "FIFO type mismatch"); }
const char* fifo_intf::get_tail()           { _wronly_err(); return NULL; }
const char* fifo_intf::get_tail(mem*)       { _wronly_err(); return NULL; }
void fifo_intf::deq_bytes(mem)              { _wronly_err(); }
variant* fifo_intf::enq_var()               { _rdonly_err(); return NULL; }
mem fifo_intf::enq_chars(const char*, mem)  { _rdonly_err(); return 0; }
bool fifo_intf::empty()                     { _rdonly_err(); return true; }
void fifo_intf::dump(fifo_intf& s) const    { s << (is_char_fifo() ? "<char-fifo>" : "<fifo>"); }


fifo::fifo(Type* rt, bool _char)
    : fifo_intf(rt, _char), head(NULL), tail(NULL), head_offs(0), tail_offs(0)  { }


void fifo_intf::_req_non_empty()
{
    if (empty())
        _empty_err();
}


void fifo_intf::_req_non_empty(bool _char)
{
    _req(_char);
    if (empty())
        _empty_err();
}


int fifo_intf::preview()
{
    _req(true);
    const char* p = get_tail();
    if (p == NULL)
        return -1;
    return *p;
}


uchar fifo_intf::get()
{
    int c = preview();
    if (c == -1)
        _empty_err();
    deq_bytes(1);
    return c;
}


bool fifo_intf::get_if(char c)
{
    int d = preview();
    if (d != -1 && d == c)
    {
        deq_bytes(1);
        return true;
    }
    return false;
}


bool fifo_intf::eol()
{
    _req(true);
    const char* p = get_tail();
    if (p == NULL)
        return true;
    return *p == '\r' || *p == '\n';
}


void fifo_intf::skip_eol()
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


int fifo_intf::skip_indent()
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


void fifo_intf::var_eat()
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


void fifo_intf::var_preview(variant& v)
{
    if (empty())
        v.clear();
    else if (is_char_fifo())
        v = *get_tail();
    else
        v = *(variant*)get_tail();
}


void fifo_intf::var_deq(variant& v)
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


void fifo_intf::var_enq(const variant& v)
{
    if (is_char_fifo())
    {
        if (v.is(variant::STR))
            enq(v.as_str());
        else
            enq(v.as_char());
    }
    else
        ::new(enq_var()) variant(v);
}


str fifo_intf::deq(mem count)
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


void fifo_intf::_token(const charset& chars, str* result)
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


void fifo_intf::enq(const char* s)  { if (s != NULL) enq(s, strlen(s)); }
void fifo_intf::enq(const str& s)   { enq_chars(s.data(), s.size()); }
void fifo_intf::enq(char c)         { enq_chars(&c, 1); }
void fifo_intf::enq(uchar c)        { enq_chars((char*)&c, 1); }
void fifo_intf::enq(long long i)    { enq(to_string(i)); }


// --- fifo ---------------------------------------------------------------- //


fifo::~fifo()                       { clear(); }
inline const char* fifo::get_tail() { return tail->data + tail_offs; }
inline bool fifo::empty()           { return tail == NULL; }
inline variant* fifo::enq_var()     { _req(false); return (variant*)enq_space(sizeof(variant)); }


void fifo::clear()
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


void fifo::deq_chunk()
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


void fifo::enq_chunk()
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


const char* fifo::get_tail(mem* count)
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


void fifo::deq_bytes(mem count)
{
    tail_offs += count;
    assert(tail != NULL && tail_offs <= ((tail == head) ? head_offs : CHUNK_SIZE));
    if (tail_offs == ((tail == head) ? head_offs : CHUNK_SIZE))
        deq_chunk();
}


mem fifo::enq_avail()
{
    if (head == NULL || head_offs == CHUNK_SIZE)
        return CHUNK_SIZE;
    return CHUNK_SIZE - head_offs;
}


char* fifo::enq_space(mem count)
{
    if (head == NULL || head_offs == CHUNK_SIZE)
        enq_chunk();
    assert(count <= CHUNK_SIZE - head_offs);
    char* result = head->data + head_offs;
    head_offs += count;
    return result;
}


mem fifo::enq_chars(const char* p, mem count)
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


/*
void fifo::dump(fifo_intf& s) const
{
    chunk* c = tail;
    int cnt = 0;
    while (c != NULL)
    {
        const char* b = c->data + (c == tail ? tail_offs : 0);
        const char* e = c->data + (c == head ? head_offs : CHUNK_SIZE);
        if (is_char_fifo())
            s.write(b, e - b);
        else
        {
            while (b < e)
            {
                if (++cnt > 1)
                    s << ", ";
                ((variant*)b)->dump(s);
                b += sizeof(variant);
            }
        }
        c = c->next;
    }
}
*/


// --- buf_fifo ------------------------------------------------------------ //


buf_fifo::buf_fifo(Type* rt, bool is_char)
  : fifo_intf(rt, is_char), buffer(NULL), bufsize(0), bufhead(0), buftail(0)  { }

buf_fifo::~buf_fifo()  { }
bool buf_fifo::empty() { _wronly_err(); return true; }
void buf_fifo::flush() { _rdonly_err(); }


const char* buf_fifo::get_tail()
{
    assert(buftail <= bufhead && bufhead <= bufsize);
    if (buftail == bufhead && empty())
        return NULL;
    assert(bufhead > buftail);
    return buffer + buftail;
}


const char* buf_fifo::get_tail(mem* count)
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


void buf_fifo::deq_bytes(mem count)
{
    assert(buftail <= bufhead && bufhead <= bufsize);
    assert(count <= bufhead - buftail);
    buftail += count;
}


variant* buf_fifo::enq_var()
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


mem buf_fifo::enq_avail()
{
    assert(buftail <= bufhead && bufhead <= bufsize);
    if (bufhead == bufsize)
        flush();
    assert(bufhead < bufsize);
    return bufsize - bufhead;
}


char* buf_fifo::enq_space(mem count)
{
    assert(buftail <= bufhead && bufhead <= bufsize);
    assert(count <= bufsize - bufhead);
    char* result = buffer + bufhead;
    bufhead += count;
    return result;
}


// TODO: this is a copy-paste of fifo::enq_chars(). Shame.
mem buf_fifo::enq_chars(const char* p, mem count)
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


// --- str_fifo ------------------------------------------------------------ //


str_fifo::str_fifo(Type* rt): buf_fifo(rt, true), string() {}
str_fifo::~str_fifo() { }


str_fifo::str_fifo(Type* rt, const str& s)
    : buf_fifo(rt, true), string(s)
{
    buffer = (char*)s.data();
    bufhead = bufsize = s.size();
}


bool str_fifo::empty()
{
    if (buftail == bufhead)
    {
        if (!string.empty())
        {
            string.clear();
            buffer = NULL;
            buftail = bufhead = bufsize = 0;
        }
        return true;
    }
    return false;
}


void str_fifo::flush()
{
    assert(bufhead == bufsize);
    string.resize(string.size() + fifo::CHUNK_SIZE);
    buffer = (char*)string.data();
    bufsize += fifo::CHUNK_SIZE;
}


str str_fifo::all() const
{
    if (string.empty() || buftail == bufhead)
        return null_str;
    return string.substr(buftail, bufhead - buftail);
}


// --- out_file ------------------------------------------------------------ //


std_file::std_file(Type* rt, int fd)
    : fifo_intf(rt, true), _fd(fd)
{
    pincrement(&refcount);  // prevent auto pointers from freeing this object,
                            // as it is supposed to be static
#ifdef DEBUG
    object::alloc--;        // compensate static objects
#endif
}


std_file::~std_file()                               { pdecrement(&refcount); }
mem std_file::enq_chars(const char* p, mem count)   { return ::write(_fd, p, count); }


std_file fout(NULL, STDIN_FILENO);
std_file ferr(NULL, STDERR_FILENO);


// --- in_text ------------------------------------------------------------- //


in_text::in_text(Type* rt, const str& fn)
    : buf_fifo(rt, true), file_name(fn), _fd(-1), _eof(false)  { }
in_text::~in_text()             { if (_fd >= 0) ::close(_fd); }
void in_text::error(int code)   { _eof = true; throw esyserr(code, file_name); }


bool in_text::empty()
{
    if (_eof)
        return true;
    if (_fd < 0)
    {
        _fd = ::open(file_name.c_str(), O_RDONLY | O_LARGEFILE);
        if (_fd < 0)
            error(errno);
        bufsize = bufhead = buftail = 0;
    }
    if (buftail == bufhead)
    {
        filebuf.resize(in_text::BUF_SIZE);
        buffer = (char*)filebuf.data();
        int result = ::read(_fd, buffer, in_text::BUF_SIZE);
        if (result < 0)
            error(errno);
        buftail = 0;
        bufsize = bufhead = result;
        _eof = result == 0;
    }
    return _eof;
}

