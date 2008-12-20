
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


class ShType: public Base
{
public:
    ShType(): Base() { }
    virtual ~ShType();
    virtual int physicalSize() const = 0;
    virtual int alignedSize() const;
    virtual bool isOrdinal() const = 0;
    virtual bool isReference() const = 0;
};


class ShReference: public ShType
{
public:
    ShType* base;
    bool constRef;
    bool constBase;

    ShReference(ShType* iBase, bool iConstRef, bool iConstBase);

    virtual int physicalSize() const  { return sizeof(ptr); };
    virtual bool isOrdinal() const    { return false; }
    virtual bool isReference() const  { return true; }
};


struct Range
{
    large min;
    large max;

    Range()  { }
    Range(large iMin, large iMax): min(iMin), max(iMax)  { }
    bool has(large v) const   { return v >= min && v <= max; }
    bool isSigned() const     { return min < 0; }
    int  physicalSize() const;
};


class ShInteger: public ShType
{
public:
    Range range;
    int size;

    ShInteger(large min, large max);
    virtual int physicalSize() const  { return size; };
    virtual bool isOrdinal() const    { return true; }
    virtual bool isReference() const  { return false; }
};


// ------------------------------------------------------------------------- //


ShType::~ShType()  { }

ShReference::ShReference(ShType* iBase, bool iConstRef, bool iConstBase)
    : ShType(), base(iBase), constRef(iConstRef), constBase(iConstBase)  { }

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
    printf("%lld\n", uint32max);
    return 0;
}

