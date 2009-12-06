

#include "common.h"
#include "runtime.h"


// --- set ----------------------------------------------------------------- //


template <class T>
    struct comparator
        { int operator() (const T& a, const T& b) { return int(a - b); } };

template <>
    struct comparator<str>
        { int operator() (const str& a, const str& b) { return a.compare(b); } };

template <>
    struct comparator<const char*>
        { int operator() (const char* a, const char* b) { return strcmp(a, b); } };


template <class T, class Comp = comparator<T> >
class set: protected vector<T>
{
protected:
    typedef vector<T> parent;
    typedef T* Tptr;
    typedef Tptr& Tref;
    
    class cont: public parent::cont
    {
        typedef typename parent::cont parent;
        container* new_(memint cap, memint siz) { return new(cap) cont(cap, siz); }
        container* null_obj()                   { return &set::null; }
        int compare(memint index, void* key)
        {
            static Comp comp;
            return comp(*container::data<T>(index), *Tptr(key));
        }
    public:
        cont(): parent()  { }
        cont(memint cap, memint siz): parent(cap, siz)  { }
    };

    static cont null;

public:
    set(): parent(&null)            { }
    set(const set& s): parent(s)    { }

    bool empty() const              { return parent::empty(); }
    memint size() const             { return parent::size(); }
    const T& operator[] (memint index) const
        { return parent::operator[] (index); }

    bool find(const T& item) const
    {
        memint index;
        return contptr::bsearch<T>(item, index);
    }

    bool insert(const T& item)
    {
        memint index;
        if (!contptr::bsearch<T>(item, index))
        {
            parent::insert(index, item);
            return true;
        }
        else
            return false;
    }

    void erase(const T& item)
    {
        memint index;
        if (contptr::bsearch<T>(item, index))
            parent::erase(index);
    }
};


template <class T, class Comp>
    typename set<T, Comp>::cont set<T, Comp>::null;



// --- dict ---------------------------------------------------------------- //


template <class Tkey, class Tval>
struct dictitem: public object
{
    const Tkey key;
    Tval val;
    dictitem(const Tkey& _key, const Tval& _val)
        : key(_key), val(_val) { }
    bool empty() const { return false; }
};


template <class Tkey, class Tval, class Comp = comparator<Tkey> >
class dict: protected vector<objptr<dictitem<Tkey, Tval> > >
{
protected:
    typedef dictitem<Tkey, Tval> Titem;
    typedef objptr<Titem> T;
    typedef vector<T> parent;
    typedef T* Tptr;
    typedef Tptr& Tref;
    enum { Tsize = sizeof(T) };

    class cont: public parent::cont
    {
        typedef typename parent::cont parent;
        container* new_(memint cap, memint siz) { return new(cap) cont(cap, siz); }
        container* null_obj()                   { return &dict::null; }
        int compare(memint index, void* key)
        {
            static Comp comp;
            return comp((*container::data<T>(index))->key, *(Tkey*)key);
        }
    public:
        cont(): parent()  { }
        cont(memint cap, memint siz): parent(cap, siz)  { }
    };

    static cont null;

public:
    dict(): parent(&null)           { }
    dict(const dict& s): parent(s)  { }
    
    bool empty() const              { return parent::empty(); }
    memint size() const             { return parent::size(); }

    typedef Titem item_type;
    const item_type& operator[] (memint index) const
        { return *parent::operator[] (index); }

    const Tval* find(const Tkey& key) const
    {
        memint index;
        if (contptr::bsearch<Tkey>(key, index))
            return &parent::operator[] (index)->val;
        else
            return NULL;
    }

    void replace(const Tkey& key, const Tval& val)
    {
        memint index;
        if (!contptr::bsearch<Tkey>(key, index))
            parent::insert(index, new Titem(key, val));
        else if (parent::unique())
            (parent::operator[] (index))->val = val;
        else
            parent::replace(index, new Titem(key, val));
    }

    void erase(const Tkey& key)
    {
        memint index;
        if (contptr::bsearch<Tkey>(key, index))
            parent::erase(index);
    }
};


template <class Tkey, class Tval, class Comp>
    typename dict<Tkey, Tval, Comp>::cont dict<Tkey, Tval, Comp>::null;



// --- tests --------------------------------------------------------------- //


void ut_fail(unsigned line, const char* e)
{
    fprintf(stderr, "%s:%u: test failed `%s'\n", __FILE__, line, e);
    exit(200);
}

#define fail(e)     ut_fail(__LINE__, e)
#define check(e)    { if (!(e)) fail(#e); }


void test_set()
{
    set<str> s1;
    check(s1.insert("GHI"));
    check(s1.insert("ABC"));
    check(s1.insert("DEF"));
    check(!s1.insert("ABC"));
    check(s1.size() == 3);
    check(s1[0] == "ABC");
    check(s1[1] == "DEF");
    check(s1[2] == "GHI");
    s1.erase("DEF");
    check(s1.size() == 2);
    check(s1[0] == "ABC");
    check(s1[1] == "GHI");
}


void test_dict()
{
    dict<str, int> d1;
    d1.replace("three", 3);
    d1.replace("one", 1);
    d1.replace("two", 2);
    check(d1.size() == 3);
    check(d1[0].key == "one");
    check(d1[1].key == "three");
    check(d1[2].key == "two");
    dict<str, int> d2 = d1;
    d1.erase("three");
    check(d1.size() == 2);
    check(d1[0].key == "one");
    check(d1[1].key == "two");
    check(*d1.find("one") == 1);
    check(d1.find("three") == NULL);
    check(d2.size() == 3);
}


int main()
{
    printf("%lu %lu\n", sizeof(object), sizeof(container));

    test_set();
    test_dict();

    if (object::allocated != 0)
    {
        fprintf(stderr, "object::allocated: %d\n", object::allocated);
        _fatal(0xff01);
    }

    return 0;
}
