
#include <assert.h>

#include <iostream>

#include "fifo.h"
#include "typesys.h"
#include "source.h"


// --- tests --------------------------------------------------------------- //


int main()
{
    {
        variant v;
        Parser parser(new InFile("x"));
        List<Symbol> list;
        fifo f(true);
    }
#ifdef DEBUG
    assert(object::alloc == 0);
#endif
}

