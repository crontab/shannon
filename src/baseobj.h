#ifndef __BASEOBJ_H
#define __BASEOBJ_H


#include "port.h"
#include "str.h"
#include "contain.h"
#include "except.h"


class Base: noncopyable
{
public:
    const string name;

    Base(): name()                          { }
    Base(const string& iName): name(iName)  { }
    virtual ~Base();

    static int objCount;
    void* operator new(size_t size);
    void operator delete(void* p);
};


class basetblimpl: protected PodArray<Base*>
{
public:
    basetblimpl();
    basetblimpl(const basetblimpl&);
    ~basetblimpl();
    void operator= (const basetblimpl&);

    int size() const                { return PodArray<Base*>::size(); }
    int empty() const               { return PodArray<Base*>::empty(); }
    Base* operator[] (int i) const  { return PodArray<Base*>::operator[] (i); }

    void insert(int, Base*);
    void add(Base*);
    void addUnique(Base* obj) throw(EDuplicate);
    void erase(int);

    bool search(const string&, int*) const;
    Base* find(const string&) const;
    int  compare(int, const string&) const;
};


//
// BaseTable: sorted searchable collection of Base*, not owned
//

template<class T>
class BaseTable: public basetblimpl
{
    typedef T* Tptr;
public:
    T* operator[] (int i) const    { return Tptr(basetblimpl::operator[] (i)); }
    void insert(int i, Base* obj)  { basetblimpl::insert(i, obj); }
    void add(T* obj)               { basetblimpl::add(obj); }
    void addUnique(T* obj) throw(EDuplicate)  { basetblimpl::addUnique(obj); }
    T* find(const string& s) const { return Tptr(basetblimpl::find(s)); }
};



template<class T>
class Auto
{
public:
    T* obj;
    Auto(): obj(NULL)                { }
    Auto(Auto<T>& a): obj(a.obj)     { a.release(); }
    Auto(T* iObj): obj(iObj)         { }
    ~Auto()                          { delete obj; }
    void operator= (Auto<T>& a)      { obj = a.obj; a.release(); }
    void operator= (T* p)            { obj = p; }
    void release()                   { obj = NULL; }
    operator T*() const              { return obj; }
    bool operator== (T* p) const     { return obj == p; }
    bool operator!= (T* p) const     { return obj != p; }
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
};


//
// BaseList: collection of Base*, owned
//

template<class T>
class BaseList: public baselistimpl
{
    typedef T* Tptr;
public:
    T* operator[] (int i) const    { return Tptr(baselistimpl::operator[] (i)); }
    void insert(int i, Base* obj)  { baselistimpl::insert(i, obj); }
    void add(T* obj)               { baselistimpl::add(obj); }
};


#endif
