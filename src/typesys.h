#ifndef __TYPESYS_H
#define __TYPESYS_H


#include "runtime.h"


class Type: public object
{
};


class State: public Type
{
public:
    memint thisSize() { return 0; } // TODO
};


#endif // __TYPESYS_H
