#ifndef __SYMBOLS_H
#define __SYMBOLS_H


#include <map>
#include <vector>

#include "runtime.h"


class Symbol: public object
{
    str name;
public:
    Symbol(const str&);
    Symbol(const char*);
    const str& getName() const { return name; }
    void setName(const str& _name)  { name = _name; }
    void setName(const char* _name)  { name = _name; }
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

    mem add(void*);
    bool empty()             const { return impl.empty(); }
    mem size()               const { return impl.size(); }
    void* operator[] (mem i) const { return impl[i]; }
};


template<class T>
class PtrList: public _PtrList
{
public:
    mem add(T* p)               { return _PtrList::add(p); }
    T* operator[] (mem i) const { return (T*)_PtrList::operator[](i); }
};


class _List: public _PtrList  // responsible for freeing the objects
{
public:
    ~_List();
    mem add(object* o);
    object* operator[] (mem i) const { return (object*)_PtrList::operator[](i); }
};


template<class T>
class List: public _List
{
public:
    mem add(T* p)               { return _List::add(p); }
    T* operator[] (mem i) const { return (T*)_List::operator[](i); }
};


#endif // __SYMBOLS_H
