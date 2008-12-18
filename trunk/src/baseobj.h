#ifndef __BASEOBJ_H
#define __BASEOBJ_H


#include <map>

#include "port.h"
#include "str.h"
#include "contain.h"
#include "except.h"


class Base
{
protected:
    string name;
public:
    Base(): name()                          { objCount++; }
    Base(const string& iName): name(iName)  { objCount++; }
    virtual ~Base();
    string getName()                        { return name; }
    static int objCount;
};


template <class T>
class Container: public PodArray<T*>
{
private:
    Container(const Container&);
    void operator= (const Container&);
public:
    Container(): PodArray<T*>()      { }
    ~Container()                     { clear(); }
    void pop()                       { delete PodArray<T*>::top(); PodArray<T*>::pop(); }
    void dequeue()                   { delete PodArray<T*>::operator[](0); PodArray<T*>::dequeue(); }
    void clear()
    {
        for (int i = PodArray<T*>::size() - 1; i >= 0; i--)
            delete PodArray<T*>::_at(i);
        PodArray<T*>::clear();
    }
};


class Map: protected std::map<string, ptr>
{
public:
    Map();
    ~Map()  { clear(); }
    void clear();
    ptr find(const string&) const throw();
    ptr get(const string&) const throw(ENotFound);
    void add(const string&, ptr) throw(EDuplicate);
    void remove(const string&) throw(EInternal);
};


template<class T, bool own>
class HashTable: public Map
{
public:
    HashTable(): Map()                        { }
    ~HashTable()                              { clear(); }
    T* find(const string& key) const throw()  { return (T*)Map::find(key); }
    T* get(const string& key) const throw(ENotFound)
                                              { return (T*)Map::get(key); }
    void add(const string& key, T* o) throw(EDuplicate)
                                              { Map::add(key, o); }
    void remove(const string& key) throw(EInternal)
    {
        if (own)
        {
            iterator i = std::map<string, ptr>::find(key);
            if (i == end())
                throw EInternal(1, '\'' + key + "' doesn't exist");
            delete (T*)(i->second);
            erase(i);
        }
        else
            Map::remove(key);
    }
    void clear()
    {
        if (own)
            for (const_iterator i = begin(); i != end(); i++)
                delete (T*)(i->second);
        Map::clear();
    }
};


template<bool own>
class BaseHash: public HashTable<Base, own>
{
public:
    BaseHash()  { }
    ~BaseHash()  { }
    void add(Base* o) throw(EDuplicate)  { HashTable<Base, own>::add(o->getName(), o); }
};


#endif
