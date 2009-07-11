#ifndef __SYMBOLS_H
#define __SYMBOLS_H


#include <map>
#include <vector>

#include "runtime.h"


class Symbol: public object
{
public:
    str const name;
    Symbol(const str& name);
    Symbol(const char* name);
};


struct EDuplicate: public emessage
    { EDuplicate(const str& symbol) throw(); };


class _SymbolTable: public noncopyable
{
    typedef std::map<str, Symbol*> Impl;
    Impl impl;
public:
    _SymbolTable();
    ~_SymbolTable();

    bool empty()             const { return impl.empty(); }
    void addUnique(Symbol*);
    Symbol* find(const str&) const;
};


template<class T>
class SymbolTable: public _SymbolTable
{
public:
    void addUnique(T* o)           { _SymbolTable::addUnique(o); }
    T* find(const str& name) const { return (T*)_SymbolTable::find(name); }
};


class _PtrList: public noncopyable
{
    typedef std::vector<void*> Impl;
    Impl impl;
public:
    _PtrList();
    ~_PtrList();

    int push_back(void*);
    bool empty()             const { return impl.empty(); }
    int size()               const { return impl.size(); }
    void* operator[] (int i) const { return impl[i]; }
};


template<class T>
class PtrList: public _PtrList
{
public:
    int push_back(T* p)           { return _PtrList::push_back(p); }
    T* operator[] (int i)   const { return (T*)_PtrList::operator[](i); }
};


class _List: public _PtrList  // responsible for freeing the objects
{
public:
    ~_List();
    int push_back(object* o)      { return _PtrList::push_back(grab(o)); }
    object* operator[] (int i) const { return (object*)_PtrList::operator[](i); }
};


template<class T>
class List: public _List
{
public:
    int push_back(T* p)           { return _List::push_back(p); }
    T* operator[] (int i)   const { return (T*)_List::operator[](i); }
};


#endif // __SYMBOLS_H
