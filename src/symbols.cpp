
#include "symbols.h"


Symbol::Symbol(Type* rt, const str& name): object(rt), name(name)  { }
Symbol::Symbol(Type* rt, const char* name): object(rt), name(name)  { }

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
void _PtrList::clear()  { impl.clear(); }


mem _PtrList::add(void* p)
{
    impl.push_back(p);
    return size() - 1;
}


_List::_List()              { }
_List::~_List()             { clear(); }
mem _List::add(object* o)   { return _PtrList::add(grab(o)); }


void _List::clear()
{
    mem i = size();
    while (i--)
        release(operator[](i));
    _PtrList::clear();
}



