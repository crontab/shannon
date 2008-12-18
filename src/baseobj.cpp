
#include <vector>

#include "str.h"
#include "baseobj.h"


Exception::~Exception() throw() { }


string EMessage::what() const throw() { return message; }
EMessage::~EMessage() throw()  { }


EInternal::EInternal(int code)
    : EMessage("Internal error #" + itostring(code))  {}
EInternal::EInternal(int code, const string& hint)
    : EMessage("Internal error #" + itostring(code) + " (" + hint + ')')  {}
EInternal::~EInternal() throw()  { }


EDuplicate::EDuplicate(const string& ientry)
    : Exception(), entry(ientry) { }
EDuplicate::~EDuplicate() throw()  { }
string EDuplicate::what() const throw() { return "Duplicate entry '" + entry + '\''; }


ENotFound::ENotFound(const string& ientry)
    : Exception(), entry(ientry) { }
ENotFound::~ENotFound() throw()  { }
string ENotFound::what() const throw() { return "Unknown entry '" + entry + '\''; }


ESysError::~ESysError() throw()  { }


string ESysError::what() const throw()
{
    char buf[1024];
    strerror_r(code, buf, sizeof(buf));
    return buf;
}


int Base::objCount;


Base::~Base()
{
    objCount--;
}


Map::Map(): std::map<string, ptr>()  { }


ptr Map::find(const string& key) const throw()
{
    const_iterator i = std::map<string, ptr>::find(key);
    if (i == end())
        return NULL;
    return i->second;
}


ptr Map::get(const string& key) const throw(ENotFound)
{
    const_iterator i = std::map<string, ptr>::find(key);
    if (i == end())
        throw ENotFound(key);
    return i->second;
}


void Map::add(const string& key, ptr value) throw(EDuplicate)
{
    std::pair<std::map<string, ptr>::iterator, bool> ret = 
        std::map<string, ptr>::insert(std::pair<string, ptr>(key, value));
    if (!ret.second)
        throw EDuplicate(key);
}


void Map::remove(const string& key) throw(EInternal)
{
    size_t result = erase(key);
    if (result == 0)
        throw EInternal(1, '\'' + key + "' doesn't exist");
}


void Map::clear()
{
    std::map<string, ptr>::clear();
}

