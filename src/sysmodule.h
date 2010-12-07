#ifndef __BUILTINS_H
#define __BUILTINS_H

#include "runtime.h"
#include "typesys.h"


// Defined in typesys.h:
//   typedef void (*CompileFunc)(Compiler*, Builtin*); -- for builtins
//   typedef void (*ExternFuncProto)(variant* result, stateobj* outerobj, variant args[]);


// --- BUILTINS ------------------------------------------------------------ //

class Compiler;

void compileLen(Compiler*, Builtin*);
void compileLo(Compiler*, Builtin*);
void compileHi(Compiler*, Builtin*);
void compileToStr(Compiler*, Builtin*);
void compileEnq(Compiler*, Builtin*);
void compileDeq(Compiler*, Builtin*);
void compileToken(Compiler*, Builtin*);
void compileSkip(Compiler*, Builtin*);

void shn_skipset(variant*, stateobj*, variant[]);
void shn_eol(variant*, stateobj*, variant[]);
void shn_line(variant*, stateobj*, variant[]);
void shn_skipln(variant*, stateobj*, variant[]);
void shn_look(variant*, stateobj*, variant[]);

void shn_strfifo(variant*, stateobj*, variant[]);


#endif // __BUILTINS_H
