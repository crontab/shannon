
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
        
        chunk(): next(NULL)  { }
    };
    
    chunk* head;   // in
    chunk* tail;    // out
    unsigned head_offs;
    unsigned tail_offs;
    bool is_char;

    void internal() const;
    void enq_chunk();
    void deq_chunk();

    mem get_avail_tail() const; // get available data within the current tail chunk
    char* get_tail()            // get a pointer to data in the tail chunk
            { return tail->data + tail_offs; }
    void deq(mem);              // dequeue up to n bytes within the current tail chunk
    mem get_avail_head() const; // get available data within the current head chunk
    void enq(char*, mem);       // dequeue up to n bytes within the current head chunk

    // virtual object* clone() const;
public:
    pod_fifo(bool is_char);
    ~pod_fifo();

    virtual bool empty() const;

    // virtual void dump(std::ostream&) const;
};



// --- IMPLEMENTATION ------------------------------------------------------ //


pod_fifo::pod_fifo(bool is_char)
    : head(NULL), tail(NULL), head_offs(0), tail_offs(0), is_char(is_char)  { }


void pod_fifo::internal() const
    { fatal(0x1002, "FIFO type mismatch"); }


pod_fifo::~pod_fifo()
{
    while (tail != NULL)
        deq_chunk();
}


bool pod_fifo::empty() const { return tail == NULL; }


void pod_fifo::deq_chunk()
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


void pod_fifo::enq_chunk()
{
    chunk* c = new chunk();
    if (head == NULL)
    {
        assert(tail == NULL);
        assert(head_offs == 0);
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


mem pod_fifo::get_avail_tail() const
{
    if (tail == NULL)
        return 0;
    mem result = 0;
    if (tail == head)
        result = head_offs - tail_offs;
    else
        result = CHUNK_SIZE - tail_offs;
    assert(result > 0 && result < CHUNK_SIZE);
    return result;
}


void pod_fifo::deq(mem count)
{
    tail_offs += count;
    assert(tail != NULL && tail_offs <= ((tail == head) ? head_offs : CHUNK_SIZE));
    if (tail_offs == ((tail == head) ? head_offs : CHUNK_SIZE))
        deq_chunk();
}


int main()
{
    variant v;
    Parser parser(new InFile("x"));
    List<Symbol> list;
}

