
#include "sysmodule.h"
#include "vm.h"
#include "compiler.h"


void compileLen(Compiler* c, Builtin*)
{
    c->expectLParen();
    c->expression(NULL);
    c->expectRParen();
    c->codegen->length();
}

