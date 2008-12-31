
CXXDEFS = -DSINGLE_THREADED
# -DSMALLER_SLOWER
CXXOPTIM = -Os
CXXOPTS = -Wall -Werror $(CXXDEFS)
CXXDEBUG = -DDEBUG -g
# CXXDEBUG = -DNDEBUG
# LDLIBS =

OBJS = obj/portatom.o obj/portmem.o obj/strbase.o obj/except.o obj/charset.o obj/strconv.o \
    obj/contain.o obj/baseobj.o obj/source.o obj/langobj.o obj/vm.o obj/codegen.o

SRCS = portatom.cpp portmem.cpp strbase.cpp strconv.cpp except.cpp charset.cpp \
    contain.cpp baseobj.cpp source.cpp langobj.cpp vm.cpp codegen.cpp \
    main.cpp main-ut.cpp

HDRS = port.h str.h except.h charset.h contain.h baseobj.h source.h langobj.h vm.h codegen.h

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

