#ifndef __BUILTINS_H
#define __BUILTINS_H

#include "runtime.h"
#include "typesys.h"


// Defined in typesys.h:
//   typedef void (*CompileFunc)(Compiler*, Builtin*); -- for builtins
//   typedef void (*ExternFuncProto)(variant* result, stateobj* outerobj, variant args[]);


// --- BUILTINS ------------------------------------------------------------ //

class Compiler;

void compileLen(Compiler*);
void compileLo(Compiler*);
void compileHi(Compiler*);
void compileToStr(Compiler*);
void compileEnq(Compiler*);
void compileDeq(Compiler*);
void compileToken(Compiler*);
void compileSkip(Compiler*);

void shn_strfifo(variant*, stateobj*, variant[]);
void shn_skipset(variant*, stateobj*, variant[]);


#endif // __BUILTINS_H
