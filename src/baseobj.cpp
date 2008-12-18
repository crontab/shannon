
#include "str.h"
#include "baseobj.h"


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

