
#include "port.h"
#include "str.h"


static void stringoverflow()
{
    fatal(CRIT_FIRST + 21, "String/array overflow");
}


void string::idxerror()
{
    fatal(CRIT_FIRST + 20, "Index overflow");
}


int stralloc;

char   emptystrbuf[strrecsize + 4];
char*  emptystr = emptystrbuf + strrecsize;


string nullstring;


void string::_alloc(int newchars) 
{
    if (newchars <= 0)
        stringoverflow();
    int allocate = memquantize(newchars + strrecsize);
#ifdef DEBUG
    stralloc += allocate;
#endif
    data = (char*)(memalloc(allocate)) + strrecsize;
    STR_REFCOUNT(data) = 1;
    STR_LENGTH(data) = newchars;
    STR_CAPACITY(data) = allocate - strrecsize;
    if (newchars < STR_CAPACITY(data))
        data[newchars] = 0;
}


void string::_realloc(int newchars) 
{
#ifdef DEBUG
    if (newchars <= 0 || STR_LENGTH(data) <= 0)
        stringoverflow();
#endif
    int cap = STR_CAPACITY(data);
    if (newchars > cap || newchars < cap / 2) // grow faster, shrink slower
    {
        int allocate = memquantize(newchars + strrecsize);
        if (allocate != cap + strrecsize)
        {
#ifdef DEBUG
            stralloc += allocate - cap - strrecsize;
#endif
            data = (char*)(memrealloc(data - strrecsize, allocate)) + strrecsize;
            cap = STR_CAPACITY(data) = allocate - strrecsize;
        }
    }
    STR_LENGTH(data) = newchars;
    // Put NULL char after actual data so that c_str() can return the string
    // without modifying the object most of the time. The exception is when
    // the string occupies exactly "capacity" bytes, in which case c_str()
    // allocates additional data to make room for the NULL char. The reason
    // we don't always reserve the NULL char is that this class is used as a
    // basis for many containers classes as well, where the extra NULL char
    // and c_str() are not needed.
    if (newchars < cap)
        data[newchars] = 0;
}


inline void string::_freedata(char* data)
{
#ifdef DEBUG
    stralloc -= memquantize(STR_CAPACITY(data) + strrecsize);
#endif
    memfree((char*)(STR_BASE(data)));
}


void string::_free()
{
    _freedata(data);
    data = emptystr;
}


int string::_unlock()
{
#ifdef SINGLE_THREADED
    return --STR_REFCOUNT(data);
#else
    return pdecrement(&STR_REFCOUNT(data));
#endif
}


void string::initialize(const char* s1, int len1, const char* s2, int len2)
{
    if (len1 <= 0)
        initialize(s2, len2);
    else if (len2 <= 0)
        initialize(s1, len1);
    else
    {
        _alloc(len1 + len2);
        memcpy(data, s1, len1);
        memcpy(data + len1, s2, len2);
    }
}


void string::initialize(const char* sc, int initlen) 
{
    if (initlen <= 0 || sc == NULL)
        data = emptystr; 
    else 
    {
        _alloc(initlen);
        memmove(data, sc, initlen);
    }
}


void string::initialize(const char* sc) 
{
    initialize(sc, hstrlen(sc));
}


void string::initialize(char c) 
{
    _alloc(1);
    data[0] = c;
}


void string::initialize(const string& s)
{
    data = s.data;
#ifdef SINGLE_THREADED
    STR_REFCOUNT(data)++;
#else
    pincrement(&STR_REFCOUNT(data));
#endif
}


const char* string::c_str() const
{
    int len = STR_LENGTH(data);
    if (len != 0 && len == STR_CAPACITY(data))
    {
        // Mute the object so that we keep c_str() as a const method
        *(pstring(this)->appendn(1)) = 0;
        STR_LENGTH(data)--;
    }
    return data;
}


void string::_unref(string& s)
{
#ifdef DEBUG
    if (STR_LENGTH(s.data) == 0 || STR_REFCOUNT(s.data) <= 1)
        stringoverflow();
#endif
    if (s._unlock() == 0)
        _freedata(s.data);
}


void string::finalize() 
{
    if (STR_LENGTH(data) != 0)
    {
        if (_unlock() == 0)
            _freedata(data);
        data = emptystr;
    }
}


void string::assign(const char* sc, int initlen) 
{
    if (STR_LENGTH(data) > 0 && initlen > 0 && STR_REFCOUNT(data) == 1)
    {
        // reuse data buffer if unique
        _realloc(initlen);
        memmove(data, sc, initlen);
    }
    else
    {
        finalize();
        if (initlen == 1)
            initialize(sc[0]);
        else if (initlen > 1)
            initialize(sc, initlen);
    }
}


void string::assign(const char* sc) 
{
    assign(sc, hstrlen(sc));
}


void string::assign(char c) 
{
    assign(&c, 1);
}


void string::assign(const string& s) 
{
    if (data != s.data)
    {
        finalize();
        initialize(s);
    }
}


char* string::resize(int newlen)
{
    if (newlen < 0)
        return NULL;

    int curlen = STR_LENGTH(data);

    // if becoming empty
    if (newlen == 0)
        finalize();

    // if otherwise string was empty before
    else if (curlen == 0)
        _alloc(newlen);

    // if length is not changing, return a unique string
    else if (newlen == curlen)
        unique();

    // non-unique reallocation
    else if (STR_REFCOUNT(data) > 1)
    {
        char* odata = data;
        _alloc(newlen);
        int copylen = imin(curlen, newlen);
        memcpy(data, odata, copylen);
#ifdef SINGLE_THREADED
        STR_REFCOUNT(odata)--;
#else
        if (pdecrement(&STR_REFCOUNT(odata)) == 0)
            _freedata(odata);
#endif
    }

    // unique reallocation
    else
        _realloc(newlen);

    return data;
}


char* string::unique()
{
    if (STR_LENGTH(data) > 0 && STR_REFCOUNT(data) > 1)
    {
        char* odata = data;
        _alloc(STR_LENGTH(data));
        memcpy(data, odata, STR_LENGTH(data));
#ifdef SINGLE_THREADED
        STR_REFCOUNT(odata)--;
#else
        if (pdecrement(&STR_REFCOUNT(odata)) == 0)
            _freedata(odata);
#endif
    }
    return data;
}


char* string::appendn(int cnt)
{
    if (cnt <= 0)
        return NULL;
    int oldlen = STR_LENGTH(data);
    if (oldlen == 0)
        _alloc(cnt);
    else
        resize(oldlen + cnt);
    return data + oldlen;
}


void string::append(const char* sc, int catlen)
{
    int oldlen = STR_LENGTH(data);
    if (data == sc && catlen > 0 && oldlen > 0) // append itself
    {
        resize(oldlen + catlen);
        memmove(data + oldlen, data, catlen);
    }
    else
    {
        char* p = appendn(catlen);
        if (p != NULL)
            memmove(data + oldlen, sc, catlen);
    }
}


void string::append(const char* sc)
{
    append(sc, hstrlen(sc));
}


void string::append(char c)
{
    if (STR_LENGTH(data) == 0)
        assign(c);
    else 
    {
        resize(STR_LENGTH(data) + 1);
        data[STR_LENGTH(data) - 1] = c;
    }
}


void string::append(const string& s1)
{
    if (STR_LENGTH(data) == 0)
        assign(s1);
    else if (STR_LENGTH(s1.data) > 0)
        append(s1.data, STR_LENGTH(s1.data));
}


string string::copy(int from, int cnt) const
{
    if (STR_LENGTH(data) > 0 && from >= 0 && from < STR_LENGTH(data)) 
    {
        int l = imin(cnt, STR_LENGTH(data) - from);
        if (from == 0 && l == STR_LENGTH(data))
            return *this;
        else if (l > 0) 
            return string(data + from, l);
    }
    return string();
}


string string::operator+ (const char* sc) const  
{
    if (STR_LENGTH(data) == 0)
        return string(sc);
    else
        return string(data, STR_LENGTH(data), sc, hstrlen(sc));
}


string string::operator+ (char c) const
{ 
    if (STR_LENGTH(data) == 0)
        return string(c);
    else
        return string(data, STR_LENGTH(data), &c, 1);
}


string string::operator+ (const string& s) const
{
    if (STR_LENGTH(data) == 0)
        return s;
    else if (STR_LENGTH(s.data) == 0)
        return *this;
    else
        return string(data, STR_LENGTH(data), s.data, STR_LENGTH(s.data));
}


string operator+ (const char* sc, const string& s)
{
    if (STR_LENGTH(s.data) == 0)
        return string(sc);
    else
        return string(sc, hstrlen(sc), s.data, STR_LENGTH(s.data));
}


string operator+ (char c, const string& s)
{
    if (STR_LENGTH(s.data) == 0)
        return string(c);
    else
        return string(&c, 1, s.data, STR_LENGTH(s.data));
}


bool string::operator== (const char* s) const
{
    int len = hstrlen(s);
    return (STR_LENGTH(data) == len) && ((len == 0) || (memcmp(data, s, len) == 0));
}


bool string::operator== (const string& s) const 
{
    return (STR_LENGTH(data) == STR_LENGTH(s.data))
        && ((STR_LENGTH(data) == 0) || (memcmp(data, s.data, STR_LENGTH(data)) == 0));
}


bool string::operator== (char c) const 
{
    return (STR_LENGTH(data) == 1) && (data[0] == c);
}


int string::compare(const string& s) const
{
    int alen = STR_LENGTH(data);
    int blen = STR_LENGTH(s.data);
    int len = imin(alen, blen);
    if (len == 0)
        return alen - blen;
    int result = memcmp(data, s.data, len);
    if (result == 0)
        return alen - blen;
    else
        return result;
}


void string::del(int from, int cnt)
{
    int l = STR_LENGTH(data);
    int d = l - from;
    if (from >= 0 && d > 0 && cnt > 0) 
    {
        if (cnt < d)
        {
            unique();
            memmove(data + from, data + from + cnt, d - cnt);
        }
        else
            cnt = d;
        resize(l - cnt);
    }
}


char* string::ins(int where, int len)
{
    int curlen = STR_LENGTH(data);
    if (len > 0 && where >= 0 && where <= curlen) 
    {
        if (curlen == 0)
        {
            resize(len);
            return data;
        }
        resize(curlen + len);
        char* p = data + where;
        int movelen = STR_LENGTH(data) - where - len;
        if (movelen > 0) 
            memmove(p + len, p, movelen);
        return p;
    }
    return NULL;
}


void string::ins(int where, const char* what, int len)
{
    char* p = ins(where, len);
    if (p != NULL)
        memmove(p, what, len);
}


void string::ins(int where, const char* what)
{
    ins(where, what, hstrlen(what));
}


void string::ins(int where, const string& what)
{
    ins(where, what.data, STR_LENGTH(what.data));
}


