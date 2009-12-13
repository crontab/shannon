

#include "runtime.h"
#include "typesys.h"


// --- object & objptr ----------------------------------------------------- //


int object::allocated = 0;


object* object::_realloc(object* p, size_t self, memint extra)
{
    assert(p->unique());
    assert(self > 0 && extra >= 0);
    return (object*)::pmemrealloc(p, self + extra);
}


void object::_assignto(object*& p)
    { if (p != this) { p->release(); p = this->ref(); } }


void* object::operator new(size_t self)
{
    void* p = ::pmemalloc(self);
#ifdef DEBUG
    pincrement(&object::allocated);
#endif
    return p;
}


void* object::operator new(size_t self, memint extra)
{
    assert(self + extra > 0);
    void* p = ::pmemalloc(self + extra);
#ifdef DEBUG
    pincrement(&object::allocated);
#endif
    return p;
}


void object::operator delete(void* p)
{
    assert(((object*)p)->refcount == 0);
#ifdef DEBUG
    pdecrement(&object::allocated);
#endif
    ::pmemfree(p);
}


object::~object()  { }


void object::release()
{
    if (this == NULL)
        return;
    assert(refcount > 0);
    if (pdecrement(&refcount) == 0)
        delete this;
}


rtobject::~rtobject()
    { }


// --- range --------------------------------------------------------------- //

/*
range::cont::~cont()
    { }

range::cont range::null;


void range::operator= (const range& r)
{
    if (!operator==(r))
        { _fin(); _init(r); }
}


void range::assign(integer l, integer r)
{
    if (!equals(l, r))
        { _fin(); _init(l, r); }
}


bool range::operator== (const range& other) const
{
    return this == &other ||
        (obj->left == other.obj->left && obj->right == other.obj->right);
}


memint range::compare(const range& r) const
{
    memint result = memint(obj->left - r.obj->left);
    if (result == 0)
        result = memint(obj->right - r.obj->right);
    return result;
}
*/


// --- ordset -------------------------------------------------------------- //

ordset::cont::cont() { }
ordset::cont::~cont() { }

ordset::cont ordset::null;


void ordset::_mkunique()
{
    _fin();
    obj = (new cont())->ref<cont>();
}


void ordset::operator= (const ordset& s)
{
    if (!operator==(s))
        { _fin(); _init(s); }
}


// --- container & contptr ------------------------------------------------- //


container::~container()
    { }

void container::overflow()
    { fatal(0x1002, "Container overflow"); }

void container::idxerr()
    { fatal(0x1003, "Container index error"); }

memint container::compare(memint index, void* key) const
    { _fatal(0x1004); return 0; }


inline memint container::calc_prealloc(memint newsize)
{
    if (newsize <= 32)
        return 64;
    else
        return newsize + newsize / 2;
}


/*
inline bool container::can_shrink(memint newsize)
{
    return newsize > 64 && newsize < _capacity / 2;
}
*/

container* container::new_growing(memint newsize)
{
    if (newsize <= 0)
        overflow();
    memint newcap = _capacity > 0 ? calc_prealloc(newsize) : newsize;
    if (newcap <= 0)
        overflow();
    return new_(newcap, newsize);
}


container* container::new_precise(memint newsize)
{
    if (newsize <= 0)
        overflow();
    return new_(newsize, newsize);
}


container* container::realloc(memint newsize)
{
    if (newsize <= 0)
        overflow();
    assert(unique());
    assert(newsize > _capacity || newsize < _size);
    _size = newsize;
    _capacity = _size > _capacity ? calc_prealloc(_size) : _size;
    if (_capacity <= 0)
        overflow();
    return (container*)_realloc(this, sizeof(*this), _capacity);
}


bool container::bsearch(void* key, memint& index, memint count) const
{
    return ::bsearch(*this, count - 1, key, index);
}
/*
{
    memint l, i, c;
    l = 0;
    h--;
    bool ret = false;
    while (l <= h) 
    {
        i = (l + h) / 2;
        c = compare(i, key);
        if (c < 0)
            l = i + 1;
        else
        {
            h = i - 1;
            if (c == 0)
                ret = true;
        }
    }
    index = l;
    return ret;
}
*/


char* contptr::_init(container* factory, memint len)
{
    assert(len >= 0);
    if (len > 0)
    {
        obj = factory->new_growing(len)->ref();
        return obj->data();
    }
    else
    {
        obj = factory->null_obj();
        return NULL;
    }
}


void contptr::_init(container* factory, const char* buf, memint len)
{
    char* p = _init(factory, len);
    if (p)
        factory->copy(p, buf, len);
}


void contptr::_fin()
    { if (!empty()) obj->release(); }


const char* contptr::back(memint i) const
{
    if (i <= 0 || i > size())
        container::idxerr();
    return obj->end() - i;
}


void contptr::operator= (const contptr& s)
{
    if (obj != s.obj)
        _assign(s.obj);
}


void contptr::assign(const char* buf, memint len)
{
    container* null = obj->null_obj();
    _fin();
    _init(null, buf, len);
}


void contptr::clear()
{
    if (!empty())
    {
        // Preserve the original object type by getting the null obj from
        // the factory.
        container* null = obj->null_obj();
        _fin();
        obj = null;
    }
}


char* contptr::mkunique()
{
    if (empty() || unique())
        return obj->data();
    else
    {
        container* o = obj->new_precise(obj->size());
        obj->copy(o->data(), obj->data(), obj->size());
        _assign(o);
        return obj->data();
    }
}


char* contptr::_insertnz(memint pos, memint len)
{
    assert(len > 0);
    chkidxa(pos);
    memint oldsize = size();
    memint newsize = oldsize + len;
    memint remain = oldsize - pos;
    if (unique())
    {
        if (newsize > obj->capacity())
            obj = obj->realloc(newsize);
        else
            obj->set_size(newsize);
        char* p = obj->data(pos);
        if (remain)
            ::memmove(p + len, p, remain);
        return p;
    }
    else
    {
        container* o = obj->new_growing(newsize);
        if (pos)
            obj->copy(o->data(), obj->data(), pos);
        char* p = o->data(pos);
        if (remain)
            obj->copy(p + len, obj->data(pos), remain);
        _assign(o);
        return p;
    }
}


char* contptr::_appendnz(memint len)
{
    assert(len > 0);
    memint oldsize = size();
    memint newsize = oldsize + len;
    if (unique())
    {
        if (newsize > obj->capacity())
            obj = obj->realloc(newsize);
        else
            obj->set_size(newsize);
        return obj->data(oldsize);
    }
    else
    {
        container* o = obj->new_growing(newsize);
        if (oldsize)
            obj->copy(o->data(), obj->data(), oldsize);
        _assign(o);
        return obj->data(oldsize);
    }
}


void contptr::_erasenz(memint pos, memint len)
{
    chkidx(pos);
    memint oldsize = size();
    memint epos = pos + len;
    chkidxa(epos);
    memint newsize = oldsize - len;
    memint remain = oldsize - epos;
    if (newsize == 0)
        clear();
    else if (unique())
    {
        char* p = obj->data(pos);
        obj->finalize(p, len);
        if (remain)
            ::memmove(p, p + len, remain);
        obj->set_size(newsize);
    }
    else
    {
        container* o = obj->new_precise(newsize);
        if (pos)
            obj->copy(o->data(), obj->data(), pos);
        if (remain)
            obj->copy(o->data(pos), obj->data(epos), remain);
        _assign(o);
    }
}


void contptr::_popnz(memint len)
{
    memint oldsize = size();
    memint newsize = oldsize - len;
    chkidx(newsize);
    if (newsize == 0)
        clear();
    else if (unique())
    {
        obj->finalize(obj->data(newsize), len);
        obj->set_size(newsize);
    }
    else
    {
        container* o = obj->new_precise(newsize);
        obj->copy(o->data(), obj->data(), newsize);
        _assign(o);
    }
}


void contptr::insert(memint pos, const char* buf, memint len)
{
    if (len > 0)
        obj->copy(_insertnz(pos, len), buf, len);
}


void contptr::insert(memint pos, const contptr& s)
{
    if (empty())
    {
        if (pos)
            container::idxerr();
        _init(s);
    }
    else
    {
        memint len = s.size();
        if (len)
        {
            // Be careful as s maybe the same as (*this)
            char* p = _insertnz(pos, len);
            obj->copy(p, s.data(), len);
        }
    }
}


void contptr::append(const char* buf, memint len)
{
    if (len > 0)
        obj->copy(_appendnz(len), buf, len);
}


void contptr::append(const contptr& s)
{
    if (empty())
        _init(s);
    else
    {
        memint len = s.size();
        if (len)
        {
            // Be careful as s maybe the same as (*this)
            char* p = _appendnz(len);
            obj->copy(p, s.data(), len);
        }
    }
}


char* contptr::resize(memint newsize)
{
    if (newsize < 0)
        container::overflow();
    memint oldsize = size();
    if (newsize == oldsize)
        return NULL;
    else if (newsize == 0)
    {
        clear();
        return NULL;
    }
    else if (newsize < oldsize)
    {
        _erasenz(newsize, oldsize - newsize);
        return NULL;
    }
    else
        return _appendnz(newsize - oldsize);
}


void contptr::resize(memint newsize, char fill)
{
    memint oldsize = size();
    char* p = resize(newsize);
    if (p)
        memset(p, fill, newsize - oldsize);
}



// --- string -------------------------------------------------------------- //


str::cont::~cont()  { }

str::cont str::null;


container* str::cont::new_(memint cap, memint siz)
    { return new(cap) cont(cap, siz); }

container* str::cont::null_obj()
    { return &str::null; }

void str::cont::finalize(void*, memint)
    { }

void str::cont::copy(void* dest, const void* src, memint len)
    { ::memcpy(dest, src, len); }


void str::_init(const char* buf, memint len)
{
    if (len > 0)
    {
        // Reserve extra byte for the NULL char
        contptr::_init(&null, buf, len + 1);
        obj->dec_size();
    }
    else
        _init();
}


void str::_init(const char* s)
{
    memint len = pstrlen(s);
    if (len > 0)
        _init(s, len);
    else
        _init();
}


const char* str::c_str() const
{
    if (empty())
        return "";
    else if (obj->has_room())
        *obj->end() = 0;
    else
    {
        ((str*)this)->push_back(char(0));
        obj->dec_size();
    }
    return obj->data();
}


void str::operator= (const char* s)
{
    _fin();
    _init(s);
}


void str::operator= (char c)
{
    _fin();
    _init(&c, 1);
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


memint str::compare(const char* s, memint blen) const
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


void str::operator+= (const char* s)
{
    memint len = pstrlen(s);
    if (len > 0)
    {
        append(s, len + 1);
        obj->dec_size();
    }
}


str str::substr(memint pos, memint len) const
{
    if (pos == 0 && len == size())
        return *this;
    if (len <= 0)
        return str();
    chkidxa(pos);
    chkidxa(pos + len);
    return str(data(pos), len);
}


str str::substr(memint pos) const
{
    if (pos == 0)
        return *this;
    chkidxa(pos);
    return str(data(pos), size() - pos);
}


// --- string utilities ---------------------------------------------------- //


static const char* _itobase(large value, char* buf, int base, int& len, bool _signed)
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
    ularge v = value;
    if (_signed && base == 10 && value < 0)
    {
        v = -value;
        // since we can't handle the lowest signed value, we just return a built-in string.
        if (large(v) < 0)   // a minimum value negated results in the same value
        {
            if (sizeof(value) == 8)
            {
                len = 20;
                return "-9223372036854775808";
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


static void _itobase2(str& result, large value, int base, int width, char padchar, bool _signed)
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
            result.replace(0, '-');
    }
    else 
        result.assign(p, reslen);
}


str _to_string(large value, int base, int width, char padchar) 
{
    str result;
    _itobase2(result, value, base, width, padchar, true);
    return result;
}


str _to_string(large value)
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


ularge from_string(const char* p, bool* error, bool* overflow, int base)
{
    *error = false;
    *overflow = false;

    if (p == 0 || *p == 0 || base < 2 || base > 64)
        { *error = true; return 0; }

    ularge result = 0;

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

        ularge t = result * unsigned(base);
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


static const charset printable_chars = "~20-~7E~81-~FE";


static void _to_printable(char c, str& s)
{
    if (c == '\\')
        s += "\\\\";
    else if (c == '\'')
        s += "\\\'";
    else if (printable_chars[c])
        s.append(&c, 1);
    else
    {
        s += "\\x";
        s += to_string(uchar(c), 16, 2, '0');
    }
}


str to_printable(char c)
{
    str result;
    _to_printable(c, result);
    return result;
}


str to_printable(const str& s)
{
    str result;
    for (memint i = 0; i < s.size(); i++)
        _to_printable(s[i], result);
    return result;
}


str to_quoted(char c)
    { return "'" + to_printable(c) + "'"; }


str to_quoted(const str& s)
    { return "'" + to_printable(s) + "'"; }


// --- object collection --------------------------------------------------- //


void objvec_impl::release_all()
{
    memint count = size();
    while (count--)
        operator[](count)->release();
    clear();
}


symbol::~symbol()  { }

symtbl::cont symtbl::null;

container* symtbl::cont::new_(memint cap, memint siz)
    { return new(cap) cont(cap, siz); }

container* symtbl::cont::null_obj()
    { return &symtbl::null; }

memint symtbl::cont::compare(memint index, void* key) const
    { return (*container::data<symbol*>(index))->name.compare(*(str*)key); }

symtbl::cont::~cont()
    { }


// --- ecmessag/emessage --------------------------------------------------- //


ecmessage::ecmessage(const char* _msg): msg(_msg)  { }
ecmessage::~ecmessage()  { }
const char* ecmessage::what() const  { return msg; }

emessage::emessage(const str& _msg): msg(_msg)  { }
emessage::emessage(const char* _msg): msg(_msg)  { }
emessage::~emessage()  { }
const char* emessage::what() const  { return msg.c_str(); }


static str sysErrorStr(int code, const str& arg)
{
    // For some reason strerror_r() returns garbage on my 64-bit Ubuntu. That's unfortunately
    // not the only strange thing about this computer and OS. Could be me, could be hardware
    // or could be Linux. Or all.
    // Upd: so I updated both hardware and OS, still grabage on 64 bit, but OK on 32-bit.
    // What am I doing wrong?
//    char buf[1024];
//    strerror_r(code, buf, sizeof(buf));
    str result = strerror(code);
    if (!arg.empty())
        result += " (" + arg + ")";
    return result;
}


esyserr::esyserr(int code, const str& arg)
    : emessage(sysErrorStr(code, arg))  { }

esyserr::~esyserr()  { }


// --- variant ------------------------------------------------------------- //


template class vector<variant>;
template class podvec<variant>;
template class set<variant>;
template class dict<variant, variant>;


variant::_None variant::none;


void variant::_fin_refcnt()
{
    switch(type)
    {
    case NONE:
    case ORD:
    case REAL:      break;
    case STR:       _str().~str(); break;
    case VEC:       _vec().~varvec(); break;
    case SET:       _set().~varset(); break;
    case ORDSET:    _ordset().~ordset(); break;
    case DICT:      _dict().~vardict(); break;
    case RTOBJ:     _rtobj()->release(); break;
    }
}


void variant::_type_err() { throw ecmessage("Variant type mismatch"); }
void variant::_range_err() { throw ecmessage("Variant range error"); }


void variant::_init(const variant& v)
{
    type = v.type;
    val = v.val;
    if (is_refcnt())
        val._obj->ref();
}


memint variant::compare(const variant& v) const
{
    if (type == v.type)
    {
        switch(type)
        {
        case NONE:  return 0;
        case ORD:
            integer d = val._ord - v.val._ord;
            return d < 0 ? -1 : d > 0 ? 1 : 0;
        case REAL:  return val._real < v.val._real ? -1 : (val._real > v.val._real ? 1 : 0);
        case STR:   return _str().compare(v._str());
        // TODO: define "deep" comparison? but is it really needed for hashing?
        case VEC:
        case SET:
        case ORDSET:
        case DICT:
        case RTOBJ: return memint(_rtobj()) - memint(v._rtobj());
        }
    }
    return int(type - v.type);
}


bool variant::operator== (const variant& v) const
{
    if (type == v.type)
    {
        switch(type)
        {
        case NONE:      return true;
        case ORD:       return val._ord == v.val._ord;
        case REAL:      return val._real == v.val._real;
        case STR:       return _str() == v._str();
        case VEC:       return _vec() == v._vec();
        case SET:       return _set() == v._set();
        case ORDSET:    return _ordset() == v._ordset();
        case DICT:      return _dict() == v._dict();
        case RTOBJ:     return _rtobj() == v._rtobj();
        }
    }
    return false;
}


bool variant:: empty() const
{
    switch(type)
    {
    case NONE:      return true;
    case ORD:       return val._ord == 0;
    case REAL:      return val._real == 0;
    case STR:       return _str().empty();
    case VEC:       return _vec().empty();
    case SET:       return _set().empty();
    case ORDSET:    return _ordset().empty();
    case DICT:      return _dict().empty();
    case RTOBJ:     return _rtobj()->empty();
    }
    return false;
}


// --- runtime objects ----------------------------------------------------- //


reference::~reference() { }

bool reference::empty() const  { return var.empty(); }


#ifdef DEBUG
void stateobj::idxerr()
    { fatal(0x1005, "Internal: object access error"); }
#endif

stateobj::stateobj(State* t)
    : rtobject(t)  { }

bool stateobj::empty() const
    { return false; }

stateobj::~stateobj()
{
    if (type() != NULL)
    {
        for (memint count = type()->selfVarCount(); count--; )
            vars[count].~variant();
    }
}

stateobj* stateobj::new_(State* type)
{
    memint varcount = type->selfVarCount();
    stateobj* obj = new(varcount * sizeof(variant)) stateobj(type);
#ifdef DEBUG
    obj->varcount = varcount;
#endif
    return obj;
}


// --- FIFO ---------------------------------------------------------------- //


#ifdef DEBUG
memint memfifo::CHUNK_SIZE = _varsize * 16;
#endif


fifo::fifo(Type* rt, bool is_char)
    : rtobject(rt), _is_char_fifo(is_char)  { }

fifo::~fifo()
    { }

void fifo::_empty_err()                { throw emessage("FIFO empty"); }
void fifo::_wronly_err()               { throw emessage("FIFO is write-only"); }
void fifo::_rdonly_err()               { throw emessage("FIFO is read-only"); }
void fifo::_fifo_type_err()            { fatal(0x1002, "FIFO type mismatch"); }
const char* fifo::get_tail()           { _wronly_err(); return NULL; }
const char* fifo::get_tail(memint*)    { _wronly_err(); return NULL; }
void fifo::deq_bytes(memint)           { _wronly_err(); }
variant* fifo::enq_var()               { _rdonly_err(); return NULL; }
memint fifo::enq_chars(const char*, memint)  { _rdonly_err(); return 0; }
bool fifo::empty() const               { _rdonly_err(); return true; }
void fifo::flush()                     { }


void fifo::_req_non_empty() const
{
    if (empty())
        _empty_err();
}


void fifo::_req_non_empty(bool ch) const
{
    _req(ch);
    if (empty())
        _empty_err();
}


int fifo::preview()
{
    _req(true);
    const char* p = get_tail();
    if (p == NULL)
        return -1;
    return *p;
}


uchar fifo::get()
{
    int c = preview();
    if (c == -1)
        _empty_err();
    deq_bytes(1);
    return c;
}


bool fifo::get_if(char c)
{
    int d = preview();
    if (d != -1 && d == c)
    {
        deq_bytes(1);
        return true;
    }
    return false;
}


bool fifo::eol()
{
    _req(true);
    const char* p = get_tail();
    if (p == NULL)
        return true;
    return *p == '\r' || *p == '\n';
}


void fifo::skip_eol()
{
    // Support all 3 models: DOS, UNIX and MacOS
    int c = preview();
    if (c == '\r')
    {
        get();
        c = preview();
    }
    if (c == '\n')
        get();
}

/*
int fifo::skip_indent()
{
    static const charset ws = " \t";
    int result = 0;
    str s = deq(ws);
    for (str::const_iterator p = s.begin(); p != s.end(); p++)
        switch (*p)
        {
        case ' ': result++; break;
        case '\t': result = ((result / TAB_SIZE) + 1) * TAB_SIZE; break;
        }
    return result;
}
*/

void fifo::var_eat()
{
    if (is_char_fifo())
        get();
    else
    {
        _req_non_empty();
        ((variant*)get_tail())->~variant();
        deq_bytes(_varsize);
    }
}


void fifo::var_preview(variant& v)
{
    if (empty())
        v.clear();
    else if (is_char_fifo())
        v = *get_tail();
    else
        v = *(variant*)get_tail();
}


void fifo::var_deq(variant& v)
{
    if (is_char_fifo())
        v = get();
    else
    {
        _req_non_empty();
        v.clear();
        memcpy((char*)&v, get_tail(), _varsize);
        deq_bytes(_varsize);
    }
}


void fifo::var_enq(const variant& v)
{
    if (is_char_fifo())
    {
        if (v.is(variant::STR))
            enq(v.as_str());
        else
            enq(v.as_char());
    }
    else
        ::new(enq_var()) variant(v);
}


str fifo::deq(memint count)
{
    _req_non_empty(true);
    str result;
    while (count > 0)
    {
        memint avail;
        const char* p = get_tail(&avail);
        if (p == NULL)
            break;
        if (count < avail)
            avail = count;
        result.append(p, avail);
        deq_bytes(avail);
        if (count == CHAR_SOME)
            break;
        count -= avail;
    }
    return result;
}


void fifo::_token(const charset& chars, str* result)
{
    _req(true);
    while (1)
    {
        memint avail;
        const char* b = get_tail(&avail);
        if (b == NULL)
            break;
        const char* p = b;
        const char* e = b + avail;
        while (p < e && chars[*p])
            p++;
        memint count = p - b;
        if (count == 0)
            break;
        if (result != NULL)
            result->append(b, count);
        deq_bytes(count);
        if (count < avail)
            break;
    }
}


str fifo::line()
{
    static charset linechars = ~charset("\r\n");
    str result;
    _token(linechars, &result);
    skip_eol();
    return result;
}


void fifo::enq(const char* s)   { if (s != NULL) enq(s, strlen(s)); }
void fifo::enq(const str& s)    { enq_chars(s.data(), s.size()); }
void fifo::enq(char c)          { enq_chars(&c, 1); }
void fifo::enq(uchar c)         { enq_chars((char*)&c, 1); }
void fifo::enq(large i)         { enq(to_string(i)); }


// --- memfifo ------------------------------------------------------------- //


memfifo::memfifo(Type* rt, bool ch)
    : fifo(rt, ch), head(NULL), tail(NULL), head_offs(0), tail_offs(0)  { }


memfifo::~memfifo()                     { clear(); }
inline const char* memfifo::get_tail()  { return tail->data + tail_offs; }
inline bool memfifo::empty() const      { return tail == NULL; }
inline variant* memfifo::enq_var()      { _req(false); return (variant*)enq_space(_varsize); }


void memfifo::clear()
{
    // TODO: also define fifos for POD variant types for faster destruction
    if (is_char_fifo())
    {
        while (tail != NULL)
        {
#ifdef DEBUG
            head_offs = tail_offs = CHUNK_SIZE;
#endif
            deq_chunk();
        }
    }
    else
    {
        while (tail != NULL)
        {
            ((variant*)get_tail())->~variant();
            deq_bytes(_varsize);
        }
    }
}


void memfifo::deq_chunk()
{
    assert(tail != NULL && head != NULL);
    chunk* c = tail;
    tail = tail->next;
    delete c;
    if (tail == NULL)
    {
        assert(head_offs == tail_offs);
        head = NULL;
        head_offs = tail_offs = 0;
    }
    else
    {
        assert(tail_offs == CHUNK_SIZE);
        tail_offs = 0;
    }
}


void memfifo::enq_chunk()
{
    chunk* c = new chunk();
    if (head == NULL)
    {
        assert(tail == NULL && head_offs == 0);
        head = tail = c;
    }
    else
    {
        assert(head_offs == CHUNK_SIZE);
        head->next = c;
        head = c;
        head_offs = 0;
    }
}


const char* memfifo::get_tail(memint* count)
{
    if (tail == NULL)
    {
        *count = 0;
        return NULL;
    }
    if (tail == head)
        *count = head_offs - tail_offs;
    else
        *count = CHUNK_SIZE - tail_offs;
    assert(*count <= CHUNK_SIZE);
    return tail->data + tail_offs;
}


void memfifo::deq_bytes(memint count)
{
    tail_offs += count;
    assert(tail != NULL && tail_offs <= ((tail == head) ? head_offs : CHUNK_SIZE));
    if (tail_offs == ((tail == head) ? head_offs : CHUNK_SIZE))
        deq_chunk();
}


memint memfifo::enq_avail()
{
    if (head == NULL || head_offs == CHUNK_SIZE)
        return CHUNK_SIZE;
    return CHUNK_SIZE - head_offs;
}


char* memfifo::enq_space(memint count)
{
    if (head == NULL || head_offs == CHUNK_SIZE)
        enq_chunk();
    assert(count <= CHUNK_SIZE - head_offs);
    char* result = head->data + head_offs;
    head_offs += count;
    return result;
}


memint memfifo::enq_chars(const char* p, memint count)
{
    _req(true);
    memint save_count = count;
    while (count > 0)
    {
        memint avail = enq_avail();
        if (count < avail)
            avail = count;
        memcpy(enq_space(avail), p, avail);
        count -= avail;
        p += avail;
    }
    return save_count;
}


// --- buffifo ------------------------------------------------------------- //


buffifo::buffifo(Type* rt, bool is_char)
  : fifo(rt, is_char), buffer(NULL), bufsize(0), bufhead(0), buftail(0)  { }

buffifo::~buffifo()  { }
bool buffifo::empty() const { _wronly_err(); return true; }
void buffifo::flush() { _rdonly_err(); }


const char* buffifo::get_tail()
{
    assert(buftail <= bufhead && bufhead <= bufsize);
    if (buftail == bufhead && empty())
        return NULL;
    assert(bufhead > buftail);
    return buffer + buftail;
}


const char* buffifo::get_tail(memint* count)
{
    assert(buftail <= bufhead && bufhead <= bufsize);
    if (buftail == bufhead && empty())
    {
        *count = 0;
        return NULL;
    }
    *count = bufhead - buftail;
    assert(*count >= (is_char_fifo() ? 1 : _varsize));
    return buffer + buftail;
}


void buffifo::deq_bytes(memint count)
{
    assert(buftail <= bufhead && bufhead <= bufsize);
    assert(count <= bufhead - buftail);
    buftail += count;
}


variant* buffifo::enq_var()
{
    _req(false);
    assert(buftail <= bufhead && bufhead <= bufsize);
    if (bufhead + _varsize > bufsize)
        flush();
    assert(bufhead + _varsize <= bufsize);
    variant* result = (variant*)(buffer + bufhead);
    bufhead += _varsize;
    return result;
}


memint buffifo::enq_avail()
{
    assert(buftail <= bufhead && bufhead <= bufsize);
    if (bufhead == bufsize)
        flush();
    assert(bufhead < bufsize);
    return bufsize - bufhead;
}


char* buffifo::enq_space(memint count)
{
    assert(buftail <= bufhead && bufhead <= bufsize);
    assert(count <= bufsize - bufhead);
    char* result = buffer + bufhead;
    bufhead += count;
    return result;
}


memint buffifo::enq_chars(const char* p, memint count)
{
    _req(true);
    memint save_count = count;
    while (count > 0)
    {
        memint avail = enq_avail();
        if (count < avail)
            avail = count;
        memcpy(enq_space(avail), p, avail);
        count -= avail;
        p += avail;
    }
    return save_count;
}


// --- strfifo ------------------------------------------------------------- //


strfifo::strfifo(Type* rt): buffifo(rt, true), string() {}
strfifo::~strfifo() { }


strfifo::strfifo(Type* rt, const str& s)
    : buffifo(rt, true), string(s)
{
    buffer = (char*)s.data();
    bufhead = bufsize = s.size();
}


void strfifo::clear()
{
    string.clear();
    buffer = NULL;
    buftail = bufhead = bufsize = 0;
}


bool strfifo::empty() const
{
    if (buftail == bufhead)
    {
        if (!string.empty())
            ((strfifo*)this)->clear();
        return true;
    }
    return false;
}


void strfifo::flush()
{
    assert(bufhead == bufsize);
    string.resize(string.size() + memfifo::CHUNK_SIZE);
    buffer = (char*)string.data();
    bufsize += memfifo::CHUNK_SIZE;
}


str strfifo::all() const
{
    if (string.empty() || buftail == bufhead)
        return str();
    return string.substr(buftail, bufhead - buftail);
}


// --- intext -------------------------------------------------------------- //


intext::intext(Type* rt, const str& fn)
    : buffifo(rt, true), file_name(fn), _fd(-1), _eof(false)  { }
intext::~intext()
    { if (_fd > 2) ::close(_fd); }
void intext::error(int code)
    { _eof = true; throw esyserr(code, file_name); }


void intext::doopen()
{
    _fd = ::open(file_name.c_str(), O_RDONLY | O_LARGEFILE);
    if (_fd < 0)
        error(errno);
    bufsize = bufhead = buftail = 0;
}


void intext::doread()
{
    filebuf.resize(intext::BUF_SIZE);
    buffer = (char*)filebuf.data();
    int result = ::read(_fd, buffer, intext::BUF_SIZE);
    if (result < 0)
        error(errno);
    buftail = 0;
    bufsize = bufhead = result;
    _eof = result == 0;
}


bool intext::empty() const
{
    if (_eof)
        return true;
    if (_fd < 0)
        ((intext*)this)->doopen();
    if (buftail == bufhead)
        ((intext*)this)->doread();
    return _eof;
}


// --- outtext -------------------------------------------------------------- //


outtext::outtext(Type* rt, const str& fn)
    : buffifo(rt, true), file_name(fn), _fd(-1), _err(false)
{
    filebuf.resize(outtext::BUF_SIZE);
    buffer = (char*)filebuf.data();
    bufsize = outtext::BUF_SIZE;
}

outtext::~outtext()
{
    try
        { flush(); }
    catch (exception&)
        { }
    if (_fd > 2)
        ::close(_fd);
}


void outtext::error(int code)
    { _err = true; throw esyserr(code, file_name); }


void outtext::flush()
{
    if (_err)
        return;
    if (bufhead > 0)
    {
        if (_fd < 0)
        {
            _fd = ::open(file_name.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_LARGEFILE, 0644);
            if (_fd < 0)
                error(errno);
        }
        int ret = ::write(_fd, buffer, bufhead);
        if (ret < 0)
            error(errno);
        bufhead = 0;
    }
}


// --- stdfile ------------------------------------------------------------- //


stdfile::stdfile(int infd, int outfd)
    : intext(NULL, "<std>"), _ofd(outfd)
{
    _fd = infd;
    if (infd == -1)
        _eof = true;

    pincrement(&refcount);  // prevent auto pointers from freeing this object,
                            // as it is supposed to be static
#ifdef DEBUG
    object::allocated--;    // compensate static objects
#endif
}


stdfile::~stdfile()
    { pdecrement(&refcount); }

memint stdfile::enq_chars(const char* p, memint count)
    { return ::write(_ofd, p, count); }


// NOTE: these objects depend on the str class, which has static initialization,
// so the best thing is to leave these in the same module as str to ensure
// proper order of initialization.
stdfile sio(STDIN_FILENO, STDOUT_FILENO);
stdfile serr(-1, STDERR_FILENO);

