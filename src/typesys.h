#ifndef __TYPESYS_H
#define __TYPESYS_H

#include "common.h"
#include "variant.h"
#include "symbols.h"


// --- BASIC LANGUAGE OBJECTS ---------------------------------------------- //


class Base: public Symbol
{
public:
    enum BaseId { TYPE, VARIABLE, DEFINITION, SCOPE };
    const BaseId baseId;

    Base(BaseId);
    Base(const str&, BaseId);

    bool isType() const       { return baseId == TYPE; }
    bool isVariable() const   { return baseId == VARIABLE; }
    bool isDefinition() const { return baseId == DEFINITION; }
    bool isScope() const      { return baseId == SCOPE; }
};


class Type: public Base
{
public:
    enum TypeId { VOID, BOOL, CHAR, INT, REAL, STR, RANGE,
        ARRAY, DICT, TINYSET, CHARSET, SET, FIFO, STATE };
};


#endif // __TYPESYS_H