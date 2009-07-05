

#include <stdlib.h>
#include <stdio.h>

#include "common.h"


void fatal(int code, const char* msg) 
{
    fprintf(stderr, "\nInternal [%04x]: %s\n", code, msg);
    exit(code);
}


const int quant = 64;
const int quant2 = 4096;

int memquantize(int a)
{
    if (a <= 32)
        return 32;
    else if (a <= 1024)
        return (a + quant - 1) & ~(quant - 1);
    else
        return (a + quant2 - 1) & ~(quant2 - 1);
}


