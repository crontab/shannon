
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "runtime.h"


// --- rcblock ------------------------------------------------------------- //


int rcblock::allocated = 0;


void outofmem()
{
    fatal(0x1001, "Out of memory");
}


void* rcblock::operator new(size_t self)
{
    void* p = ::malloc(self);
    if (p == NULL)
        outofmem();
#ifdef DEBUG
    pincrement(&rcblock::allocated);
#endif
    return p;
}


void* rcblock::operator new(size_t self, memint extra)
{
    assert(self + extra > 0);
    void* p = ::malloc(self + extra);
    if (p == NULL)
        outofmem();
#ifdef DEBUG
    pincrement(&rcblock::allocated);
#endif
    return p;
}


rcblock* rcblock::realloc(rcblock* p, memint size)
{
    assert(p->refcount == 1);
    assert(size > 0);
    p = (rcblock*)::realloc(p, size);
    if (p == NULL)
        outofmem();
    return p;
}


void rcblock::operator delete(void* p)
{
#ifdef DEBUG
    pdecrement(&rcblock::allocated);
#endif
    ::free(p);
}


// --- rcdynamic ----------------------------------------------------------- //


rcdynamic* rcdynamic::realloc(rcdynamic* p, memint _capacity, memint _used)
{
    assert(p->unique());
    p = (rcdynamic*)rcblock::realloc(p, sizeof(rcdynamic) + _capacity);
    p->capacity = _capacity;
    p->used = _used;
    return p;
}


// --- container ----------------------------------------------------------- //


container::_nulldyn container::_null;


char* container::_init(memint len)
{
    if (len > 0)
    {
        dyn = rcref2(_precise_alloc(len));
        return dyn->data();
    }
    else
    {
        _init();
        return NULL;
    }
}


void container::_init(const char* buf, memint len)
{
    ::memcpy(_init(len), buf, len);
}


void container::_dofin()
{
    if (dyn->deref())
        delete dyn;
#ifdef DEBUG
    dyn = NULL;
#endif
}


void container::_overflow()
    { fatal(0x1002, "Container overflow"); }

void container::_idxerr()
    { fatal(0x1003, "Container index error"); }

const char* container::at(memint i) const
    { _idx(i); return data(i); }


void container::clear()
{
    _fin();
    _init();
}


char* container::assign(memint len)
{
    _fin();
    return _init(len);
}


void container::assign(const char* buf, memint len)
{
    ::memcpy(assign(len), buf, len);
}


void container::assign(const container& c)
{
    if (dyn != c.dyn)
    {
        _fin();
        _init(c);
    }
}


memint container::_calc_capcity(memint size)
{
    if (size <= 16)
        return 32;
    else if (size < 1024)
        return 2 * size;
    else
        return size + size / 4;
}


inline bool container::_can_shrink(memint newsize)
{
    return newsize > 32 && newsize < memsize() / 2;
}


rcdynamic* container::_grow_alloc(memint newsize)
{
    assert(newsize > 0);
    return rcdynamic::alloc(_calc_capcity(newsize), newsize);
}


rcdynamic* container::_precise_alloc(memint newsize)
{
    assert(newsize > 0);
    return rcdynamic::alloc(newsize, newsize);
}


rcdynamic* container::_grow_realloc(memint newsize)
{
    assert(newsize > 0);
    return rcdynamic::realloc(dyn, _calc_capcity(newsize), newsize);
}


rcdynamic* container::_precise_realloc(memint newsize)
{
    assert(newsize > 0);
    return rcdynamic::realloc(dyn, newsize, newsize);
}


char* container::_mkunique()
{
    rcdynamic* d = _precise_alloc(size());
    ::memcpy(d->data(), dyn->data(), size());
    _replace(d);
    return dyn->data();
}


char* container::insert(memint pos, memint len)
{
    _idxa(pos);
    if (unsigned(pos) > unsigned(size()))
        _idxerr();
    if (len <= 0)
        return NULL;
    if (empty())
        return assign(len);

    memint oldsize = size();
    memint newsize = oldsize + len;
    if (newsize < dyn->size() || newsize > ALLOC_MAX)
        _overflow();
    memint remain = oldsize - pos;

    if (unique())
    {
        if (newsize > dyn->memsize())
            dyn = _grow_realloc(newsize);
        else
            dyn->size(newsize);
        char* p = dyn->data(pos);
        if (remain)
            ::memmove(p + len, p, remain);
        return p;
    }
    else
    {
        rcdynamic* d = _grow_alloc(newsize);
        ::memcpy(d->data(), dyn->data(), pos);
        char* p = d->data(pos);
        if (remain)
            ::memcpy(p + len, dyn->data(pos), remain);
        _replace(d);
        return p;
    }
}


void container::insert(memint pos, const char* buf, memint len)
{
    memcpy(insert(pos, len), buf, len);
}


void container::insert(memint pos, const container& c)
{
    if (empty() && pos == 0)
        assign(c);
    else
        insert(pos, c.data(), c.size());
}


char* container::append(memint len)
{
    if (len <= 0)
        return NULL;
    memint oldsize = size();
    memint newsize = oldsize + len;
    if (oldsize == 0)
        return assign(len);
    if (newsize < oldsize || newsize > ALLOC_MAX)
        _overflow();
    else if (unique())
    {
        if (newsize > dyn->memsize())
            dyn = _grow_realloc(newsize);
        else
            dyn->size(newsize);
    }
    else
    {
        rcdynamic* d = _grow_alloc(newsize);
        ::memcpy(d->data(), dyn->data(), oldsize);
        _replace(d);
    }
    return dyn->data(oldsize);
}


void container::append(const char* buf, memint len)
{
    memcpy(append(len), buf, len);
}


void container::append(const container& c)
{
    if (empty())
        assign(c);
    else
        append(c.data(), c.size());
}


void container::erase(memint pos, memint len)
{
    _idx(pos);
    if (len <= 0 || empty())
        return;
    memint oldsize = size();
    memint epos = pos + len;
    _idxa(epos);
    memint newsize = oldsize - len;
    memint remain = oldsize - epos;
    if (newsize == 0)
        clear();
    else if (unique())
    {
        if (_can_shrink(newsize))
            dyn = _precise_realloc(newsize);
        if (remain)
            ::memmove(dyn->data(pos), dyn->data(epos), remain);
        dyn->size(newsize);
    }
    else
    {
        rcdynamic* d = _precise_alloc(newsize);
        ::memcpy(d->data(), dyn->data(), pos);
        if (remain)
            ::memcpy(d->data(pos), dyn->data(epos), remain);
        _replace(d);
    }
}


void container::pop_back(memint len)
{
    memint oldsize = size();
    memint newsize = oldsize - len;
    _idx(newsize);
    if (newsize == 0)
        clear();
    else if (unique())
    {
        dyn->size(newsize);
    }
    else
    {
        rcdynamic* d = _precise_alloc(newsize);
        ::memcpy(d->data(), dyn->data(), newsize);
        _replace(d);
    }
}


char* container::resize(memint newsize)
{
    if (newsize < 0)
        _overflow();
    memint oldsize = size();
    if (newsize == oldsize)
        return NULL;
    else if (newsize < oldsize)
    {
        erase(newsize, oldsize - newsize);
        return NULL;
    }
    else
        return append(newsize - oldsize);
}


void container::resize(memint newsize, char fill)
{
    memint oldsize = size();
    char* p = resize(newsize);
    if (p)
        memset(p, fill, newsize - oldsize);
}

// --- string -------------------------------------------------------------- //


const char* str::c_str() const
{
    memint t = size();
    if (t)
    {
        if (t == memsize())
        {
            ((str*)this)->push_back<char>(0);
            dyn->size(t);
        }
        *dyn->data(t) = 0;
    }
    return data();
}


int str::cmp(const char* s, memint blen) const
{
    memint alen = size();
    memint len = imin(alen, blen);
    if (len == 0)
        return alen - blen;
    int result = ::memcmp(data(), s, len);
    if (result == 0)
        return alen - blen;
    else
        return result;
}


memint str::find(char c) const
{
    if (empty())
        return npos;
    const char* p = data();
    const char* f = (const char*)::memchr(p, c, size());
    if (f == NULL)
        return npos;
    return f - p;
}


memint str::rfind(char c) const
{
    if (empty())
        return npos;
    const char* b = data();
    const char* p = b + size();
    do
    {
        if (*p == c)
            return p - b;
        p--;
    }
    while (p >= b);
    return npos;
}


void str::_init(const char* s)
{
    memint len = pstrlen(s);
    if (len <= 0)
        container::_init();
    else
        container::_init(s, len);
}


void str::operator+= (const char* s)
{
    memint len = pstrlen(s);
    if (len > 0)
        container::append(s, len);
}


str str::substr(memint pos, memint len) const
{
    if (pos == 0 && len == size())
        return *this;
    if (len <= 0)
        return str();
    _idx(pos);
    _idxa(pos + len);
    return str(data(pos), len);
}

// --- string utilities ---------------------------------------------------- //


static const char* _itobase(long long value, char* buf, int base, int& len, bool _signed)
{
    // internal conversion routine: converts the value to a string 
    // at the end of the buffer and returns a pointer to the first
    // character. this is to get rid of copying the string to the 
    // beginning of the buffer, since finally the string is supposed 
    // to be copied to a dynamic string in itostring(). the buffer 
    // must be at least 65 bytes long.

    static char digits[65] = 
        "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    char* pdigits;
    if (base > 36)
	pdigits = digits;       // start from '.'
    else
	pdigits = digits + 2;   // start from '0'
    
    int i = 64;
    buf[i] = 0;

    bool neg = false;
    unsigned long long v = value;
    if (_signed && base == 10 && value < 0)
    {
        v = -value;
        // since we can't handle the lowest signed value, we just return a built-in string.
        if (((long long)v) < 0)   // an minimum value negated results in the same value
        {
            if (sizeof(value) == 8)
            {
                len = 20;
                return "-9223372036854775808";
            }
            else if (sizeof(value) == 4)
            {
                len = 11;
                return "-2147483648";
            }
            else
                abort();
        }
        neg = true;
    }

    do
    {
        buf[--i] = pdigits[unsigned(v % base)];
        v /= base;
    } while (v > 0);

    if (neg)
        buf[--i] = '-';

    len = 64 - i;
    return buf + i;
}


static void _itobase2(str& result, long long value, int base, int width, char padchar, bool _signed)
{
    result.clear();

    if (base < 2 || base > 64)
        return;

    char buf[65];   // the longest possible string is when base=2
    int reslen;
    const char* p = _itobase(value, buf, base, reslen, _signed);

    if (width > reslen)
    {
        if (padchar == 0)
        {
            // default pad char
            if (base == 10)
                padchar = ' ';
            else if (base > 36)
                padchar = '.';
            else
                padchar = '0';
        }

        bool neg = *p == '-';
        if (neg) { p++; reslen--; }
        width -= reslen;
        if (width > 0)
            result.resize(width, padchar);
        result.append(p, reslen);
        if (neg)
            result.mkunique()[0] = '-';
    }
    else 
        result.assign(p, reslen);
}


str _to_string(long long value, int base, int width, char padchar) 
{
    str result;
    _itobase2(result, value, base, width, padchar, true);
    return result;
}


str _to_string(long long value)
{
    str result;
    _itobase2(result, value, 10, 0, ' ', true);
    return result;
}


str _to_string(memint value)
{
    str result;
    _itobase2(result, value, 10, 0, ' ', false);
    return result;
}


unsigned long long from_string(const char* p, bool* error, bool* overflow, int base)
{
    *error = false;
    *overflow = false;

    if (p == 0 || *p == 0 || base < 2 || base > 64)
        { *error = true; return 0; }

    unsigned long long result = 0;

    do 
    {
        int c = *p++;

        if (c >= 'a')
        {
            // for the numeration bases that use '.', '/', digits and
            // uppercase letters the letter case is insignificant.
            if (base <= 38)
                c -= 'a' - '9' - 1;
            else  // others use both upper and lower case letters
                c -= ('a' - 'Z' - 1) + ('A' - '9' - 1);
        }
        else if (c > 'Z')
            { *error = true; return 0; }
        else if (c >= 'A')
            c -= 'A' - '9' - 1;
        else if (c > '9')
            { *error = true; return 0; }

        c -= (base > 36) ? '.' : '0';
        if (c < 0 || c >= base)
            { *error = true; return 0; }

        unsigned long long t = result * unsigned(base);
        if (t / base != result)
            { *overflow = true; return 0; }
        result = t;
        t = result + unsigned(c);
        if (t < result)
            { *overflow = true; return 0; }
        result = t;

    }
    while (*p != 0);

    return result;
}


str remove_filename_path(const str& fn)
{
    memint i = fn.rfind('/');
    if (i == str::npos)
    {
        i = fn.rfind('\\');
        if (i == str::npos)
            return fn;
    }
    return fn.substr(i + 1);
}


str remove_filename_ext(const str& fn)
{
    memint i = fn.rfind('.');
    if (i == str::npos)
        return fn;
    return fn.substr(0, i);
}


