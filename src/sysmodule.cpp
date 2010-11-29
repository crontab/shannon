
#include "sysmodule.h"
#include "vm.h"
#include "compiler.h"


void compileLen(Compiler* c, Builtin*)
    { c->codegen->length(); }

void compileLo(Compiler* c, Builtin*)
    { c->codegen->lo(); }

void compileHi(Compiler* c, Builtin*)
    { c->codegen->hi(); }

