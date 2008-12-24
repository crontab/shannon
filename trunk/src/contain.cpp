
#include <string.h>

#include "contain.h"


int FifoChunk::chunkCount = 0;


FifoChunk::FifoChunk()
{
#ifdef DEBUG
    chunkCount++;
#endif
    data = (char*)memalloc(FIFO_CHUNK_SIZE);
}

FifoChunk::FifoChunk(const FifoChunk& f)
{
#ifdef DEBUG
    chunkCount++;
#endif
    data = (char*)memalloc(FIFO_CHUNK_SIZE);
    memcpy(data, f.data, FIFO_CHUNK_SIZE);
}

FifoChunk::~FifoChunk()
{
    memfree(data);
#ifdef DEBUG
    chunkCount--;
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
        if (data != NULL)
            memcpy(_chunkat(chunks() - 1).data + right, data, len);
        right += len;
        datasize -= len;
        if (datasize == 0)
            return;
        if (data != NULL)
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
        if (data != NULL)
            memcpy(data, _chunkat(0).data + left, len);
        left += len;
        if (left == curright)
        {
            Array<FifoChunk>::del(0);
            left = 0;
        }
        datasize -= len;
        if (data != NULL)
            data += len;
        result += len;
    }
    return result;
}



int stackimpl::stackAlloc = 0;

void stackimpl::stackunderflow() const
{
    fatal(CRIT_FIRST + 30, "Stack underflow");
}


void stackimpl::invstackop() const
{
    fatal(CRIT_FIRST + 31, "Invalid stack operation");
}


stackimpl::stackimpl()
    : end(NULL), begin(NULL), capend(NULL)  { }


void stackimpl::clear()
{
    if (begin != NULL)
    {
        stackAlloc -= capend - begin;
        memfree(begin);
        end = begin = capend = NULL;
    }
}


void stackimpl::grow()
{
    int newsize = end - begin;
    int oldcap = capend - begin;
    int newcap = memquantize(newsize);
    if (newcap > oldcap)
    {
        stackAlloc += newcap - oldcap;
        begin = (char*)memrealloc(begin, newcap);
        capend = begin + newcap;
        end = begin + newsize;
#ifdef DEBUG
        if (end >= capend)
            invstackop();
#endif
    }
}

