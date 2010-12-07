
#include "runtime.h"


#ifdef DEBUG
int memfifo::CHUNK_SIZE = 32 * _varsize;
int intext::BUF_SIZE = 4096 * int(sizeof(integer));
#endif


charset non_eol_chars = ~charset("\r\n");


fifo::fifo(Type* rt, bool is_char) throw()
    : rtobject(rt), max_token(0), _is_char_fifo(is_char)  { }

fifo::~fifo() throw()
    { }

void fifo::dump(fifo& stm) const        { stm << "fifo:" << get_name(); }
void fifo::_empty_err()                 { throw efifo("FIFO empty"); }
void fifo::_full_err()                  { throw efifo("FIFO full"); }
void fifo::_wronly_err()                { throw efifo("FIFO is write-only"); }
void fifo::_rdonly_err()                { throw efifo("FIFO is read-only"); }
void fifo::_fifo_type_err()             { fatal(0x2001, "FIFO type mismatch"); }
void fifo::_token_err()                 { throw efifo("Token too long"); }
const char* fifo::get_tail()            { _wronly_err(); return NULL; }
const char* fifo::get_tail(memint*)     { _wronly_err(); return NULL; }
void fifo::deq_bytes(memint)            { _wronly_err(); }
variant* fifo::enq_var()                { _rdonly_err(); return NULL; }
void fifo::enq_char(char)               { _rdonly_err(); }
memint fifo::enq_chars(const char*, memint) { _rdonly_err(); return 0; }
bool fifo::empty() const                { _rdonly_err(); return true; }
void fifo::flush()                      { }


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


void fifo::deq_var(variant* v)
{
    _req_non_empty(false);
    *(podvar*)v = *(podvar*)get_tail();
    deq_bytes(_varsize);
}


#ifdef DEBUG
// Used only in unit tests
void fifo::var_eat()
{
    if (is_char_fifo())
        get();
    else
    {
        _req_non_empty();
        ((variant*)get_tail())->~variant();
        deq_bytes(_varsize);
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
        v.clear();
        deq_var(&v);
    }
}


void fifo::var_enq(const variant& v)
{
    if (is_char_fifo())
    {
        if (v.is(variant::STR))
            enq(v._str());
        else if (v.is(variant::ORD))
            enq(v._uchar());
        else
            variant::_type_err();
    }
    else
        ::new(enq_var()) variant(v);
}
#endif


str fifo::deq(memint count)
{
    _req_non_empty(true);
    str result;
    while (count > 0)
    {
        memint avail;
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
    memint total = 0;
    while (1)
    {
        memint avail;
        const char* b = get_tail(&avail);
        if (b == NULL)
            break;
        const char* p = b;
        const char* e = b + avail;
        while (p < e && chars[*p])
            p++;
        memint count = p - b;
        if (count == 0)
            break;
        if (max_token > 0)
        {
            total += count;
            if (total > max_token)
                _token_err();
        }
        if (result != NULL)
            result->append(b, count);
        deq_bytes(count);
        if (count < avail)
            break;
    }
}


str fifo::line()
{
    str result;
    _token(non_eol_chars, &result);
    skip_eol();
    return result;
}


void fifo::enq(const char* s)   { if (s != NULL) enq(s, strlen(s)); }
void fifo::enq(const str& s)    { enq_chars(s.data(), s.size()); }
void fifo::enq(large i)         { enq(to_string(i)); }


void fifo::enq(const varvec& v)
{
    _req(false);
    for (memint i = 0; i < v.size() - 1; i++)
        new(enq_var()) variant(v[i]);
}


// --- memfifo ------------------------------------------------------------- //


memfifo::memfifo(Type* rt, bool ch) throw()
    : fifo(rt, ch), head(NULL), tail(NULL), head_offs(0), tail_offs(0)  { }


memfifo::~memfifo() throw()             { try { clear(); } catch(exception&) { } }
inline const char* memfifo::get_tail()  { return tail ? (tail->data + tail_offs) : NULL; }
inline bool memfifo::empty() const      { return tail == NULL; }
inline variant* memfifo::enq_var()      { _req(false); return (variant*)enq_space(_varsize); }
str memfifo::get_name() const           { return "<memfifo>"; }


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
            deq_bytes(_varsize);
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


const char* memfifo::get_tail(memint* count)
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


void memfifo::deq_bytes(memint count)
{
    assert(tail != NULL && (tail_offs + count) <= ((tail == head) ? head_offs : CHUNK_SIZE));
    tail_offs += int(count);
    if (tail_offs == ((tail == head) ? head_offs : CHUNK_SIZE))
        deq_chunk();
}


memint memfifo::enq_avail()
{
    if (head == NULL || head_offs == CHUNK_SIZE)
        return CHUNK_SIZE;
    return CHUNK_SIZE - head_offs;
}


char* memfifo::enq_space(memint count)
{
    if (head == NULL || head_offs == CHUNK_SIZE)
        enq_chunk();
    assert(count <= CHUNK_SIZE - head_offs);
    char* result = head->data + head_offs;
    head_offs += int(count);
    return result;
}


void memfifo::enq_char(char c)
{
    _req(true);
    *enq_space(1) = c;
}


memint memfifo::enq_chars(const char* p, memint count)
{
    _req(true);
    memint save_count = count;
    while (count > 0)
    {
        memint avail = enq_avail();
        if (count < avail)
            avail = count;
        memcpy(enq_space(avail), p, avail);
        count -= avail;
        p += avail;
    }
    return save_count;
}


// --- buffifo ------------------------------------------------------------- //


buffifo::buffifo(Type* rt, bool is_char) throw()
  : fifo(rt, is_char), buffer(NULL), bufsize(0), bufhead(0), buftail(0), buforig(0),
    event(NULL)  { }

buffifo::~buffifo() throw() { }
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


const char* buffifo::get_tail(memint* count)
{
    assert(buftail <= bufhead && bufhead <= bufsize);
    if (buftail == bufhead && empty())
    {
        *count = 0;
        return NULL;
    }
    *count = bufhead - buftail;
    assert(*count >= (is_char_fifo() ? 1 : _varsize));
    return buffer + buftail;
}


void buffifo::deq_bytes(memint count)
{
    assert(buftail <= bufhead && bufhead <= bufsize);
    assert(count <= bufhead - buftail);
    buftail += count;
}


variant* buffifo::enq_var()
{
    _req(false);
    assert(buftail <= bufhead && bufhead <= bufsize);
    if (bufhead + _varsize > bufsize)
        flush();
    assert(bufhead + _varsize <= bufsize);
    variant* result = (variant*)(buffer + bufhead);
    bufhead += _varsize;
    return result;
}


memint buffifo::enq_avail()
{
    assert(buftail <= bufhead && bufhead <= bufsize);
    if (bufhead == bufsize)
        flush();
    assert(bufhead < bufsize);
    return bufsize - bufhead;
}


char* buffifo::enq_space(memint count)
{
    assert(buftail <= bufhead && bufhead <= bufsize);
    assert(count <= bufsize - bufhead);
    char* result = buffer + bufhead;
    bufhead += count;
    return result;
}


void buffifo::enq_char(char c)
{
    _req(true);
    assert(buftail <= bufhead && bufhead <= bufsize);
    if (bufhead + 1 > bufsize)
        flush();
    assert(bufhead + 1 <= bufsize);
    buffer[bufhead] = c;
    bufhead++;
}


memint buffifo::enq_chars(const char* p, memint count)
{
    _req(true);
    memint save_count = count;
    while (count > 0)
    {
        memint avail = enq_avail();
        if (count < avail)
            avail = count;
        memcpy(enq_space(avail), p, avail);
        count -= avail;
        p += avail;
    }
    return save_count;
}


void buffifo::call_bufevent() const
{
    if (event)
        event->event(buffer, buftail, bufhead);
}


bufevent* buffifo::set_bufevent(bufevent* e)
{
    bufevent* prev = event;
    call_bufevent();
    event = e;
    call_bufevent();
    return prev;
}


// --- strfifo ------------------------------------------------------------- //


strfifo::strfifo(Type* rt) throw()  : buffifo(rt, true), string()  {}
strfifo::~strfifo() throw()         { }
str strfifo::get_name() const       { return "<strfifo>"; }


strfifo::strfifo(Type* rt, const str& s) throw()
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
        call_bufevent();
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
        return str();
    return string.substr(buftail, bufhead - buftail);
}


// --- intext -------------------------------------------------------------- //


// *BSD/Darwin hack
#ifndef O_LARGEFILE
#  define O_LARGEFILE 0
#endif


intext::intext(Type* rt, const str& fn) throw()
    : buffifo(rt, true), file_name(fn), _fd(-1), _eof(false)  { }

intext::~intext() throw()       { if (_fd > 2) ::close(_fd); }
void intext::error(int code)    { _eof = true; throw esyserr(code, file_name); }
str intext::get_name() const    { return file_name; }


void intext::doopen()
{
    _fd = ::open(file_name.c_str(), O_RDONLY | O_LARGEFILE);
    if (_fd < 0)
        error(errno);
    bufsize = bufhead = buftail = 0;
}


void intext::doread()
{
    call_bufevent();
    filebuf.resize(intext::BUF_SIZE);
    buffer = (char*)filebuf.data();
    memint result = ::read(_fd, buffer, intext::BUF_SIZE);
    if (result < 0)
        error(errno);
    buforig += bufhead;
    buftail = 0;
    bufsize = bufhead = result;
    _eof = result == 0;
    call_bufevent();
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


outtext::outtext(Type* rt, const str& fn) throw()
    : buffifo(rt, true), file_name(fn), _fd(-1), _err(false)
{
    filebuf.resize(outtext::BUF_SIZE);
    buffer = (char*)filebuf.data();
    bufsize = outtext::BUF_SIZE;
}


outtext::~outtext() throw()
{
    try
        { flush(); }
    catch (exception&)
        { }
    if (_fd > 2)
        ::close(_fd);
}


void outtext::error(int code)
    { _err = true; throw esyserr(code, file_name); }


str outtext::get_name() const
    { return file_name; }


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
        memint ret = ::write(_fd, buffer, bufhead);
        if (ret < 0)
            error(errno);
        buforig += bufhead;
        bufhead = 0;
    }
}


// --- stdfile ------------------------------------------------------------- //


stdfile::stdfile(int infd, int outfd) throw()
    : intext(NULL, "<stdio>"), _ofd(outfd)
{
    _fd = infd;
    if (infd == -1)
        _eof = true;
    _mkstatic();
}

stdfile::~stdfile() throw()
    { }

void stdfile::enq_char(char c)
    { if (::write(_ofd, &c, 1) < 1) _full_err(); }

memint stdfile::enq_chars(const char* p, memint count)
    { return ::write(_ofd, p, count); }



stdfile sio(STDIN_FILENO, STDOUT_FILENO);
stdfile serr(-1, STDERR_FILENO);


// --- System utilities ---------------------------------------------------- //


enum FileType
{
    FT_FILE, 
    FT_DIRECTORY, 
    FT_OTHER,       // device or pipe
    FT_ERROR = -1
};


static FileType getFileType(const char* path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return FT_ERROR;
    if ((st.st_mode & S_IFDIR) == S_IFDIR)
        return FT_DIRECTORY;
    if ((st.st_mode & S_IFREG) == S_IFREG)
        return FT_FILE;
    return FT_OTHER;
}


bool isFile(const char* path)
    { return getFileType(path) == FT_FILE; }

