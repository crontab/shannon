
#include "symbols.h"


Symbol::Symbol(const str& name): name(name)  { }
Symbol::Symbol(const char* name): name(name)  { }

EDuplicate::EDuplicate(const str& symbol) throw()
    : emessage("Duplicate identifier: " + symbol)  { }


_SymbolTable::_SymbolTable()  { }
_SymbolTable::~_SymbolTable()  { }


void _SymbolTable::addUnique(Symbol* o)
{
    std::pair<Impl::iterator, bool> result = impl.insert(Impl::value_type(o->name, o));
    if (!result.second)
        throw EDuplicate(o->name);
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


int _PtrList::push_back(void* p)
{
    impl.push_back(p);
    return size() - 1;
}


_List::~_List()
{
    for(int i = 0; i < size(); i++)
        release(operator[](i));
}




