

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "runtime.h"


// --- tests --------------------------------------------------------------- //


#define check(x) assert(x)


#define XSTR(s) _STR(s)
#define _STR(s) #s

#ifdef SH64
#  define INTEGER_MAX_STR "9223372036854775807"
#  define INTEGER_MAX_STR_PLUS "9223372036854775808"
#  define INTEGER_MIN_STR "-9223372036854775808"
#else
#  define INTEGER_MAX_STR "2147483647"
#  define INTEGER_MAX_STR_PLUS "2147483648"
#  define INTEGER_MIN_STR "-2147483648"
#endif


int main()
{
    printf("%lu %lu %lu\n", sizeof(rcblock), sizeof(rcdynamic), sizeof(container));

    if (rcblock::allocated != 0)
    {
        fprintf(stderr, "rcblock::allocated: %d\n", rcblock::allocated);
        _fatal(0xff01);
    }

    return 0;
}
