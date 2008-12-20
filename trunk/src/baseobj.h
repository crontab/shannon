#ifndef __BASEOBJ_H
#define __BASEOBJ_H


#include "port.h"
#include "str.h"
#include "contain.h"
#include "except.h"


class Base
{
public:
    string name;

    Base(): name()                          { }
    Base(const string& iName): name(iName)  { }
    virtual ~Base();

    static int objCount;
    void* operator new(size_t size);
    void operator delete(void* p);
};


template<class T>
class Auto
{
public:
    T* obj;
    Auto()                          { }
//    Auto(T* iObj): obj(iObj)        { }
    ~Auto()                         { delete obj; }
    operator T*() const             { return obj; }
};


typedef Auto<Base> BasePtr;


class baselistimpl: protected Array<BasePtr>
{
public:
    baselistimpl();
    baselistimpl(const baselistimpl&);
    ~baselistimpl();
    void operator= (const baselistimpl&);

    int size() const                { return Array<BasePtr>::size(); }
    int empty() const               { return Array<BasePtr>::empty(); }
    Base* operator[] (int i) const  { return Array<BasePtr>::operator[] (i); }

    void insert(int, Base*);
    void add(Base*);
    void remove(int);
    void erase(int);
    void clear();

    int find(const string&) const;
    int compare(int, const string&) const;
};


template<class T>
class BaseList: public baselistimpl
{
    typedef T* Tptr;
public:
    T* operator[] (int i) const    { return Tptr(baselistimpl::operator[] (i)); }
    void insert(int i, Base* obj)  { baselistimpl::insert(i, obj); }
    void add(Base* obj)            { baselistimpl::add(obj); }
};



#endif
