
#include "symbols.h"


Symbol::Symbol(const str& name): name(name)  { }
Symbol::Symbol(const char* name): name(name)  { }

EDuplicate::EDuplicate(const str& symbol) throw()
    : emessage("Duplicate identifier: " + symbol)  { }


_SymbolTable::_SymbolTable()  { }
_SymbolTable::~_SymbolTable()  { }


void _SymbolTable::addUnique(Symbol* o)
{
    std::pair<Impl::iterator, bool> result = impl.insert(Impl::value_type(o->getName(), o));
    if (!result.second)
        throw EDuplicate(o->getName());
}


Symbol* _SymbolTable::find(const str& name) const
{
    Impl::const_iterator i = impl.find(name);
    if (i == impl.end())
        return NULL;
    return i->second;
}


_PtrList::_PtrList()  { }
_PtrList::~_PtrList()  { }


mem _PtrList::add(void* p)
{
    impl.push_back(p);
    return size() - 1;
}


mem _List::add(object* o)   { return _PtrList::add(grab(o)); }


_List::~_List()
{
    for(mem i = 0; i < size(); i++)
        release(operator[](i));
}

