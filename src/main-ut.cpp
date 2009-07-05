
// We rely on assert(), so make sure it is turned on for this program
#ifdef NDEBUG
#  undef NDEBUG
#endif

#include <assert.h>

#include <iostream>

#include "variant.h"

using namespace std;

#define assert_throw(e,...) { bool __t = false; try { __VA_ARGS__; } catch(e) { __t = true; } assert(__t); }
#define assert_nothrow(...) { bool __t = false; try { __VA_ARGS__; } catch(...) { __t = true; } assert(!__t); }



int main()
{
    {
        variant v1 = none;
        variant v2 = new_tuple();
        variant v3 = "abc";
        variant v4 = 1;
        variant v5 = 1.1;
        variant v6 = new_dict();
        variant v7 = new_set();
        variant v8 = v3;

        v7 = new_dict();
        v6 = new_tuple();
        v5 = none;
        v4 = new_set();
        v3 = "def";
        v2 = 2;
        v1 = 2.2;
        v8 = v3;

        v2.as_signed<int>();
        v3.cat("ghi");
        
        v4.add("abc");
        v4.ins("sed");
        v4.del("sed");

        v6.add(2);
        v6.add("xyz");
        v6.ins(2, "ert");
        v6.ins("qwe");
        v6.del(2);

        v7.put("one", 1);
        v7.put("two", "TWO");
        v7.put("three", 3.3);
        v7.put("four", true);
        v7.put("five", new_dict());
        v7.del("one");
        v7.put("two", none);

        variant v7i = v7.get("some");

        cout << "v1: " << v1 << endl;
        cout << "v2: " << v2 << endl;
        cout << "v3: " << v3 << endl;
        cout << "v4: " << v4 << endl;
        cout << "v5: " << v5 << endl;
        cout << "v6: " << v6 << endl;
        cout << "v7: " << v7 << endl;
        cout << "v7i: " << v7i << endl;
        cout << "v8: " << v8 << endl;
        cout << "substr: " << v3.sub(2) << endl;
        cout << "substr: " << v3.sub(2, 2) << endl;

        const dict& d = v7.as_dict();
        dict::iterator i = d.begin();
        while (i != d.end())
        {
            cout << "elem: " << i->first << ": " << i->second << endl;
            i++;
        }
    }


#ifdef RT_DEBUG
    cout << "object::alloc: " << object::alloc << endl;
#endif
    return 0;
}
