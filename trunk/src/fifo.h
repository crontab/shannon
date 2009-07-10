#ifndef __FIFO_H
#define __FIFO_H


#include "common.h"
#include "charset.h"
#include "variant.h"


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

    virtual bool empty();   // throws efifowronly

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

    virtual void dump(fifo_intf&) const; // just displays <fifo>

    fifo_intf& operator<< (const char* s);
    fifo_intf& operator<< (const str& s)    { enq(s); return *this; }
    fifo_intf& operator<< (integer);
    fifo_intf& operator<< (uinteger);
    fifo_intf& operator<< (mem);
    fifo_intf& operator<< (char c)          { enq(&c, 1); return *this; }
    fifo_intf& operator<< (uchar c)         { enq((char*)&c, 1); return *this; }
};

const char endl = '\n';


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
    // virtual void dump(fifo_intf&) const;
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

    str all() const;
};


// Non-buffered file output, suitable for standard out/err, should be static
// in the program.
class std_file: public fifo_intf
{
protected:
    int _fd;
    virtual mem enq_chars(const char*, mem);
public:
    std_file(int fd);
    ~std_file();
};


extern std_file fout;
extern std_file ferr;



#endif // __FIFO_H
