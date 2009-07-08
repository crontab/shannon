

CXXDOPTS = -Wall -Werror -fno-rtti -DDEBUG -g
CXXROPTS = -Wall -Werror -fno-rtti -O2


DOBJS = debug/common.o debug/charset.o debug/variant.o debug/source.o \
	debug/symbols.o debug/typesys.o

ROBJS = release/common.o release/charset.o release/variant.o release/source.o \
	release/symbols.o release/typesys.o

SRCS = common.cpp charset.cpp variant.cpp source.cpp \
	symbols.cpp typesys.cpp \
	main.cpp main-ut.cpp

HDRS = common.h charset.h variant.h bsearch.h source.h \
	symbols.h typesys.h


all: dirs shannon shannon-ut

release: dirs shn

ut unit-test: dirs shannon-ut
	@if ./shannon-ut ; then echo ; echo "Unit tests succeeded." ; else echo; echo "***** Unit tests failed *****" ; fi
	@echo

dirs:
	@mkdir -p debug release

depend: Makefile.dep.debug Makefile.dep.release


Makefile.dep.debug: $(SRCS) $(HDRS)
	@touch $@
	@makedepend -pdebug/ -f$@ -Y $(SRCS) 2>/dev/null
	@if diff $@.bak $@ > /dev/null ; then echo "Dependencies unchanged" ; else echo "****** DEBUG dependecies have changed" ; fi
	@rm -f $@.bak

include Makefile.dep.debug


Makefile.dep.release: $(SRCS) $(HDRS)
	@touch $@
	@makedepend -prelease/ -f$@ -Y $(SRCS) 2>/dev/null
	@if diff $@.bak $@ > /dev/null ; then echo "Dependencies unchanged" ; else echo "****** RELEASE dependecies have changed" ; fi
	@rm -f $@.bak

include Makefile.dep.release


shannon: $(DOBJS) debug/main.o
	$(CXX) $(CXXDOPTS) $(LDLIBS) $^ -o $@

shannon-ut: $(DOBJS) debug/main-ut.o
	$(CXX) $(CXXDOPTS) $(LDLIBS) $^ -o $@

debug/%.o: %.cpp Makefile
	$(CXX) $(CXXDOPTS) -c $< -o $@

shn: $(ROBJS) release/main.o
	$(CXX) $(CXXROPTS) $(LDLIBS) $^ -o $@
	@strip $@

release/%.o: %.cpp Makefile
	$(CXX) $(CXXROPTS) -c $< -o $@

%.s: %.cpp
	$(CXX) $(CXXROPTS) -S -fverbose-asm $< -o $@

clean:
	@rm -f $(DOBJS) debug/main.o debug/main-ut.o
	@rm -f $(ROBJS) release/main.o
	@rm -f shannon shannon-ut shn
	@rm -f core *.core

