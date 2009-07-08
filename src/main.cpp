
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
    virtual str  cdeq(int) = 0;
    virtual void cenq(const str&, int) = 0;
    virtual char cpreview() const = 0;
    virtual char get() = 0;
    virtual str  token(const cset&) = 0;
    virtual void skip(const cset&) = 0;
*/
};


class pod_fifo: public fifo_intf
{
    enum { CHUNK_SIZE = 256 };

    struct chunk
    {
        char data[CHUNK_SIZE];
        chunk* next;
        chunk* prev;
        
        chunk(chunk* next, chunk* prev)
            : next(next), prev(prev)  { }
    };
    
    chunk* head;
    chunk* tail;
    int head_offs;
    int tail_offs;
    bool is_char;

    void internal() const;
    void enq_chunk();
    void deq_chunk();

    // virtual object* clone() const;
public:
    pod_fifo(bool is_char);
    ~pod_fifo();

    // virtual void dump(std::ostream&) const;
};



// --- IMPLEMENTATION ------------------------------------------------------ //


pod_fifo::pod_fifo(bool is_char)
    : head(NULL), tail(NULL), head_offs(0), tail_offs(0), is_char(is_char)  { }


pod_fifo::~pod_fifo()
{
    while (tail != NULL)
    {
        tail_offs = 0;
        deq_chunk();
    }
}


void pod_fifo::internal() const
    { fatal(0x1002, "FIFO type mismatch"); }


void pod_fifo::enq_chunk()
{
    assert(head_offs == 0);
    head = new chunk(head, NULL);
    head_offs = CHUNK_SIZE;
}


void pod_fifo::deq_chunk()
{
    assert(tail_offs == 0 && tail != NULL && tail->next == NULL);
    chunk* c = tail;
    tail = c->prev;
    delete c;
    if (tail != NULL)
        tail_offs = CHUNK_SIZE;
    else
        head = NULL;
}


int main()
{
    variant v;
    Parser parser(new InFile("x"));
    List<Symbol> list;
}
