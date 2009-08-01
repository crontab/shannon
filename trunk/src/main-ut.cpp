
#define __STDC_LIMIT_MACROS

#include "common.h"
#include "runtime.h"
#include "parser.h"
#include "typesys.h"
#include "vm.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>


#define fail(e) \
    (fprintf(stderr, "%s:%u: test failed `%s'\n", __FILE__, __LINE__, e), exit(200))

#define check(e) \
    { if (!(e)) fail(#e); }

#define check_throw(a) \
    { bool chk_throw = false; try { a; } catch(exception&) { chk_throw = true; } check(chk_throw); }


#define XSTR(s) _STR(s)
#define _STR(s) #s

#ifdef SH64
#  define INTEGER_MAX_STR "9223372036854775807"
#  define INTEGER_MAX_STR_PLUS "9223372036854775808"
#  define INTEGER_MIN_STR "-9223372036854775808"
#else
#  define INTEGER_MAX_STR "2147483647"
#  define INTEGER_MAX_STR_PLUS "2147483648"
#  define INTEGER_MIN_STR "-2147483648"
#endif



void test_common()
{
    // make sure the assembly code for atomic ops is sane
    int i = 1;
    check(pincrement(&i) == 2);
    check(pdecrement(&i) == 1);

    // string conversion
    check(to_string(integer(0)) == "0");
    check(to_string(integer(-1)) == "-1");
    check(to_string(INTEGER_MAX) == INTEGER_MAX_STR);
    check(to_string(INTEGER_MIN) == INTEGER_MIN_STR);
    check(to_string(1, 10, 4, '0') == "0001");
    check(to_string(integer(123456789)) == "123456789");
    check(to_string(-123, 10, 7, '0') == "-000123");
    check(to_string(0xabcde, 16, 6) == "0ABCDE");
    
    bool e = true, o = true;
    check(from_string("0", &e, &o) == 0);
    check(!o); check(!e);
    check(from_string(INTEGER_MAX_STR, &e, &o) == INTEGER_MAX);
    check(from_string(INTEGER_MAX_STR_PLUS, &e, &o) == uinteger(INTEGER_MAX) + 1);
    check(from_string("92233720368547758070", &e, &o) == 0); check(o);
    check(from_string("-1", &e, &o) == 0 ); check(e);
    check(from_string("89abcdef", &e, &o, 16) == 0x89abcdef);
    check(from_string("afg", &e, &o, 16) == 0); check(e);
    
    // syserror
    check(str(esyserr(2, "arg").what()) == "No such file or directory (arg)");
    check(str(esyserr(2).what()) == "No such file or directory");
    
    check(remove_filename_path("/usr/bin/true") == "true");
    check(remove_filename_path("usr/bin/true") == "true");
    check(remove_filename_path("/true") == "true");
    check(remove_filename_path("true") == "true");
    check(remove_filename_path("c:\\Windows\\false") == "false");
    check(remove_filename_path("\\Windows\\false") == "false");
    check(remove_filename_path("Windows\\false") == "false");
    check(remove_filename_path("\\false") == "false");
    check(remove_filename_path("false") == "false");

    check(remove_filename_ext("/usr/bin/true.exe") == "/usr/bin/true");
    check(remove_filename_ext("true.exe") == "true");
    check(remove_filename_ext("true") == "true");
}


class test_obj: public object
{
public:
    void dump(fifo_intf& s) const
        { s << "test_obj"; }
    test_obj(): object(NULL)  { }
};


void test_variant()
{
    int save_alloc = object::alloc;
    {
        variant v1 = null;              check(v1.is_null());
        variant v2 = 0;                 check(v2.is(variant::INT));     check(v2.as_int() == 0);
        variant v3 = 1;                 check(v3.as_int() == 1);
        variant v4 = INTEGER_MAX;       check(v4.as_int() == INTEGER_MAX);
        variant v5 = INTEGER_MIN;       check(v5.as_int() == INTEGER_MIN);
        variant v6 = 1.1;               check(v6.as_real() == real(1.1));
        variant v7 = false;             check(!v7.as_bool());
        variant v8 = true;              check(v8.as_bool());
        variant v12 = 'x';              check(v12.as_char() == 'x');
        variant v9 = "";                check(v9.as_str().empty());
        variant v10 = "abc";            check(v10.as_str() == "abc");
        str s1 = "def";
        variant vst = s1;               check(vst.as_str() == s1);
        object* o = new test_obj();
        variant vo = o;                 check(vo.is_obj());  check(vo.as_obj() == o);

        check_throw(v1.as_int());
        check_throw(v1.as_real());
        check_throw(v1.as_bool());
        check_throw(v1.as_char());
        check_throw(v1.as_str());
        check_throw(v1.as_obj());
        
        check(v1.to_string() == "null");
        check(v2.to_string() == "0");
        check(v3.to_string() == "1");
        check(v4.to_string() == INTEGER_MAX_STR);
        check(v5.to_string() == INTEGER_MIN_STR);
        // check(v6.to_string() == "1.1");
        check(v7.to_string() == "false");
        check(v8.to_string() == "true");
        check(v9.to_string() == "''");
        check(v10.to_string() == "'abc'");
        check(vst.to_string() == "'def'");
        check(v12.to_string() == "'x'");
        check(vo.to_string() == "test_obj");

        check(v1 < v2); check(!(v2 < v1));
        check(v2 < v3);
        check(v3 != v6); check(v3 < v6); check(!(v6 < v3));
        
        variant v;
        check(v == null);
        v = 0;                 check(v.as_int() == 0);              check(v == 0);
        check(v != null);      check(v != true);                    check(v != "abc");
        v = 1;                 check(v.as_int() == 1);              check(v == 1);
        v = INTEGER_MAX;       check(v.as_int() == INTEGER_MAX);    check(v == INTEGER_MAX);
        v = INTEGER_MIN;       check(v.as_int() == INTEGER_MIN);    check(v == INTEGER_MIN);
        v = 1.1;               check(v.as_real() == real(1.1));     check(v == 1.1);
        v = false;             check(!v.as_bool());                 check(v == false);
        v = true;              check(v.as_bool());                  check(v == true);
        v = 'x';               check(v.as_char() == 'x');           check(v == 'x');    check(v != 'z');
        v = "";                check(v.as_str().empty());           check(v == "");
        v = "abc";             check(v.as_str() == "abc");          check(v == "abc");
        v = s1;                check(v.as_str() == s1);             check(v == s1);
        v = o;                 check(v.as_obj() == o);
        check(v != null);
        v = null;
        check(!v.is(variant::OBJECT));
        check(v == null);

        variant voa = vo;
        check_throw(voa.unique());
        
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
        objptr<Symbol> s = new Symbol(Symbol::THISVAR, NULL, "sym");
        check(s->name == "sym");
        SymbolTable<Symbol> t;
        check(t.empty());
        objptr<Symbol> s1 = new Symbol(Symbol::THISVAR, NULL, "abc");
        check(s1->is_unique());
        t.addUnique(s1);
        check(s1->is_unique());
        objptr<Symbol> s2 = new Symbol(Symbol::THISVAR, NULL, "def");
        t.addUnique(s2);
        objptr<Symbol> s3 = new Symbol(Symbol::THISVAR, NULL, "abc");
        check_throw(t.addUnique(s3));
        check(t.find("abc") == s1);
        check(s2 == t.find("def"));
        check(t.find("xyz") == NULL);
        check(!t.empty());

        List<Symbol> l;
        check(l.size() == 0);
        check(l.empty());
        l.add(s1);
        check(!s1->is_unique());
        l.add(new Symbol(Symbol::THISVAR, NULL, "ghi"));
        check(l.size() == 2);
        check(!l.empty());
    }
    check(object::alloc == save_alloc);
}


void test_source()
{
    {
        in_text file(NULL, "nonexistent");
        check_throw(file.open());
        check_throw(file.get());
    }
    {
        str_fifo m(NULL, "one\t two\n567");
        check(m.preview() == 'o');
        check(m.get() == 'o');
        check(m.token(identRest) == "ne");
        m.skip(wsChars);
        check(m.token(identRest) == "two");
        check(m.eol());
        check(!m.eof());
        m.skip_eol();
        check(m.token(digits) == "567");
        check(m.eol());
        check(m.eof());
    }
    {
#ifdef XCODE
        const char* fn = "../../src/tests/parser.txt";
#else
        const char* fn = "tests/parser.txt";
#endif
        Parser p(fn, new in_text(NULL, fn));
        static Token expect[] = {
            tokIdent, tokComma, tokSep,
            tokIndent, tokAssert, tokSep,
            tokIndent, tokIdent, tokIdent, tokIdent, tokSep,
            tokIdent, tokIdent, tokConst, tokPeriod, tokSep,
            tokBlockEnd,
            tokIntValue, tokSep,
            tokIndent, tokStrValue, tokSep,
            tokIdent, tokComma, tokSep,
            tokIdent, tokSep,
            tokBlockEnd, tokBlockEnd,
            tokIdent, tokSingle, tokIdent, tokSep,
            tokIdent, tokBlockBegin, tokIdent, tokSep, 
            tokIdent, tokSep, tokBlockEnd,
            tokBlockBegin, tokIdent, tokSep,   // curly
            tokIdent, tokSingle, tokIdent, tokSep, tokBlockEnd,
            tokBlockEnd,
            tokEof
        };
        int i = 0;
        while (p.next() != tokEof && expect[i] != tokEof)
        {
            check(expect[i] == p.token);
            switch (i)
            {
            case 0: check(p.strValue == "Thunder") break;
            case 1: check(p.strValue == ","); break;
            case 17: check(p.intValue == 42); break;
            case 20: check(p.strValue == "Thunder, 'thunder',\tthunder, Thundercats"); break;
            }
            i++;
        }
        check(expect[i] == tokEof && p.token == tokEof);
    }
    {
        Parser p("mem", new str_fifo(NULL,
            INTEGER_MAX_STR"\n  "INTEGER_MAX_STR_PLUS"\n  if\n aaa"
            " 'asd\n'[\\t\\r\\n\\x41\\\\]' '\\xz'"));
        check(p.next() == tokIntValue);
        check(p.intValue == INTEGER_MAX);
        check(p.next() == tokSep);
        check(p.getLineNum() == 1);
        check(p.next() == tokIndent);
        check(p.getIndent() == 2);
        check(p.getLineNum() == 2);
        check_throw(p.next()); // integer overflow
        check(p.next() == tokSep);
        check(p.getLineNum() == 2);
        check(p.next() == tokIf);
        check(p.next() == tokSep);
        check(p.getLineNum() == 3);
        check_throw(p.next()); // unmatched unindent
        check(p.next() == tokIndent);
        check(p.next() == tokIdent);
        check_throw(p.next()); // unexpected end of line
        check(p.next() == tokStrValue);
        check(p.strValue == "[\t\r\nA\\]");
        check_throw(p.next()); // bad hex sequence
    }
}


void test_bidir_char_fifo(fifo_intf& fc)
{
    check(fc.is_char_fifo());
    fc.enq("0123456789abcdefghijklmnopqrstuvwxy");
    fc.var_enq(variant('z'));
    fc.var_enq(variant("./"));
    check(fc.preview() == '0');
    check(fc.get() == '0');
    variant v;
    fc.var_deq(v);
    check(v.as_char() == '1');
    v.clear();
    fc.var_preview(v);
    check(v.as_char() == '2');
    check(fc.deq(16) == "23456789abcdefgh");
    check(fc.deq(fifo::CHAR_ALL) == "ijklmnopqrstuvwxyz./");
    check(fc.empty());

    fc.enq("0123456789");
    fc.enq("abcdefghijklmnopqrstuvwxyz");
    check(fc.get() == '0');
    while (!fc.empty())
        fc.deq(fifo_intf::CHAR_SOME);

    fc.enq("0123456789abcdefghijklmnopqrstuvwxyz");
    check(fc.deq("0-9") == "0123456789");
    check(fc.deq("a-z") == "abcdefghijklmnopqrstuvwxyz");
    check(fc.empty());
}


void test_fifos()
{
#ifdef DEBUG
    fifo::CHUNK_SIZE = 2 * sizeof(variant);
#endif

    fifo f(NULL, false);
    objptr<vector> t = new vector(NULL);
    t->push_back(0);
    f.var_enq((vector*)t);
    f.var_enq("abc");
    f.var_enq("def");
    variant w = new range(NULL, 1, 2);
    f.var_enq(w);
    // f.dump(std::cout); std::cout << std::endl;
    variant x;
    f.var_deq(x);
    check(x.is_obj());
    f.var_deq(w);
    check(w.is(variant::STR));
    f.var_eat();
    variant vr;
    f.var_preview(vr);
    check(vr.is_obj());

    fifo fc(NULL, true);
    test_bidir_char_fifo(fc);
    
    str_fifo fs(NULL);
    test_bidir_char_fifo(fs);
}


void test_typesys()
{
    initTypeSys();
    try
    {
    check(defTypeRef->isTypeRef());
    check(defTypeRef->get_rt() == defTypeRef);
    check(queenBee->defNone->isNone());
    check(queenBee->defNone->get_rt() == defTypeRef);
    check(queenBee->defInt->isInt());
    check(queenBee->defInt->get_rt() == defTypeRef);
    check(queenBee->defInt->isOrdinal());
    check(queenBee->defBool->isBool());
    check(queenBee->defBool->get_rt() == defTypeRef);
    check(queenBee->defBool->isEnum());
    check(queenBee->defBool->isOrdinal());
    check(queenBee->defBool->rangeEq(0, 1));
    check(queenBee->defChar->isChar());
    check(queenBee->defChar->get_rt() == defTypeRef);
    check(queenBee->defChar->isOrdinal());
    check(queenBee->defStr->isString());
    check(queenBee->defStr->get_rt() == defTypeRef);
    check(queenBee->defStr->isVector());
    check(queenBee->defChar->deriveVector() == queenBee->defStr);
    check_throw(queenBee->defNone->deriveFifo());
    check_throw(queenBee->defNone->deriveSet());
    check_throw(queenBee->defNone->deriveVector());

    Symbol* b = queenBee->findDeep("true");
    check(b != NULL && b->isDefinition());
    check(PDef(b)->value.as_int() == 1);
    check(PDef(b)->type->isBool());

    {
        State state(NULL, queenBee, queenBee->defBool);
        b = state.findDeep("true");
        check(b->isDefinition());
        check_throw(state.findDeep("untrue"));
        state.addThisVar(queenBee->defInt, "a");
        check_throw(state.addThisVar(queenBee->defInt, "a"));
        state.addThisVar(queenBee->defInt, "true");
        state.addTypeAlias("ool", queenBee->defBool);
        b = state.findDeep("ool");
        check(b->isDefinition());
        check(b->isTypeAlias());
        check(PDef(b)->aliasedType()->isBool());
        state.addThisVar(queenBee->defInt, "v");
        Symbol* v = state.findDeep("v");
        check(v->isThisVar());
        check(PVar(v)->id == 2);
        state.addThisVar(queenBee->defChar, "c1");
        state.addThisVar(queenBee->defChar, "c2");
        v = state.findDeep("c2");
        check(v->isThisVar());
        check(PVar(v)->id == 4);
        b = state.findDeep("result");
        check(b->isResultVar());
        check(PVar(b)->type->isBool());
    }
    }
    catch(exception&)
    {
        doneTypeSys();
    }
    doneTypeSys();
}


void test_vm()
{
    initTypeSys();

    try
    {

    {
        Module mod("test");

        Constant* c = mod.addConstant(queenBee->defChar, "c", char(1));

        // Arithmetic, typecasts
        variant r;
        ConstCode seg;
        {
            CodeGen gen(&seg);
            gen.loadConst(c->type, c->value);
            gen.explicitCastTo(queenBee->defVariant, "Huh?");
            gen.toBool();
            gen.explicitCastTo(queenBee->defInt, "Huh?");
            gen.loadInt(9);
            gen.arithmBinary(opAdd);
            gen.loadInt(2);
            gen.arithmUnary(opNeg);
            gen.arithmBinary(opSub);
            gen.explicitCastTo(queenBee->defStr, "Huh?");
            gen.endConstExpr(queenBee->defStr);
        }
        seg.run(r);
        check(r.as_str() == "12");

        // String operations
        c = mod.addConstant(queenBee->defStr, "s", "ef");
        seg.clear();
        {
            CodeGen gen(&seg);
            gen.loadChar('a');
            gen.elemToVec();
            gen.loadChar('b');
            gen.elemCat();
            gen.loadStr("cd");
            gen.cat();
            gen.loadStr("");
            gen.cat();
            gen.loadConst(c->type, c->value);
            gen.cat();
            gen.endConstExpr(queenBee->defStr);
        }
        seg.run(r);
        check(r.as_str() == "abcdef");

        // Range operations
        seg.clear();
        {
            CodeGen gen(&seg);
            gen.loadInt(6);
            gen.loadInt(5);
            gen.loadInt(10);
            gen.mkRange();
            gen.inRange();
            gen.loadInt(1);
            gen.loadInt(5);
            gen.loadInt(10);
            gen.mkRange();
            gen.inRange();
            gen.mkRange();
            gen.endConstExpr(queenBee->defBool->deriveRange());
        }
        seg.run(r);
        check(prange(r.as_obj())->get_rt()->isRange()
            && prange(r.as_obj())->equals(1, 0));

        // Vector concatenation
        vector* t = new vector(queenBee->defInt->deriveVector(), 1, 3);
        t->push_back(4);
        c = mod.addConstant(queenBee->defInt->deriveVector(), "v", t);
        seg.clear();
        {
            CodeGen gen(&seg);
            gen.loadInt(1);
            gen.elemToVec();
            gen.loadInt(2);
            gen.elemCat();
            gen.loadConst(c->type, c->value);
            gen.cat();
            gen.endConstExpr(queenBee->defInt->deriveVector());
        }
        seg.run(r);
        check(r.to_string() == "[1, 2, 3, 4]");

        seg.clear();
        {
            CodeGen gen(&seg);
            gen.loadBool(true);
            gen.elemToVec();

            gen.loadStr("abc");
            gen.loadStr("abc");
            gen.cmp(opEqual);
            gen.elemCat();

            gen.loadInt(1);
            gen.elemToVec();
            gen.loadTypeRef(queenBee->defInt->deriveVector());
            gen.dynamicCast();
            gen.loadTypeRef(queenBee->defInt->deriveVector());
            gen.testType();
            gen.elemCat();

            mem s = seg.size();
            gen.loadInt(1);
            gen.testType(queenBee->defInt); // compile-time
            gen.testType(queenBee->defBool);
            check(s == seg.size() - 1);
            gen.elemCat();
            gen.loadConst(queenBee->defVariant, 2); // doesn't yield variant actually
            gen.implicitCastTo(queenBee->defVariant, "Type mismatch");
            gen.testType(queenBee->defVariant);
            gen.elemCat();
            gen.loadStr("");
            gen.toBool();
            gen.elemCat();
            gen.loadStr("abc");
            gen.toBool();
            gen.elemCat();
            gen.endConstExpr(queenBee->defBool->deriveVector());
        }
        seg.run(r);
        check(r.to_string() == "[true, true, true, true, true, false, true]");
    }

    {
        Module mod("test2");
        Dict* dictType = queenBee->defInt->createContainer(queenBee->defStr);
        dict* d = new dict(dictType);
        d->tie("key1", 2);
        d->tie("key2", 3);
        Constant* c = mod.addConstant(dictType, "dict", d);
        Array* arrayType = queenBee->defStr->createContainer(queenBee->defBool);
        Ordset* ordsetType = queenBee->defChar->deriveSet();
        Set* setType = queenBee->defInt->deriveSet();
        check(!setType->isOrdset());
        {
            CodeGen gen(&mod);
            BlockScope block(&mod, &gen);

            Variable* v1 = block.addLocalVar(dictType, "v1");
            gen.loadStr("k1");
            gen.loadInt(15);
            gen.pairToDict(dictType);
            gen.initLocalVar(v1);
            gen.loadVar(v1);
            gen.loadStr("k2");
            gen.loadInt(25);
            gen.storeContainerElem();

            Variable* v2 = block.addLocalVar(arrayType, "v2");
            gen.loadNullComp(arrayType);
            gen.initLocalVar(v2);
            gen.loadVar(v2);
            gen.loadBool(false);
            gen.loadStr("abc");
            gen.storeContainerElem(false);
            gen.loadBool(true);
            gen.loadStr("def");
            gen.storeContainerElem();

            Variable* v3 = block.addLocalVar(ordsetType, "v3");
            gen.loadNullComp(NULL);
            gen.implicitCastTo(ordsetType, "Huh?");
            gen.initLocalVar(v3);
            gen.loadVar(v3);
            gen.loadChar('a');
            gen.addToSet();
            gen.loadVar(v3);
            gen.loadChar('b');
            gen.addToSet();

            Variable* v4 = block.addLocalVar(setType, "v4");
            gen.loadNullComp(setType);
            gen.initLocalVar(v4);
            gen.loadVar(v4);
            gen.loadInt(100);
            gen.addToSet();
            gen.loadVar(v4);
            gen.loadInt(1000);
            gen.addToSet();
            gen.loadVar(v4);
            gen.loadInt(2000);
            gen.addToSet();
            gen.loadVar(v4);
            gen.loadInt(1000);
            gen.delSetElem();

            gen.loadConst(queenBee->defVariant, 10);
            gen.elemToVec();
            gen.loadStr("xyz");
            gen.loadInt(1);
            gen.loadContainerElem();
            gen.elemCat();
            gen.dup();
            gen.loadInt(0);
            gen.loadContainerElem();
            gen.elemCat();
            gen.loadConst(c->type, c->value);
            gen.loadStr("key2");
            gen.loadContainerElem();
            gen.elemCat();
            gen.loadVar(v1);
            gen.elemCat();
            gen.loadVar(v2);
            gen.elemCat();
            gen.dup();
            gen.loadInt(6);
            gen.loadInt(21);
            gen.storeContainerElem();
            gen.dup();
            gen.loadInt(6);
            gen.loadInt(22);
            gen.storeContainerElem();
            gen.loadVar(v3);
            gen.elemCat();
            gen.loadVar(v4);
            gen.elemCat();

            gen.loadChar('a');
            gen.loadVar(v3);
            gen.inSet();
            gen.elemCat();

            gen.loadChar('c');
            gen.loadVar(v3);
            gen.inSet();
            gen.elemCat();

            gen.loadInt(2000);
            gen.loadVar(v4);
            gen.inSet();
            gen.elemCat();

            gen.loadInt(1000);
            gen.loadVar(v4);
            gen.inSet();
            gen.elemCat();

            gen.loadStr("k3");
            gen.loadVar(v1);
            gen.keyInDict();
            gen.elemCat();

            gen.loadStr("k1");
            gen.loadVar(v1);
            gen.keyInDict();
            gen.elemCat();

            ThisVar* var = mod.addThisVar(queenBee->defInt, "var");
            gen.loadInt(200);
            gen.initThisVar(var);
            gen.loadVar(var);
            gen.elemCat();

            gen.loadVar(v1);
            gen.loadStr("k2");
            gen.delDictElem();

            Symbol* io = mod.findDeep("sio");
            check(io->isVariable());
            gen.loadVar(PVar(io));
            gen.elemCat();

            Symbol* r = mod.findDeep("sresult");
            check(r->isVariable());
            gen.loadVar(PVar(r));
            gen.elemCat();
            
            gen.storeVar(queenBee->sresultvar);
            block.deinitLocals();
            gen.end();
        }
        variant result = mod.run();
        str s = result.to_string();
        check(s ==
            "[10, 'y', 10, 3, ['k1': 15], ['abc', 'def'], 22, [97, 98], [100, 2000], "
            "true, false, true, false, false, true, 200, <char-fifo>, null]");
    }

    {
        Module mod("test2");
        {
            CodeGen gen(&mod);
            BlockScope block(&mod, &gen);

            Variable* s0 = block.addLocalVar(queenBee->defVariant->deriveVector(), "s0");
            gen.loadNullComp(NULL);
            gen.implicitCastTo(queenBee->defVariant->deriveVector(), "Huh?");
            gen.initLocalVar(s0);

            Variable* s1 = block.addLocalVar(queenBee->defStr, "s1");
            gen.loadStr("abc");
            gen.loadStr("def");
            gen.cat();
            gen.initLocalVar(s1);

            Variable* s2 = block.addLocalVar(queenBee->defInt, "s2");
            gen.loadInt(123);
            gen.initLocalVar(s2);

            Variable* s3 = block.addLocalVar(queenBee->defStr->deriveSet(), "s3");
            gen.loadStr("abc");
            gen.elemToSet();
            gen.initLocalVar(s3);
            
            gen.loadVar(s0);
            gen.loadVar(s1);
            gen.elemCat();
            gen.loadVar(s2);
            gen.elemCat();
            gen.dup();
            gen.count();
            gen.elemCat();

            mem o1 = gen.jumpForward();
            gen.nop();
            gen.resolveJump(o1);

            // if(true, 10, 20)
            gen.loadBool(true);
            mem of = gen.boolJumpForward(opJumpFalse);
            gen.loadInt(10);
            gen.genPop();
            mem oj = gen.jumpForward();
            gen.resolveJump(of);
            gen.loadInt(20);
            gen.resolveJump(oj);
            gen.elemCat();

            gen.loadChar('a');
            gen.caseLabel(queenBee->defChar, 'a');
            gen.swap();
            gen.discard();
            gen.elemCat();

            gen.loadInt(10);
            gen.caseLabel(queenBee->defInt->deriveRange(), new range(NULL, 9, 11));
            gen.swap();
            gen.discard();
            gen.elemCat();

            gen.loadStr("22:49");
            gen.caseLabel(queenBee->defStr, "22:49");
            gen.swap();
            gen.discard();
            gen.elemCat();

            gen.loadTypeRef(queenBee->defInt);
            gen.caseLabel(defTypeRef, queenBee->defInt);
            gen.swap();
            gen.discard();
            gen.elemCat();

            gen.loadVar(s3);
            gen.loadChar('d');
            gen.addToSet();
            gen.loadChar('d');
            gen.loadVar(s3);
            gen.inSet();
            gen.elemCat();

//            gen.loadStr("The value of true is: ");
//            gen.echo();
//            gen.loadBool(true);
//            gen.echo();
//            gen.echoLn();
//            mem f = mod.registerAssertFileName(__FILE__);
//            gen.loadBool(false);
//            gen.assertion(f, __LINE__);

            gen.exit();

            block.deinitLocals();   // not reached
            gen.end();
        }
        variant result = mod.run();
        str s = result.to_string();
        check(s == "['abcdef', 123, 2, 10, true, true, true, true, true]");
    }

    }
    catch (exception&)
    {
        doneTypeSys();
        throw;
    }

    doneTypeSys();
}


int main()
{
    sio << "short: " << sizeof(short) << "  long: " << sizeof(long)
         << "  long long: " << sizeof(long long) << "  int: " << sizeof(int)
         << "  void*: " << sizeof(void*) << "  float: " << sizeof(float)
         << "  double: " << sizeof(double) << '\n';
    sio << "integer: " << sizeof(integer) << "  mem: " << sizeof(mem)
         << "  real: " << sizeof(real) << "  variant: " << sizeof(variant)
         << "  object: " << sizeof(object) << "  joffs: " << sizeof(joffs_t) << '\n';

    check(sizeof(int) == 4);
    check(sizeof(mem) >= 4);

#ifdef SH64
    check(sizeof(integer) == 8);
#  ifdef PTR64
    check(sizeof(variant) == 16);
#  else
    check(sizeof(variant) == 12);
#  endif
#else
    check(sizeof(integer) == 4);
    check(sizeof(variant) == 8);
#endif

    int exitcode = 0;
    try
    {
        test_common();
        test_variant();
        test_symbols();
        test_source();
        test_fifos();
        test_typesys();
        test_vm();
    }
    catch (exception& e)
    {
        fprintf(stderr, "Exception: %s\n", e.what());
        exitcode = 201;
    }

    if (object::alloc != 0)
    {
        fprintf(stderr, "Error: object::alloc = %d\n", object::alloc);
        exitcode = 202;
    }

    return exitcode;
}

