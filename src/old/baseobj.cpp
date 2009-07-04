
#include "common.h"
#include "baseobj.h"
#include "bsearch.h"


int Base::objCount;


Base::~Base()
{
}


void* Base::operator new(size_t size)  { objCount++; return memalloc(size); }
void Base::operator delete(void* p)    { objCount--; memfree(p); }



basetblimpl::basetblimpl()
        : PodArray<BaseNamed*>()  { }

basetblimpl::basetblimpl(const basetblimpl& a)
        : PodArray<BaseNamed*>(a)  { }

basetblimpl::~basetblimpl()
        { }

void basetblimpl::operator= (const basetblimpl& a)
        { PodArray<BaseNamed*>::operator=(a); }

void basetblimpl::insert(int index, BaseNamed* obj)
        { PodArray<BaseNamed*>::ins(index, obj); }

void basetblimpl::add(BaseNamed* obj)
        { PodArray<BaseNamed*>::add(obj); }

void basetblimpl::erase(int index)
        { PodArray<BaseNamed*>::del(index); }

void basetblimpl::clear()
        { PodArray<BaseNamed*>::clear(); }

bool basetblimpl::search(const string& key, int* index) const
{
    return bsearch<basetblimpl, string>(*this, key, size(), *index);
}

Base* basetblimpl::find(const string& key) const
{
    int index;
    if (search(key, &index))
        return _at(index);
    return NULL;
}

int basetblimpl::compare(int index, const string& key) const
{
    return _at(index)->name.compare(key);
}

void basetblimpl::addUnique(BaseNamed* obj) throw(EDuplicate)
{
    int index;
    if (search(obj->name, &index))
        throw EDuplicate(obj->name);
    insert(index, obj);
}




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


string StringTable::addUnique(const string& str)
{
    int index;
    if (!search(str, &index))
    {
        StringInfo* si = new StringInfo(str, size());
        insert(index, si);
        list.add(si);
    }
    return operator[] (index)->name;
}

