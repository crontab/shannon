#ifndef __BUILTINS_H
#define __BUILTINS_H

#include "runtime.h"
#include "typesys.h"


// Defined in typesys.h:
//   typedef void (*CompileFunc)(Compiler*, Builtin*); -- for builtins
//   typedef void (*ExternFuncProto)(stateobj* outerobj, variant* args);


// --- BUILTINS ------------------------------------------------------------ //

class Compiler;

 
void compileLen(Compiler*, Builtin*);


#endif // __BUILTINS_H
