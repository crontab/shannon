
#include <iostream>
#include <set>

#include "variant.h"
#include "symbols.h"
#include "source.h"

int main()
{
    variant v;
    Parser parser(new InFile("x"));
    List<Symbol> list;
    std::cout << sizeof(std::set<variant>) << std::endl;
}
