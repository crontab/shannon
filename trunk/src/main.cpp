
#include "charset.h"
#include "str.h"
#include "except.h"
#include "array.h"


// ------------------------------------------------------------------------ //
// --- TOKEN EXTRACTOR ---------------------------------------------------- //
// ------------------------------------------------------------------------ //




class _AtExit
{
public:
    ~_AtExit()
    {
        if (Base::objCount != 0)
            fprintf(stderr, "Internal: objCount = %d\n", Base::objCount);
        if (stralloc != 0)
            fprintf(stderr, "Internal: stralloc = %d\n", stralloc);
        if (fifoChunkAlloc != 0)
            fprintf(stderr, "Internal: fifoChunkAlloc = %d\n", fifoChunkAlloc);
    }
} _atexit;



int main()
{
    return 0;
}

