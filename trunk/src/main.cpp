

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


#include "runtime.h"
#include "source.h"
#include "symbols.h"
#include "typesys.h"


// --- tests --------------------------------------------------------------- //

#define check(x) assert(x)


int main()
{
    initTypeSys();
    try
    {
        Parser parser("x", new in_text("x"));

    }
    catch (std::exception& e)
    {
        ferr << "Exception: " << e.what() << endl;
    }
    doneTypeSys();
#ifdef DEBUG
    assert(object::alloc == 0);
#endif
}

