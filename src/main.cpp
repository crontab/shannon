
#include "common.h"
#include "variant.h"


class namedobj: public object
{
public:
    const str name;
    namedobj(const str& name): name(name)  { }
};


int main()
{
    return 0;
}
