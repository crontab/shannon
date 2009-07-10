
#include <assert.h>
#include <string.h>

#include <iostream>

#include "charset.h"
#include "typesys.h"
#include "source.h"


DEF_EXCEPTION(efifoempty, "Invalid FIFO operation");
DEF_EXCEPTION(efifordonly, "FIFO is read-only");
DEF_EXCEPTION(efifowronly, "FIFO is write-only");


class fifo_intf: public object
{
protected:
    bool _char;

    void _empty_err() const;
    void _fifo_type_err() const;
    void _rdonly_err() const;
    void _wronly_err() const;
    void _req(bool req_char) const      { if (req_char != _char) _fifo_type_err(); }
    void _req_non_empty(bool _char) const;

    // Minimal set of methods required for the character FIFO operations.
    // Implementations of variant FIFOs should guarantee variants will never
    // be fragmented, so that a buffer returned by get_tail() always contains
    // at least one variant (8, 12 or 16 bytes depending on the host platform).
    // enq_space() should guarantee at most sizeof(variant) consecutive bytes.
    virtual const char* get_tail() const;       // Get a pointer to tail data, at least 1 byte is guaranteed if not empty
    virtual const char* get_tail(mem*) const;   // ... also return the length
    virtual void deq_bytes(mem);                // Discard n consecutive bytes returned by get_tail()
    virtual char* enq_space(mem);               // Reserve n consecutive bytes, not more than sizeof(variant)
    virtual mem enq_bytes(const char*, mem);    // Push arbitrary number of bytes, return actual number, char fifo only

    void _token(const charset& chars, str* result);

public:
    fifo_intf(bool is_char): _char(is_char)  { }

    enum { CHAR_ALL = mem(-2), CHAR_SOME = mem(-1) };

    virtual bool empty() const = 0;

    // Variant FIFO operations
    void var_enq(const variant&);
    const variant& var_preview();
    void var_deq(variant&);
    void var_eat();

    // Character FIFO operations
    bool is_char_fifo() const           { return _char; }
    char preview() const;
    char get();
    str  deq(mem);  // CHAR_ALL, CHAR_SOME can be specified
    str  deq(const charset& c)          { str s; _token(c, &s); return s; }
    void eat(const charset& c)          { _token(c, NULL); }
    mem  enq(const char* p, mem count)  { return enq_bytes(p, count); }
    mem  enq(const str& s)              { return enq_bytes(s.data(), s.size()); }
};


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
    const char* get_tail() const;
    const char* get_tail(mem*) const;
    void deq_bytes(mem);
    char* enq_space(mem);
    mem enq_bytes(const char*, mem);

    mem enq_avail() const;

public:
    fifo(bool is_char);
    ~fifo();

    void clear();

    virtual bool empty() const;
    virtual void dump(std::ostream&) const;
};


class input_fifo: public fifo_intf
{
protected:
    int   fd;
    char* buffer;
    int   bufsize;
    int   bufpos;
    bool  eof;

public:
};


// --- IMPLEMENTATION ------------------------------------------------------ //


#ifdef DEBUG
mem fifo::CHUNK_SIZE = sizeof(variant) * 16;
#endif


void fifo_intf::_empty_err() const          { throw efifoempty(); }
void fifo_intf::_fifo_type_err() const      { fatal(0x1002, "FIFO type mismatch"); }
void fifo_intf::_rdonly_err() const         { throw efifordonly(); }
void fifo_intf::_wronly_err() const         { throw efifowronly(); }

const char* fifo_intf::get_tail() const     { _wronly_err(); return NULL; }
const char* fifo_intf::get_tail(mem*) const { _wronly_err(); return NULL; }
void fifo_intf::deq_bytes(mem)              { _wronly_err(); }
char* fifo_intf::enq_space(mem)             { _rdonly_err(); return NULL; }
mem fifo_intf::enq_bytes(const char*, mem)  { _rdonly_err(); return 0; }


fifo::fifo(bool _char)
    : fifo_intf(_char), head(NULL), tail(NULL), head_offs(0), tail_offs(0)  { }


void fifo_intf::_req_non_empty(bool _char) const
{
    _req(_char);
    if (empty())
        _empty_err();
}


char fifo_intf::preview() const
{
    _req_non_empty(true);
    return *get_tail();
}


char fifo_intf::get()
{
    _req_non_empty(true);
    char result = *get_tail();
    deq_bytes(1);
    return result;
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
    char* p = enq_space(sizeof(variant));
    ::new(p) variant(v);
}


str fifo_intf::deq(mem count)
{
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


fifo::~fifo() { clear(); }


inline const char* fifo::get_tail() const { return tail->data + tail_offs; }


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


bool fifo::empty() const        { return tail == NULL; }


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


const char* fifo::get_tail(mem* count) const
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


char* fifo::enq_space(mem count)
{
    if (head == NULL || head_offs == CHUNK_SIZE)
        enq_chunk();
    assert(count <= CHUNK_SIZE - head_offs);
    char* result = head->data + head_offs;
    head_offs += count;
    return result;
}


mem fifo::enq_bytes(const char* p, mem count)
{
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


mem fifo::enq_avail() const
{
    if (head == NULL || head_offs == CHUNK_SIZE)
        return CHUNK_SIZE;
    return CHUNK_SIZE - head_offs;
}


void fifo::dump(std::ostream& s) const
{
    if (empty())
        return;
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


int main()
{
    {
/*
        variant v;
        Parser parser(new InFile("x"));
        List<Symbol> list;
*/

#define check assert
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
        fc.enq("0123456789abcdefghijklmnopqrstuvwxyz");
        fc.enq("./");
        check(fc.preview() == '0');
        check(fc.get() == '0');
        check(fc.get() == '1');
        check(fc.deq(16) == "23456789abcdefgh");
        check(fc.deq(fifo::CHAR_ALL) == "ijklmnopqrstuvwxyz./");

        fc.enq("0123456789");
        fc.enq("abcdefghijklmnopqrstuvwxyz");
        check(fc.get() == '0');
        check(fc.deq(fifo::CHAR_SOME) == "123456789abcdef");
        check(fc.deq(fifo::CHAR_SOME) == "ghijklmnopqrstuv");
        check(fc.deq(fifo::CHAR_SOME) == "wxyz");

        fc.enq("0123456789abcdefghijklmnopqrstuvwxyz");
        check(fc.deq("0-9") == "0123456789");
        check(fc.deq("a-z") == "abcdefghijklmnopqrstuvwxyz");
        check(fc.empty());
    }
#ifdef DEBUG
    assert(object::alloc == 0);
#endif
}

