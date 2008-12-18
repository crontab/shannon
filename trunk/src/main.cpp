
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "charset.h"
#include "str.h"
#include "array.h"
#include "source.h"
#include "baseobj.h"


// ------------------------------------------------------------------------ //
// --- TEXT FILE READER --------------------------------------------------- //
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

