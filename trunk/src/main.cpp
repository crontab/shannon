
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "fifo.h"
#include "typesys.h"
#include "source.h"


// --- tests --------------------------------------------------------------- //


int main()
{
    {
        variant v;
        Parser parser("x", new in_text("x"));
        List<Symbol> list;
        fifo f(true);
        
        fout << "Hello, world" << endl;
    }
#ifdef DEBUG
    assert(object::alloc == 0);
#endif
}

