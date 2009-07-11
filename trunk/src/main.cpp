
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>


#include "runtime.h"
#include "source.h"
#include "symbols.h"


class State;

class Base: public Symbol
{
public:
    enum BaseId { VARIABLE, DEFINITION };

    BaseId const baseId;

    Base(BaseId);
    Base(const str&, BaseId);

    bool isVariable() const   { return baseId == VARIABLE; }
    bool isDefinition() const { return baseId == DEFINITION; }
};

typedef Base* PBase;


class Type: public object
{
protected:
public:
    enum TypeId { VOID, BOOL, CHAR, INT, REAL, STR, RANGE,
        ARRAY, DICT, TINYSET, CHARSET, SET, FIFO, STATE };

    TypeId const typeId;
    
    Type(TypeId);
    ~Type();
};

typedef Type* PType;


class Scope: SymbolTable<Base>
{
protected:
    Scope* const outer;
    PtrList<State> uses;

public:
    Scope(Scope* _outer);
    ~Scope();
    Base* deepFind(const str&) const;
};


class Definition: public Base
{
public:
    Definition(const str& _name);
};


class State: public Definition, public Scope
{
public:
    State* const parent;
    // level: 0 means module (static state), 1+ means function. Refs to outer
    // states are passed as arguments.
    int const level;
    
    State(const str& _name, State* _parent);
};


class Module: public State
{
public:
    Module(const str& _name): State(_name, NULL)  { }
};


// --- BASIC LANGUAGE OBJECTS ---------------------------------------------- //


Base::Base(BaseId _id): Symbol(null_str), baseId(_id)  { }
Base::Base(const str& _name, BaseId _id): Symbol(_name), baseId(_id)  { }


// --- Type ---------------------------------------------------------------- //


Type::Type(TypeId _t): typeId(_t) { }
Type::~Type() { }


// --- Scope --------------------------------------------------------------- //


Scope::Scope(Scope* _outer): outer(_outer)  { }
Scope::~Scope()  { }


Base* Scope::deepFind(const str& ident) const
{
    Base* b = find(ident);
    if (b != NULL)
        return b;
    for (int i = uses.size() - 1; i >= 0; i--)
    {
        b = uses[i]->find(ident);
        if (b != NULL)
            return b;
    }
    if (outer != NULL)
        return outer->deepFind(ident);
    return NULL;
}


// --- Definition ---------------------------------------------------------- //


Definition::Definition(const str& _name)
    : Base(_name, DEFINITION)  { }


// --- State --------------------------------------------------------------- //


State::State(const str& _name, State* _parent)
  : Definition(_name), Scope(_parent), parent(_parent),
    level(_parent == NULL ? 0 : _parent->level + 1) { }



// --- tests --------------------------------------------------------------- //


int main()
{
    {
        variant v;
        Parser parser("x", new in_text("x"));
        List<Symbol> list;
        fifo f(true);
        
        Scope s(NULL);

        fout << sizeof(object) << endl;

        fout << "Hello, world" << endl;
    }
#ifdef DEBUG
    assert(object::alloc == 0);
#endif
}

