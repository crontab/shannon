
CXXDEFS =
# -DSMALLER_SLOWER
CXXOPTIM = -Os
CXXOPTS = -Wall -Werror $(CXXDEFS)
CXXDEBUG = -DDEBUG -g
# CXXDEBUG = -DNDEBUG
# LDLIBS =

OBJS = obj/portatom.o obj/portmem.o obj/variant.o

SRCS = portatom.cpp portmem.cpp variant.cpp main.cpp main-ut.cpp

HDRS = common.h variant.h

all: dirs shannon shannon-ut

ut: dirs shannon-ut
	@echo
	@if ./shannon-ut ; then echo "Unit tests succeeded." ; else echo "***** Unit tests failed *****" ; fi
	@echo

dirs:
	@mkdir -p obj

depend: Makefile.dep

Makefile.dep: $(SRCS) $(HDRS)
	@touch $@
	makedepend $(CXXDEFS) -pobj/ -f$@ -Y $(SRCS) 2>/dev/null
	@if diff $@.bak $@ > /dev/null ; then echo "Dependencies unchanged" ; else echo "****** Dependecies have changed" ; fi
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

