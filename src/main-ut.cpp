
#define __STDC_LIMIT_MACROS

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>

#include <iostream>
#include <sstream>

#include "common.h"
#include "variant.h"
#include "symbols.h"


using namespace std;


#define fail(e) \
    (printf("%s:%u: test failed `%s'\n", __FILE__, __LINE__, e), exit(200))

#define check(e) \
    { if (!(e)) fail(#e); }

#define check_throw(e,...) \
    { bool chk_throw = false; try { __VA_ARGS__; } catch(e&) { chk_throw = true; } check(chk_throw); }

#define check_nothrow(...) \
    { try { __VA_ARGS__; } catch(...) { fail("exception thrown"); } }


class test_obj: public object
{
protected:
    virtual object* clone() const { fail("Can't clone test object"); }
public:
    void dump(std::ostream& s) const
        { s << "test_obj"; }
    test_obj()  { }
};


void test_variant()
{
    int save_alloc = object::alloc;
    {
        variant v1 = null;              check(v1.is_null());
        variant v2 = 0;                 check(v2.is_int());     check(v2.as_int() == 0);
        variant v3 = 1;                 check(v3.is_int());     check(v3.as_int() == 1);
        variant v4 = INT64_MAX;         check(v4.is_int());     check(v4.as_int() == INT64_MAX);
        variant v5 = INT64_MIN;         check(v5.is_int());     check(v5.as_int() == INT64_MIN);
        variant v6 = 1.1;               check(v6.is_real());    check(v6.as_real() == 1.1);
        variant v7 = false;             check(v7.is_bool());    check(!v7.as_bool());
        variant v8 = true;              check(v8.is_bool());    check(v8.as_bool());
        variant v12 = 'x';              check(v12.is_char());   check(v12.as_char() == 'x');
        variant v9 = "";                check(v9.is_str());     check(v9.as_str().empty());
        variant v10 = "abc";            check(v10.is_str());    check(v10.as_str() == "abc");
        string s1 = "def";
        variant vst = s1;               check(vst.is_str());    check(vst.as_str() == s1);
        variant vr = new_range();       check(vr.is_range());   check(vr.is_null_ptr());
        variant vr1 = new_range(1, 5);  check(vr1.is_range());  check(!vr1.is_null_ptr());
        variant vr2(6, 7);              check(vr2.is_range());  check(!vr2.is_null_ptr());
        variant vr3(6, 5);              check(vr3.is_range());  check(vr3.is_null_ptr());
        variant vt = new_tuple();       check(vt.is_tuple());   check(vt.is_null_ptr());
        variant vd = new_dict();        check(vd.is_dict());    check(vd.is_null_ptr());
        variant vs = new_set();         check(vs.is_set());     check(vs.is_null_ptr());
        object* o = new test_obj();
        variant vo = o;                 check(vo.is_object());  check(vo.as_object() == o);

        check_throw(evarianttype, v1.as_int());
        check_throw(evarianttype, v1.as_real());
        check_throw(evarianttype, v1.as_bool());
        check_throw(evarianttype, v1.as_char());
        check_throw(evarianttype, v1.as_str());
        check_throw(evarianttype, v1.as_range());
        check_throw(evarianttype, v1.as_tuple());
        check_throw(evarianttype, v1.as_dict());
        check_throw(evarianttype, v1.as_set());
        check_throw(evarianttype, v1.as_object());
        
        check(v1.to_string() == "null");
        check(v2.to_string() == "0");
        check(v3.to_string() == "1");
        check(v4.to_string() == "9223372036854775807");
        check(v5.to_string() == "-9223372036854775808");
        check(v6.to_string() == "1.1");
        check(v7.to_string() == "false");
        check(v8.to_string() == "true");
        check(v9.to_string() == "\"\"");
        check(v10.to_string() == "\"abc\"");
        check(vst.to_string() == "\"def\"");
        check(v12.to_string() == "'x'");
        check(vr.to_string() == "[]");
        check(vr1.to_string() == "[1..5]");
        check(vr2.to_string() == "[6..7]");
        check(vr3.to_string() == "[]");
        check(vt.to_string() == "[]");
        check(vd.to_string() == "[]");
        check(vs.to_string() == "[]");
        check(vo.to_string() == "[test_obj]");

        check_throw(evarianttype, v1 < v2);
        check(v2 < v3);
        
        variant v;
        check(v == null);
        v = 0;                 check(v.as_int() == 0);              check(v == 0);
        check(v != null);      check(v != true);                    check(v != "abc");
        v = 1;                 check(v.as_int() == 1);              check(v == 1);
        v = INT64_MAX;         check(v.as_int() == INT64_MAX);      check(v == INT64_MAX);
        v = INT64_MIN;         check(v.as_int() == INT64_MIN);      check(v == INT64_MIN);
        v = 1.1;               check(v.as_real() == 1.1);           check(v == 1.1);
        v = false;             check(!v.as_bool());                 check(v == false);
        v = true;              check(v.as_bool());                  check(v == true);
        v = 'x';               check(v.as_char() == 'x');           check(v == 'x');    check(v != 'z');
        v = "";                check(v.as_str().empty());           check(v == "");
        v = "abc";             check(v.as_str() == "abc");          check(v == "abc");
        v = s1;                check(v.as_str() == s1);             check(v == s1);
        v = new_range();       check(v.is_null_ptr());              check(v.left() == 0 && v.right() == -1);
        v = new_tuple();       check(v.is_null_ptr());
        v = new_dict();        check(v.is_null_ptr());
        v = new_set();         check(v.is_null_ptr());
        v = o;                 check(v.as_object() == o);
        check(v != null);
        check(!v.is_null());
        check(!v.is_int());
        check(!v.is_real());
        check(!v.is_bool());
        check(!v.is_int());
        check(!v.is_char());
        check(!v.is_str());
        check(!v.is_range());
        check(!v.is_tuple());
        check(!v.is_dict());
        check(!v.is_set());
        v = null;
        check(!v.is_object());
        check(v == null);

        // str
        vst = "";
        check(vst.empty()); check(vst.size() == 0);
        vst.append("abc");
        vst.append("");
        vst.append(s1);
        vst.append("ghi");
        vst.append(variant("jkl"));
        check(vst.as_str() == "abcdefghijkl");
        check(vst.substr(5, 3) == "fgh");
        check(vst.substr(5) == "fghijkl");
        vst.erase(1);
        check(vst.as_str() == "acdefghijkl");
        vst.erase(3, 4);
        check(vst.as_str() == "acdijkl");
        check(vst.getch(2) == 'd');
        vst.resize(10, '-');
        check(vst.as_str() == "acdijkl---");
        check(vst.size() == 10);
        vst.resize(5, '-');
        check(vst.as_str() == "acdij");

        // range
        vr = new_range();
        check(vr.empty());
        vr = new_range(-1, 1);
        check(vr.left() == -1 && vr.right() == 1);
        check(vr.has(0));
        check(!vr.has(-2));
        vr = new_range(5, 2);
        check(vr.left() == 0 && vr.right() == -1);
        check(!vr.has(-2));
        variant vr4 = 0;
        vr4.assign(0, 5);
        check(vr4.left() == 0 && vr4.right() == 5);
        check_throw(evarianttype, vr4.has("abc"));

        // tuple
        check(vt.empty()); check(vt.size() == 0);
        check_throw(evariantindex, vt.insert(1, 0));
        vt.push_back(0);
        vt.push_back("abc");
        vt.insert(vt.size(), 'd');
        vt.push_back(vs);
        vt.insert(1, true);
        check(vt.to_string() == "[0, true, \"abc\", 'd', []]");
        check(!vt.empty()); check(vt.size() == 5);
        check_throw(evariantindex, vt.insert(10, 0));
        check_throw(evariantindex, vt.insert(-1, 0));
        vt.erase(2);
        check(vt.to_string() == "[0, true, 'd', []]");
        check(vt[2] == 'd');
        vt.put(2, "asd");
        check(vt.to_string() == "[0, true, \"asd\", []]");
        check_throw(evariantindex, vt.put(4, 0));
        vt.resize(2);
        check(vt.to_string() == "[0, true]");
        vt.resize(4);
        check(vt.to_string() == "[0, true, null, null]");
        check(vt.size() == 4);

        // dict
        check(vd.empty()); check(vd.size() == 0);
        vd.put("k1", "abc");
        vd.put("k2", true);
        vd.put("k3", new_set());
        check_throw(evarianttype, vd.put(0, 0));
        check(vd.to_string() == "[\"k1\": \"abc\", \"k2\": true, \"k3\": []]");
        check(!vd.empty()); check(vd.size() == 3);
        vd.put("k2", null);
        check(vd.to_string() == "[\"k1\": \"abc\", \"k3\": []]");
        vd.put("kz", null);
        check(vd.to_string() == "[\"k1\": \"abc\", \"k3\": []]");
        stringstream vds;
        vforeach(dict, i, vd)
            vds << ' ' << i->first << ": " << i->second;
        check(vds.str() == " \"k1\": \"abc\" \"k3\": []");
        vd.erase("k2");
        check(vd.to_string() == "[\"k1\": \"abc\", \"k3\": []]");
        vd.erase("k3");
        check(vd.to_string() == "[\"k1\": \"abc\"]");
        check(vd["k1"] == "abc");
        check(vd["k2"] == null);
        check(vd["kz"] == null);
        check(vd.has("k1"));
        check(!vd.has("k2"));

        // dict[int]
        vd = new_dict();
        vd.put(100, 'a');
        // vd.put(10000000000ll, 'b'); TODO: make this work
        check(vd.to_string() == "[100: 'a']");    
        
        // dict[range]
        vd = new_dict();
        vd.put(new_range(0, 4), new_range(0, 1));
        vd.put(new_range(5, 6), "abc");
        check(vd.has(new_range(5, 6)));
        check(!vd.has(new_range()));
        check(!vd.has(new_range(0, 6)));
        
        // set
        check(vs.empty()); check(vs.size() == 0);
        vs.insert(5);
        vs.insert(26);
        vs.insert(127);
        check_throw(evarianttype, vs.insert("abc"));
        check(vs.to_string() == "[5, 26, 127]");
        stringstream vdss;
        vforeach(set, i, vs)
            vdss << ' ' << *i;
        check(vdss.str() == " 5 26 127");
        check(!vs.empty()); check(vs.size() == 3);
        vs.erase(26);
        vs.erase(226);
        check(vs.to_string() == "[5, 127]");
        check(vs.has(5));
        check(vs.has(127));
        check(!vs.has(26));
        
        // various sets
        vs = new_set();
        vs.insert("abc");
        vs.insert("abc");
        vs.insert("def");
        check(vs.to_string() == "[\"abc\", \"def\"]")
        check(vs.has("abc"));

        vs = new_set();
        vs.insert(1.1);
        vs.insert(2.2);
        check(vs.to_string() == "[1.1, 2.2]")
        check(vs.has(1.1));

        vs = new_set();
        vs.insert(null);
        check(vs.to_string() == "[null]")
        check(vs.has(null));
        
        stringstream ss;
        ss << vs;
        check(ss.str() == "[null]");
        
        // ref counting
        vs = new_set();
        vs.insert(0);
        check(vs.as_set().is_unique());
        variant vss = vs;
        check(vss.has(0));
        check(!vs.as_set().is_unique());
        check(!vss.as_set().is_unique());
        check(vs.to_string() == "[0]");
        check(vss.to_string() == "[0]");
        vs.insert(1);
        check(vs.as_set().is_unique());
        check(vss.as_set().is_unique());
        check(vs.to_string() == "[0, 1]");
        check(vss.to_string() == "[0]");
        
        // object
        // vo = vo; // this doesn't work at the moment
        // cout << vo.as_object() << endl;
    
        int save_alloc2 = object::alloc;
        objptr<test_obj> p1 = new test_obj();
        check(object::alloc == save_alloc2 + 1);
        check(p1->is_unique());
        objptr<test_obj> p2 = p1;
        check(!p1->is_unique());
        check(!p2->is_unique());
        check(p1 == p2);
        objptr<test_obj> p3 = new test_obj();
        check(p1 != p3);
        p3 = p3; // see if it doesn't core dump
    }
    check(object::alloc == save_alloc);
}


void test_symbols()
{
    int save_alloc = object::alloc;
    {
        objptr<Symbol> s = new Symbol("sym");
        check(s->name == "sym");
        SymbolTable<Symbol> t;
        check(t.empty());
        objptr<Symbol> s1 = new Symbol("abc");
        check(s1->is_unique());
        t.addUnique(s1);
        check(s1->is_unique());
        objptr<Symbol> s2 = new Symbol("def");
        t.addUnique(s2);
        objptr<Symbol> s3 = new Symbol("abc");
        check_throw(EDuplicate, t.addUnique(s3));
        check(t.find("abc") == s1);
        check(s2 == t.find("def"));
        check(t.find("xyz") == NULL);
        check(!t.empty());

        List<Symbol> l;
        check(l.size() == 0);
        check(l.empty());
        l.push_back(s1);
        check(!s1->is_unique());
        l.push_back(new Symbol("ghi"));
        check(l.size() == 2);
        check(!l.empty());
    }
    check(object::alloc == save_alloc);
}


int main()
{
    check(sizeof(int) != 8);

    try
    {
        test_variant();
        test_symbols();
    }
    catch (exception& e)
    {
        fail(e.what());
    }

    check(object::alloc == 0);

    return 0;
}
