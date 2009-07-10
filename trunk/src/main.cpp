
#include <assert.h>
#include <string.h>

#include <iostream>

#include "charset.h"
#include "typesys.h"
#include "source.h"


DEF_EXCEPTION(efifoempty, "Invalid FIFO operation");
DEF_EXCEPTION(efifordonly, "FIFO is read-only");
DEF_EXCEPTION(efifowronly, "FIFO is write-only");


// The abstract FIFO interface. There are 2 modes of operation: variant FIFO
// and character FIFO. Destruction of variants is basically not handled by
// this class to give more flexibility to implementations (e.g. there may be
// buffers shared between 2 fifos or other container objects). If you implement,
// say, only input methods, the default output methods will throw an exception
// with a message "FIFO is read-only", and vice versa. Iterators may be
// implemented in descendant classes but are not supported by default.
class fifo_intf: public object
{
protected:
    bool _char;

    void _empty_err() const;
    void _fifo_type_err() const;
    // TODO: _req() can be empty in RELEASE build
    void _req(bool req_char) const      { if (req_char != _char) _fifo_type_err(); }
    void _req_non_empty(bool _char);

    // Minimal set of methods required for both character and variant FIFO
    // operations. Implementations should guarantee variants will never be
    // fragmented, so that a buffer returned by get_tail() always contains at
    // least sizeof(variant) bytes (8, 12 or 16 bytes depending on the host
    // platform) in variant mode, or at least 1 byte in character mode.
    virtual const char* get_tail();          // Get a pointer to tail data
    virtual const char* get_tail(mem*);      // ... also return the length
    virtual void deq_bytes(mem);             // Discard n consecutive bytes returned by get_tail()
    virtual variant* enq_var();              // Reserve uninitialized space for a variant
    virtual mem enq_chars(const char*, mem); // Push arbitrary number of bytes, return actual number, char fifo only

    void _token(const charset& chars, str* result);

public:
    fifo_intf(bool is_char): _char(is_char)  { }

    enum { CHAR_ALL = mem(-2), CHAR_SOME = mem(-1) };

    virtual bool empty() = 0;

    // Variant FIFO operations
    void var_enq(const variant&);
    const variant& var_preview();
    void var_deq(variant&);
    void var_eat();

    // Character FIFO operations
    bool is_char_fifo() const           { return _char; }
    char preview();
    char get();
    str  deq(mem);  // CHAR_ALL, CHAR_SOME can be specified
    str  deq(const charset& c)          { str s; _token(c, &s); return s; }
    void eat(const charset& c)          { _token(c, NULL); }
    mem  enq(const char* p, mem count)  { return enq_chars(p, count); }
    mem  enq(const str& s)              { return enq_chars(s.data(), s.size()); }

    // TODO: iostream-like << operators for everything to completely replace
    // the std::iostream. Possibly only output methods are needed.
};


// The fifo class implements a linked list of "chunks" in memory, where
// each chunk is the size of 32 * sizeof(variant). Both enqueue and deqeue
// operations are O(1), and memory usage is better than that of a plain linked 
// list of elements, as "next" pointers are kept for bigger chunks of elements
// rather than for each element. Can be used both for variants and chars. This
// class "owns" variants, i.e. proper construction and desrtuction is done.
class fifo: public fifo_intf
{
public:
#ifdef DEBUG
    static mem CHUNK_SIZE; // settable from unit tests
#else
    enum { CHUNK_SIZE = 32 * sizeof(variant) };
#endif

protected:
    struct chunk: noncopyable
    {
        chunk* next;
        char data[0];
        
#ifdef DEBUG
        chunk(): next(NULL)         { pincrement(&object::alloc); }
        ~chunk()                    { pdecrement(&object::alloc); }
#else
        chunk(): next(NULL) { }
#endif
        void* operator new(size_t)  { return new char[sizeof(chunk) + CHUNK_SIZE]; }
    };

    chunk* head;    // in
    chunk* tail;    // out
    unsigned head_offs;
    unsigned tail_offs;

    void enq_chunk();
    void deq_chunk();

    // Overrides
    const char* get_tail();
    const char* get_tail(mem*);
    void deq_bytes(mem);
    variant* enq_var();
    mem enq_chars(const char*, mem);

    char* enq_space(mem);
    mem enq_avail();

public:
    fifo(bool is_char);
    ~fifo();

    void clear();

    virtual bool empty();
    virtual void dump(std::ostream&) const;
};


// This is an abstract buffered fifo class. Implementations should validate the
// buffer in the overridden empty() and flush() methods, for input and output
// fifos respectively. To simplify things, buf_fifo objects are not supposed to
// be reusable, i.e. once the end of file is reached, the implementation is not
// required to reset its state. Variant fifo implementations should guarantee
// at least sizeof(variant) bytes in calls to get_tail() and enq_var().
class buf_fifo: public fifo_intf
{
protected:
    char* buffer;
    mem   bufsize;
    mem   bufhead;
    mem   buftail;

    const char* get_tail();
    const char* get_tail(mem*);
    void deq_bytes(mem);
    variant* enq_var();
    mem enq_chars(const char*, mem);

    char* enq_space(mem);
    mem enq_avail();

public:
    buf_fifo(bool is_char);
    ~buf_fifo();

    virtual bool empty(); // throws efifowronly
    virtual void flush(); // throws efifordonly
};


class str_fifo: public buf_fifo
{
protected:
    str string;

public:
    str_fifo();
    str_fifo(const str&);
    ~str_fifo();
    bool empty();
    void flush();
};


// --- str_fifo ------------------------------------------------------------ //


str_fifo::~str_fifo() { }
str_fifo::str_fifo(): buf_fifo(true), string() {}


str_fifo::str_fifo(const str& s)
    : buf_fifo(true), string(s)
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


// --- buf_fifo ------------------------------------------------------------ //


buf_fifo::buf_fifo(bool is_char)
  : fifo_intf(is_char), buffer(NULL), bufsize(0), bufhead(0), buftail(0)  { }

buf_fifo::~buf_fifo()  { }
bool buf_fifo::empty() { throw efifowronly(); }
void buf_fifo::flush() { throw efifordonly(); }


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
    assert(buftail <= bufhead && bufhead <= bufsize);
    _req(false);
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


// --- fifo_intf ----------------------------------------------------------- //


#ifdef DEBUG
mem fifo::CHUNK_SIZE = sizeof(variant) * 16;
#endif


void fifo_intf::_empty_err() const          { throw efifoempty(); }
void fifo_intf::_fifo_type_err() const      { fatal(0x1002, "FIFO type mismatch"); }
const char* fifo_intf::get_tail()           { throw efifowronly(); }
const char* fifo_intf::get_tail(mem*)       { throw efifowronly(); }
void fifo_intf::deq_bytes(mem)              { throw efifowronly(); }
variant* fifo_intf::enq_var()               { throw efifordonly(); }
mem fifo_intf::enq_chars(const char*, mem)  { throw efifordonly(); }


fifo::fifo(bool _char)
    : fifo_intf(_char), head(NULL), tail(NULL), head_offs(0), tail_offs(0)  { }


void fifo_intf::_req_non_empty(bool _char)
{
    _req(_char);
    if (empty())
        _empty_err();
}


char fifo_intf::preview()
{
    _req(true);
    const char* p = get_tail();
    if (p == NULL)
        _empty_err();
    return *p;
}


char fifo_intf::get()
{
    char c = preview();
    deq_bytes(1);
    return c;
}


void fifo_intf::var_eat()
{
    _req_non_empty(false);
    ((variant*)get_tail())->~variant();
    deq_bytes(sizeof(variant));
}


const variant& fifo_intf::var_preview()
{
    _req_non_empty(false);
    return *(variant*)get_tail();
}


void fifo_intf::var_deq(variant& v)
{
    _req_non_empty(false);
    v.clear();
    memcpy((char*)&v, get_tail(), sizeof(variant));
    deq_bytes(sizeof(variant));
}


void fifo_intf::var_enq(const variant& v)
{
    _req(false);
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
    _req_non_empty(true);
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


void fifo::dump(std::ostream& s) const
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


// --- tests --------------------------------------------------------------- //


#define check assert


void test_bidir_char_fifo(fifo_intf& fc)
{
    check(fc.is_char_fifo());
    fc.enq("0123456789abcdefghijklmnopqrstuvwxyz");
    fc.enq("./");
    check(fc.preview() == '0');
    check(fc.get() == '0');
    check(fc.get() == '1');
    check(fc.deq(16) == "23456789abcdefgh");
    check(fc.deq(fifo::CHAR_ALL) == "ijklmnopqrstuvwxyz./");
    check(fc.empty());

    fc.enq("0123456789");
    fc.enq("abcdefghijklmnopqrstuvwxyz");
    check(fc.get() == '0');
    while (!fc.empty())
        fc.deq(fifo_intf::CHAR_SOME);

    fc.enq("0123456789abcdefghijklmnopqrstuvwxyz");
    check(fc.deq("0-9") == "0123456789");
    check(fc.deq("a-z") == "abcdefghijklmnopqrstuvwxyz");
    check(fc.empty());
}


void test_str_fifo()
{
    str_fifo fs;
    test_bidir_char_fifo(fs);
}


int main()
{
    {
/*
        variant v;
        Parser parser(new InFile("x"));
        List<Symbol> list;
*/

#ifdef DEBUG
        fifo::CHUNK_SIZE = 2 * sizeof(variant);
#endif

        fifo f(false);
        variant v = new_tuple();
        v.push_back(0);
        f.var_enq(v);
        f.var_enq("abc");
        f.var_enq("def");
        variant w = new_range(1, 2);
        f.var_enq(w);
        f.dump(std::cout); std::cout << std::endl;
        variant x;
        f.var_deq(x);
        check(x.is_tuple());
        f.var_deq(w);
        check(w.is_str());
        f.var_eat();
        check(f.var_preview().is_range());

        fifo fc(true);
        test_bidir_char_fifo(fc);
        
        test_str_fifo();
    }
#ifdef DEBUG
    assert(object::alloc == 0);
#endif
}

