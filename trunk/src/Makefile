
CXXDEFS = -DDEBUG
# DDEBUG = -DNDEBUG
CXXOPTS = -Os -Wall -Werror $(CXXDEFS)

OBJS = obj/portatom.o obj/portmem.o obj/strbase.o obj/except.o obj/charset.o obj/strconv.o \
    obj/contain.o obj/baseobj.o obj/source.o

SRCS = portatom.cpp portmem.cpp strbase.cpp strconv.cpp except.cpp charset.cpp \
    contain.cpp baseobj.cpp source.cpp \
    main.cpp main-ut.cpp

HDRS = port.h str.h except.h charset.h contain.h baseobj.h source.h

all: dirs bin/shannon bin/shannon-ut

ut: dirs bin/shannon-ut
	@echo
	@if bin/shannon-ut ; then echo "Unit tests succeeded." ; else echo "***** Unit tests failed *****" ; fi
	@echo

dirs:
	@mkdir -p obj bin

depend: Makefile.dep

Makefile.dep: $(SRCS) $(HDRS)
	@touch $@
	makedepend $(CXXDEFS) -pobj/ -f$@ -Y $(SRCS) 2>/dev/null
	@if diff $@.bak $@ > /dev/null ; then echo "Dependencies unchanged" ; else echo "****** Dependecies have changed" ; fi
	@rm -f $@.bak

include Makefile.dep

bin/shannon: $(OBJS) obj/main.o
	$(CXX) $^ -o $@

bin/shannon-ut: $(OBJS) obj/main-ut.o
	$(CXX) $^ -o $@

obj/%.o: %.cpp
	$(CXX) $(CXXOPTS) -c $< -o $@

clean:
	@rm -f $(OBJS) obj/main.o obj/main-ut.o
	@rm -f bin/shannon bin/shannon-ut
	@rm -f core *.core

