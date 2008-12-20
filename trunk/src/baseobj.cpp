
#include "str.h"
#include "baseobj.h"
#include "bsearch.h"


int Base::objCount;


Base::~Base()
{
}

void* Base::operator new(size_t size)  { objCount++; return memalloc(size); }
void Base::operator delete(void* p)    { objCount--; memfree(p); }


baselistimpl::baselistimpl()
        : Array<BasePtr>()  { }

baselistimpl::baselistimpl(const baselistimpl& a)
        : Array<BasePtr>(a)  { }

baselistimpl::~baselistimpl()
        { }

void baselistimpl::operator= (const baselistimpl& a)
        { Array<BasePtr>::operator=(a); }

void baselistimpl::insert(int index, Base* obj)
        { Array<BasePtr>::ins(index).obj = obj; }

void baselistimpl::add(Base* obj)
        { Array<BasePtr>::add().obj = obj; }

void baselistimpl::remove(int index)
        { Array<BasePtr>::del(index); }

void baselistimpl::erase(int index)
        { PodArray<BasePtr>::del(index); }

void baselistimpl::clear()
        { Array<BasePtr>::clear(); }

int baselistimpl::find(const string& key) const
{
    int index;
    if (bsearch<baselistimpl, string>(*this, key, size(), index))
        return index;
    return -1;
}

int baselistimpl::compare(int index, const string& key) const
{
    return _at(index).obj->name.compare(key);
}


