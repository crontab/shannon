
CXXDEFS =
# -DSMALLER_SLOWER
CXXOPTIM = -Os
CXXOPTS = -Wall -Werror $(CXXDEFS)
CXXDEBUG = -DDEBUG -g
# CXXDEBUG = -DNDEBUG
# LDLIBS =

OBJS = obj/common.o obj/charset.o obj/variant.o obj/source.o obj/symbols.o

SRCS = common.cpp charset.cpp variant.cpp source.cpp symbols.cpp main.cpp main-ut.cpp

HDRS = common.h charset.h variant.h bsearch.h source.h symbols.h

all: dirs shannon shannon-ut

ut unit-test: dirs shannon-ut
	@if ./shannon-ut ; then echo ; echo "Unit tests succeeded." ; else echo; echo "***** Unit tests failed *****" ; fi
	@echo

dirs:
	@mkdir -p obj

depend: Makefile.dep

Makefile.dep: $(SRCS) $(HDRS)
	@touch $@
	makedepend $(CXXDEFS) -pobj/ -f$@ -Y $(SRCS) 2>/dev/null
	@if diff $@.bak $@ > /dev/null ; then echo ; else echo "****** Dependecies have changed" ; fi
	@rm -f $@.bak

include Makefile.dep

shannon: $(OBJS) obj/main.o
	$(CXX) $(LDLIBS) $^ -o $@

shannon-ut: $(OBJS) obj/main-ut.o
	$(CXX) $(LDLIBS) $^ -o $@

obj/%.o: %.cpp Makefile
	$(CXX) $(CXXDEBUG) $(CXXOPTIM) $(CXXOPTS) -c $< -o $@

%.s: %.cpp
	$(CXX) $(CXXOPTIM) $(CXXOPTS) -S -fverbose-asm $< -o $@

clean:
	@rm -f $(OBJS) obj/main.o obj/main-ut.o
	@rm -f shannon shannon-ut
	@rm -f core *.core

