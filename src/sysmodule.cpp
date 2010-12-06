
#include "sysmodule.h"
#include "vm.h"
#include "compiler.h"


void compileLen(Compiler* c)
    { c->codegen->length(); }

void compileLo(Compiler* c)
    { c->codegen->lo(); }

void compileHi(Compiler* c)
    { c->codegen->hi(); }

void compileToStr(Compiler* c)
    { c->codegen->toStr(); }

void compileEnq(Compiler* c)
    { c->codegen->fifoEnq(); }

void compileDeq(Compiler* c)
    { c->codegen->fifoDeq(); }

void compileToken(Compiler* c)
    { c->codegen->fifoToken(); }

void compileSkip(Compiler* c)
{
    Type* fifoType = c->codegen->getTopType(2);
    if (!fifoType->isByteFifo())
        c->error("'skip' is only applicable to small ordinal fifos");
    Type* setType = c->codegen->getTopType();
    if (!setType->isByteSet())
        c->error("Small ordinal set expected");
    if (!PContainer(setType)->index->canAssignTo(PFifo(fifoType)->elem))
        c->error("Incompatible set element type");
    c->codegen->staticCall(queenBee->skipFunc);
}


void shn_strfifo(variant* result, stateobj*, variant args[])
{
    new(result) variant(new strfifo(queenBee->defCharFifo, args[-1]._str()));
}


void shn_skipset(variant*, stateobj*, variant args[])
{
    args[-2]._fifo()->skip(args[-1]._ordset().get_charset());
}


