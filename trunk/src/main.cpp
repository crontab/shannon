
#include <assert.h>

#include <iostream>

#include "charset.h"
#include "typesys.h"
#include "source.h"


class fifo_intf: public object
{
public:
    enum {CHAR_ALL = -1, CHAR_SOME = -2};

    virtual bool empty() const = 0;
    virtual void enq(const variant&) = 0;
    virtual void deq(variant&) = 0;

/*
    virtual bool is_char_fifo() const = 0;
    virtual str  cdeq(mem) = 0;
    virtual void cenq(const str&) = 0;
    virtual char cpreview() const = 0;
    virtual char get() = 0;
    virtual str  token(const cset&) = 0;
    virtual void skip(const cset&) = 0;
*/
};


class pod_fifo: public fifo_intf
{
    enum { CHUNK_SIZE = 256 };

    struct chunk: noncopyable
    {
        char data[CHUNK_SIZE];
        chunk* next;
        chunk* prev;
        
        chunk(chunk* next, chunk* prev)
            : next(next), prev(prev)  { }
    };
    
    chunk* first;   // in
    chunk* last;    // out
    int head_offs;
    int tail_offs;
    bool is_char;

    void internal() const;
    void enq_chunk();
    void deq_chunk();

    mem get_avail() const;

    // virtual object* clone() const;
public:
    pod_fifo(bool is_char);
    ~pod_fifo();

    virtual bool empty() const { return tail == NULL; }

    // virtual void dump(std::ostream&) const;
};



// --- IMPLEMENTATION ------------------------------------------------------ //


pod_fifo::pod_fifo(bool is_char)
    : head(NULL), tail(NULL), head_offs(0), tail_offs(0), is_char(is_char)  { }


void pod_fifo::internal() const
    { fatal(0x1002, "FIFO type mismatch"); }


bool pod_fifo::empty() const { return tail == NULL; }


int main()
{
    variant v;
    Parser parser(new InFile("x"));
    List<Symbol> list;
}
