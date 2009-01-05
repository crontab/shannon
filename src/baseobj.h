#ifndef __BASEOBJ_H
#define __BASEOBJ_H


#include "port.h"
#include "common.h"


class Base: noncopyable
{
public:
    Base()  { }
    virtual ~Base();

    static int objCount;
    static void* operator new(size_t size);
    static void operator delete(void* p);
};


class BaseNamed: public Base
{
public:
    const string name;

    BaseNamed()
            : Base(), name()  { }
    BaseNamed(const string& iName)
            : Base(), name(iName)  { }
    void setNamePleaseThisIsWrongIKnow(const string& iName)
            { *(string*)&name = iName; }
};


class basetblimpl: protected PodArray<BaseNamed*>
{
public:
    basetblimpl();
    basetblimpl(const basetblimpl&);
    ~basetblimpl();
    void operator= (const basetblimpl&);

    int size() const                { return PodArray<BaseNamed*>::size(); }
    int empty() const               { return PodArray<BaseNamed*>::empty(); }
    Base* operator[] (int i) const  { return PodArray<BaseNamed*>::operator[] (i); }

    void insert(int, BaseNamed*);
    void add(BaseNamed*);
    void addUnique(BaseNamed* obj) throw(EDuplicate);
    void erase(int);
    void clear();

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
    void insert(int i, T* obj)     { basetblimpl::insert(i, obj); }
    void add(T* obj)               { basetblimpl::add(obj); }
    void addUnique(T* obj) throw(EDuplicate)  { basetblimpl::addUnique(obj); }
    T* find(const string& s) const { return Tptr(basetblimpl::find(s)); }
    bool _isClone(const BaseTable& t) const
            { return data == t.data; }
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


//
// String-to-int map
//

class StringInfo: public BaseNamed
{
public:
    const int id;
    StringInfo(const string& str, int id):
        BaseNamed(str), id(id)  { }
};


class StringTable: public BaseTable<StringInfo>
{
public:
    BaseList<StringInfo> list;
    string addUnique(const string& str);
};

#endif
