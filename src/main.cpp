
#include <stdio.h>

#include "charset.h"
#include "str.h"
#include "contain.h"
#include "except.h"
#include "baseobj.h"


// ------------------------------------------------------------------------ //
// --- TOKEN EXTRACTOR ---------------------------------------------------- //
// ------------------------------------------------------------------------ //


enum Token
{
    tokUndefined = -1,
    tokBlockBegin, tokBlockEnd, tokEnd // these depend on C-style vs. Python-style mode
};


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
    return 0;
}

