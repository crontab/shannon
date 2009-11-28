

#include "common.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>


// --- rcblock ------------------------------------------------------------- //


class rcblock: public noncopyable
{
    friend void test_rcblock();

protected:
    int refcount;

    void* operator new(size_t);
    void* operator new(size_t, memint extra);

    static rcblock* alloc(memint extra)
        { return new(extra) rcblock(); }
    static rcblock* realloc(rcblock*, memint size);

public:
    static int allocated; // for detecting memory leaks in DEBUG mode

    void  operator delete (void* p);

    rcblock(): refcount(0)  { }
    bool unique() const     { return refcount == 1; }
    void ref()              { pincrement(&refcount); }
    bool deref()            { assert(refcount >= 1); return pdecrement(&refcount) == 0; }
};


template <class T>
    inline T* rcref(T* o)           { if (o) o->ref(); return o; }
template <class T>
    inline T* rcref2(T* o)          { o->ref(); return o; }
template <class T>
    inline void rcrelease(T* o)     { if (o && o->deref()) delete o; }
template <class T>
    void rcassign(T*& p, T* o)      { if (p != o) { rcrelease(p); p = rcref(o); } }



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


class rcdynamic: public rcblock
{
    friend class container;
    friend class str;

protected:
    memint capacity;
    memint used;

    char* data() const          { return (char*)this + sizeof(rcdynamic); }
    char* data(memint i) const  { assert(i >= 0 && i <= used); return data() + i; }
    memint empty() const        { return used == 0; }
    memint size() const         { return used; }
    void size(memint _used)     { assert(_used > 0 && _used <= capacity); used = _used; }
    memint memsize() const      { return capacity; }

    static rcdynamic* alloc(memint _capacity, memint _used)
        { return new(_capacity) rcdynamic(_capacity, _used); }
    static rcdynamic* realloc(rcdynamic*, memint _capacity, memint _used);

    rcdynamic(memint _capacity, memint _used)
        : rcblock(), capacity(_capacity), used(_used)
                { assert(_capacity >= 0 && _used >= 0 && _used <= _capacity); }
};


rcdynamic* rcdynamic::realloc(rcdynamic* p, memint _capacity, memint _used)
{
    assert(p->unique());
    p = (rcdynamic*)rcblock::realloc(p, sizeof(rcdynamic) + _capacity);
    p->capacity = _capacity;
    p->used = _used;
    return p;
}


// --- container ----------------------------------------------------------- //


// Generic dynamic copy-on-write buffer for POD data
class container: public noncopyable
{
    friend void test_container();
    friend void test_string();

protected:
    struct _nulldyn: public rcdynamic
    {
        char dummy[16];
        _nulldyn(): rcdynamic(0, 0)  { }
        bool valid()  { return size() == 0 && memsize() == 0 && dummy[0] == 0; }
    };
    static _nulldyn _null;

    rcdynamic* dyn;

    void _init()                            { dyn = &_null; }
    char* _init(memint);
    void _init(const char*, memint);
    void _init(const container& c)          { dyn = rcref2(c.dyn); }
    void _dofin();
    void _fin()                             { if (!empty()) _dofin(); }

    static void _overflow();
    static void _idxerr();
    void _idx(memint i) const               { if (unsigned(i) >= unsigned(size())) _idxerr(); }
    void _idxa(memint i) const              { if (unsigned(i) > unsigned(size())) _idxerr(); }
    static memint _capgrow(memint size);
    static rcdynamic* grow_alloc(memint);
    static rcdynamic* precise_alloc(memint);
    rcdynamic* grow_realloc(memint);
    rcdynamic* precise_realloc(memint);

    memint memsize() const                  { return dyn->memsize(); }
    bool unique() const                     { return empty() || dyn->unique(); }

public:
    container()                             { _init(); }
    container(const char* buf, memint len)  { _init(buf, len); }
    container(const container& c)           { _init(c); }
    ~container()                            { _fin(); }

    bool empty() const                      { return dyn->empty(); }
    memint size() const                     { return dyn->size(); }
    const char* data() const                { return dyn->data(); }
          char* data()                      { return dyn->data(); }
    const char* data(memint i) const        { return dyn->data(i); }
          char* data(memint i)              { return dyn->data(i); }
    const char* at(memint i) const;
          char* at(memint i);

    void clear();
    char* assign(memint);
    void assign(const char*, memint);
    void assign(const container&);
    void operator= (const container& c)     { assign(c); }

    char* insert(memint pos, memint len);
    void insert(memint pos, const char*, memint len);
    void insert(memint pos, const container&);
    char* append(memint len);
    void append(const char*, memint len);
    void append(const container&);

    template <class T>
        void push_back(const T& t)          { ::new(append(1)) T(t); }
    void erase(memint pos, memint len);
    void erase(memint pos);
};


container::_nulldyn container::_null;


char* container::_init(memint len)
{
    if (len > 0)
    {
        dyn = rcref2(precise_alloc(len));
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

char* container::at(memint i)
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


memint container::_capgrow(memint size)
{
    if (size <= 16)
        return 32;
    else if (size < 1024)
        return 2 * size;
    else
        return size + size / 4;
}


rcdynamic* container::grow_alloc(memint newsize)
{
    return rcdynamic::alloc(_capgrow(newsize), newsize);
}


rcdynamic* container::precise_alloc(memint newsize)
{
    return rcdynamic::alloc(newsize, newsize);
}


rcdynamic* container::grow_realloc(memint newsize)
{
    return rcdynamic::realloc(dyn, _capgrow(newsize), newsize);
}


rcdynamic* container::precise_realloc(memint newsize)
{
    return rcdynamic::realloc(dyn, newsize, newsize);
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
            dyn = grow_realloc(newsize);
        else
            dyn->size(newsize);
        char* p = dyn->data(pos);
        if (remain)
            ::memmove(p + len, p, remain);
        return p;
    }
    else
    {
        rcdynamic* d = grow_alloc(newsize);
        ::memcpy(d->data(), dyn->data(), pos);
        char* p = d->data(pos);
        if (remain)
            ::memcpy(p + len, dyn->data(pos), remain);
        _fin();
        dyn = rcref2(d);
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
            dyn = grow_realloc(newsize);
        else
            dyn->size(newsize);
    }
    else
    {
        rcdynamic* d = grow_alloc(newsize);
        ::memcpy(d->data(), dyn->data(), oldsize);
        _fin();
        dyn = rcref2(d);
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
        if (oldsize > 32 && newsize < oldsize / 2)
            dyn = precise_realloc(newsize);
        if (remain)
            ::memmove(dyn->data(pos), dyn->data(epos), remain);
        dyn->size(newsize);
    }
    else
    {
        rcdynamic* d = precise_alloc(newsize);
        ::memcpy(d->data(), dyn->data(), pos);
        if (remain)
            ::memcpy(d->data(pos), dyn->data(epos), remain);
        _fin();
        dyn = rcref2(d);
    }
}


void container::erase(memint pos)
{
    erase(pos, size() - pos);
}


// --- string -------------------------------------------------------------- //


class str: public container
{
protected:
    void _init(const char*);

public:
    str(): container()                      { }
    str(const char* s)                      { _init(s); }
    str(const str& s): container(s)         { }
    str(const char* buf, memint len): container(buf, len)  { }
    ~str()                                  { }

    const char* c_str() const;
    int cmp(const char*, memint) const;

    bool operator== (const char* s) const   { return cmp(s, pstrlen(s)) == 0; }
    bool operator== (const str& s) const    { return cmp(s.data(), s.size()) == 0; }
    bool operator!= (const char* s) const   { return !(*this == s); }
    bool operator!= (const str& s) const    { return !(*this == s); }
    void operator+= (const char* s);
    void operator+= (const str& s)          { container::append(s); }
    str  substr(memint pos, memint len);
    str  substr(memint pos)                 { return substr(pos, size() - pos); }
};

bool operator== (const char* s1, const str& s2)
    { return s2.cmp(s1, pstrlen(s1)) == 0; }

bool operator!= (const char* s1, const str& s2)
    { return !(s1 == s2); }


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
    int result = memcmp(data(), s, len);
    if (result == 0)
        return alen - blen;
    else
        return result;
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


str str::substr(memint pos, memint len)
{
    if (pos == 0 && len == size())
        return *this;
    if (len <= 0)
        return str();
    _idx(pos);
    _idxa(pos + len);
    return str(data(pos), len);
}


// --- tests --------------------------------------------------------------- //


#define check(x) assert(x)


void test_rcblock()
{
    rcblock* b = rcref(rcblock::alloc(10));
    check(b->refcount == 1);
    rcblock* c = rcref(b);
    check(b->refcount == 2);
    rcrelease(c);
    check(b->refcount == 1);
    rcassign(b, rcblock::alloc(20));
    rcrelease(b);
}


void test_container()
{
    // TODO: check the number of reallocations
    container c1;
    check(c1.empty());
    check(c1.size() == 0);
    check(c1.memsize() == 0);

    container c2("ABC", 3);
    check(!c2.empty());
    check(c2.size() == 3);
    check(c2.memsize() == 3);

    check(c1.unique());
    check(c2.unique());
    c1.assign(c2);
    check(!c2.unique());
    check(!c1.unique());
    container c3("DEFG", 4);
    c1.insert(3, c3);
    check(c1.unique());
    check(c1.size() == 7);
    check(c1.memsize() > 7);
    check(memcmp(c1.data(), "ABCDEFG", 7) == 0);

    container c4 = c1;
    check(!c1.unique());
    c1.insert(3, "ab", 2);
    check(c1.unique());
    check(c1.size() == 9);
    check(c1.memsize() > 9);
    check(memcmp(c1.data(), "ABCabDEFG", 9) == 0);
    c1.insert(0, "@", 1);
    check(c1.unique());
    check(c1.size() == 10);
    check(memcmp(c1.data(), "@ABCabDEFG", 10) == 0);
    c1.insert(10, "0123456789", 10);
    check(c1.size() == 20);
    check(memcmp(c1.data(), "@ABCabDEFG0123456789", 20) == 0);

    c2.append(c2);
    check(memcmp(c2.data(), "ABCABC", 6) == 0);
    check(c2.size() == 6);
    c2.append("abcd", 4);
    check(c2.size() == 10);
    check(memcmp(c2.data(), "ABCABCabcd", 10) == 0);
    c4 = c2;
    check(!c2.unique());
    c2.append(c3);
    check(c2.unique());
    check(c2.size() == 14);
    check(memcmp(c2.data(), "ABCABCabcdDEFG", 14) == 0);

    c1.erase(4, 2);
    check(memcmp(c1.data(), "@ABCDEFG0123456789", 18) == 0);
    c4 = c1;
    c1.erase(8, 5);
    check(memcmp(c1.data(), "@ABCDEFG56789", 13) == 0);
    c1.erase(8, 5);
    check(memcmp(c1.data(), "@ABCDEFG", 8) == 0);

    c1.push_back('!');
    check(c1.size() == 9);
    check(memcmp(c1.data(), "@ABCDEFG!", 9) == 0);
    c4 = c1;
    c1.push_back('?');
    check(c1.size() == 10);
    check(memcmp(c1.data(), "@ABCDEFG!?", 10) == 0);

    check(container::_null.valid());
}


void test_string()
{
    str s1;
    check(s1.empty());
    check(s1.size() == 0);
    check(s1.c_str()[0] == 0);
    str s2 = "Kuku";
    check(!s2.empty());
    check(s2.size() == 4);
    check(s2.memsize() == 4);
    check(s2 == "Kuku");
    str s3 = s1;
    check(s3.empty());
    str s4 = s2;
    check(s4 == s2);
    check(s4 == "Kuku");
    check(!s4.unique());
    check(!s2.unique());
    str s5 = "!";
    check(s5.size() == 1);
    check(s5.c_str()[0] == '!');
    check(s5.c_str()[1] == 0);
    str s6 = "";
    check(s6.empty());
    check(s6.c_str()[0] == 0);
    s6 = s5;
    check(s6 == s5);
    s5 = s6;
    check(s6 == "!");
    s4 = "Mumu";
    check(s4 == "Mumu");
    check(*s4.data(2) == 'm');

    str s7 = "ABC";
    s7 += "DEFG";
    check(s7.size() == 7);
    check(s7 == "ABCDEFG");
    s7 += "HIJKL";
    check(s7.size() == 12);
    check(s7 == "ABCDEFGHIJKL");
    s7 += s4;
    check(s7.size() == 16);
    check(s7 == "ABCDEFGHIJKLMumu");
    s1 += "Bubu";
    check(s1 == "Bubu");
    check(s1.size() == 4);
    s1.append("Tutu", 4);
    check(s1.size() == 8);
    check(s1 == "BubuTutu");
    s1.erase(2, 4);
    check(s1.size() == 4);
    check(s1 == "Butu");
    check(s1.substr(1, 2) == "ut");
    check(s1.substr(1) == "utu");
    check(s1.substr(0) == "Butu");
}


int main()
{
    printf("%lu %lu %lu\n", sizeof(rcblock), sizeof(rcdynamic), sizeof(container));

    test_rcblock();
    test_container();
    test_string();

    if (rcblock::allocated != 0)
    {
        fprintf(stderr, "rcblock::allocated: %d\n", rcblock::allocated);
        _fatal(0xff01);
    }

    return 0;
}
