#ifndef __BASEOBJ_H
#define __BASEOBJ_H


#include <map>


#ifndef __PORT_H
#include "port.h"
#endif

#ifndef __STR_H
#include "str.h"
#endif

#ifndef __ARRAY_H
#include "array.h"
#endif


// ------------------------------------------------------------------------ //
// --- EXCEPTIONS --------------------------------------------------------- //
// ------------------------------------------------------------------------ //

class Exception
{
public:
    virtual ~Exception() throw();
    virtual string what() const throw() = 0;
};


class EMessage: public Exception
{
    string message;
public:
    EMessage(const string& imessage): Exception(), message(imessage)  { }
    virtual ~EMessage() throw();
    virtual string what() const throw();
};


class EInternal: public EMessage
{
public:
    EInternal(int code);
    EInternal(int code, string const& hint);
    virtual ~EInternal() throw();
};


class EDuplicate: public Exception
{
    string entry;
public:
    EDuplicate(const string& ientry);
    virtual ~EDuplicate() throw();
    virtual string what() const throw();
    const string& getEntry() const throw() { return entry; }
};


class ENotFound: public Exception
{
    string entry;
public:
    ENotFound(const string& ientry);
    virtual ~ENotFound() throw();
    virtual string what() const throw();
    const string& getEntry() const throw() { return entry; }
};


class ESysError: public Exception
{
    int code;
public:
    ESysError(int icode): Exception(), code(icode)  { }
    virtual ~ESysError() throw();
    virtual string what() const throw();
};


// ------------------------------------------------------------------------ //
// --- BASE OBJECTS AND CONTAINERS ---------------------------------------- //
// ------------------------------------------------------------------------ //

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


template <class T, bool own>
class Container: public Array<ptr>
{
public:
    Container(): Array<ptr>()        { }
    ~Container()                     { clear(); }
    void add(T* o)                   { Array<ptr>::add(o); }
    T* top() const                   { return (T*)Array<ptr>::top(); }
    T* operator[] (int index) const  { return (T*)Array<ptr>::operator[](index); }
    void pop()                       { if (own) delete top(); Array<ptr>::pop(); }
    void clear()
    {
        if (own)
            for (int i = size() - 1; i >= 0; i--)
                delete operator[] (i);
        Array<ptr>::clear();
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
