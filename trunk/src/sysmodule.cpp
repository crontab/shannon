
#include "sysmodule.h"
#include "vm.h"
#include "compiler.h"


void compileLen(Compiler* c, Builtin*)
    { c->codegen->length(); }

void compileLo(Compiler* c, Builtin*)
    { c->codegen->lo(); }

void compileHi(Compiler* c, Builtin*)
    { c->codegen->hi(); }

void compileToStr(Compiler* c, Builtin*)
    { c->codegen->toStr(); }

void compileEnq(Compiler* c, Builtin*)
    { c->codegen->fifoEnq(); }

void compileDeq(Compiler* c, Builtin*)
    { c->codegen->fifoDeq(); }

void compileToken(Compiler* c, Builtin*)
    { c->codegen->fifoToken(); }


void compileSkip(Compiler* c, Builtin* b)
{
    // TODO: maybe more possibilities, e.g. skip(n), skip({...}) for any fifo
    Type* fifoType = c->codegen->getTopType(2);
    if (!fifoType->isByteFifo())
        c->error("'skip' is only applicable to small ordinal fifos");
    Type* setType = c->codegen->getTopType();
    if (!setType->isByteSet())
        c->error("Small ordinal set expected");
    if (!PContainer(setType)->index->canAssignTo(PFifo(fifoType)->elem))
        c->error("Incompatible set element type");
    c->codegen->staticCall(b->staticFunc);
}


void shn_skipset(variant*, stateobj*, variant args[])
{
    args[-2]._fifo()->skip(args[-1]._ordset().get_charset());
}


void shn_eol(variant* result, stateobj*, variant args[])
{
    new(result) variant((int)args[-1]._fifo()->eol());
}


void shn_line(variant* result, stateobj*, variant args[])
{
    new(result) variant(args[-1]._fifo()->line());
}


void shn_skipln(variant*, stateobj*, variant args[])
{
    fifo* f = args[-1]._fifo();
    int c = f->preview();
    if (c != -1 && c != '\r' && c != '\n')
        f->skip(non_eol_chars);
    f->skip_eol();
}


void shn_look(variant* result, stateobj*, variant args[])
{
    new(result) variant((uchar)args[-1]._fifo()->look());
}

void shn_strfifo(variant* result, stateobj*, variant args[])
{
    new(result) variant(new strfifo(queenBee->defCharFifo, args[-1]._str()));
}


