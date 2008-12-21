
#include <stdio.h>

#include "str.h"
#include "except.h"
#include "langobj.h"


enum OpCode
{
    opNone = 0,
    
    opLoad0,        // []
    opLoadInt,      // [int]
    opLoadLarge,    // [int,int]
    opLoadChar,     // [int]
    opLoadFalse,    // []
    opLoadTrue,     // []
    opLoadNull,     // []
    opLoadStr,      // [string]
};

union OpQuant
{
    ptr ptr_;
    int int_;
};



// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //



// ------------------------------------------------------------------------- //


class _AtExit
{
public:
    ~_AtExit()
    {
        if (Base::objCount != 0)
            fprintf(stderr, "Internal: objCount = %d\n", Base::objCount);
        if (stralloc != 0)
            fprintf(stderr, "Internal: stralloc = %d\n", stralloc);
        if (FifoChunk::chunkCount != 0)
            fprintf(stderr, "Internal: chunkCount = %d\n", FifoChunk::chunkCount);
    }
} _atexit;




int main()
{
    try
    {
        ShQueenBee system;
    }
    catch (Exception& e)
    {
        fprintf(stderr, "\n*** Exception: %s\n", e.what().c_str());
    }

    printf("sizeof(Base): %lu  sizeof(ShBase): %lu\n", sizeof(Base), sizeof(ShBase));
    return 0;
}

