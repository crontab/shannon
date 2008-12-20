
#include <stdio.h>

#include "charset.h"
#include "str.h"
#include "contain.h"
#include "except.h"
#include "source.h"
#include "baseobj.h"
#include "bsearch.h"


const large int8min   = -128LL;
const large int8max   = 127LL;
const large uint8max  = 255LL;
const large int16min  = -32768LL;
const large int16max  = 32767LL;
const large uint16max = 65535LL;
const large int32min  = -2147483648LL;
const large int32max  = 2147483647LL;
const large uint32max = 4294967295LL;
const large int64min  = LARGE_MIN;
const large int64max  = LARGE_MAX;
const int   memAlign  = sizeof(int);


// --- BASIC LANGUAGE OBJECTS ---------------------------------------------- //


class ShType;
class ShScope;


class ShBase: public Base
{
public:
    ShScope* owner;
    
    ShBase(): Base(), owner(NULL)  { }
    virtual const ShType* type() const = 0;
};


class ShType: public ShBase
{
public:
    ShType(): ShBase() { }
    virtual const ShType* type() const { return this; };
    virtual int physicalSize() const = 0;
    virtual int alignedSize() const;
    virtual bool isComplete() const = 0;
    virtual bool isOrdinal() const = 0;
    virtual bool hasCircularRefs() const = 0;
};


class ShScope: public ShType
{
protected:
    BaseTable<ShBase> symbols;
    BaseList<ShType> anonTypes;
public:
    void add(ShBase* obj);
    ShBase* find(const string& name) const { return symbols.find(name); }
    ShBase* deepSearch(const string&) const throw(ENotFound);
};



// --- LANGUAGE TYPES ----------------------------------------------------- //


struct Range
{
    const large min;
    const large max;

    Range(large iMin, large iMax): min(iMin), max(iMax)  { }
    bool has(large v) const   { return v >= min && v <= max; }
    bool isSigned() const     { return min < 0; }
    int  physicalSize() const;
};


class ShInteger: public ShType
{
public:
    const Range range;
    const int size;

    ShInteger(large min, large max);
    virtual int  physicalSize() const     { return size; };
    virtual bool isComplete() const       { return true; }
    virtual bool isOrdinal() const        { return true; }
    virtual bool hasCircularRefs() const  { return false; }
};


// ------------------------------------------------------------------------- //
// ------------------------------------------------------------------------- //


// --- BASIC LANGUAGE OBJECTS ---------------------------------------------- //


void ShScope::add(ShBase* obj)
{
    if (obj->owner != NULL)
        throw EInternal(3, "ShScope::add(): obj->owner != NULL");
    symbols.addUnique(obj);
    obj->owner = this;
}


ShBase* ShScope::deepSearch(const string& name) const throw(ENotFound)
{
    ShBase* obj = find(name);
    if (obj != NULL)
        return obj;
    if (owner != NULL)
        return owner->deepSearch(name);
    throw ENotFound(name);
}


// --- LANGUAGE TYPES ----------------------------------------------------- //


int ShType::alignedSize() const
{
    return ((physicalSize() / memAlign) + 1) * memAlign;
}



int Range::physicalSize() const
{
    if (min >= 0)
    {
        if (max <= uint8max)
            return 1;
        if (max <= uint16max)
            return 2;
        if (max <= uint32max)
            return 4;
        return 8;
    }
    if (min == int64min)
        return 8;
    large t = ~min;
    if (max > t)
        t = max;
    if (t <= int8max)
        return 1;
    if (t <= int16max)
        return 2;
    if (t <= int32max)
        return 4;
    return 8;
}


ShInteger::ShInteger(large min, large max)
    : range(min, max), size(range.physicalSize())
{
}


class _AtExit
{
public:
    ~_AtExit()
    {
        if (Base::objCount != 0)
            fprintf(stderr, "Internal: objCount = %d\n", Base::objCount);
        if (stralloc != 0)
            fprintf(stderr, "Internal: stralloc = %d\n", stralloc);
        if (FifoChunk::chunkCount != 0)
            fprintf(stderr, "Internal: chunkCount = %d\n", FifoChunk::chunkCount);
    }
} _atexit;



// ------------------------------------------------------------------------- //



int main()
{
    ShInteger i(0, 255);
    printf("sizeof(BaseTable): %lu\n", sizeof(BaseTable<ShBase>));
    return 0;
}

