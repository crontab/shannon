
# ARCH = -arch i386
# ARCH = -arch x86_64
# SHBITS = -DSHN_64
# SHTHR = -DSHN_THR

CXXDOPTS = $(ARCH) $(SHBITS) $(SHTHR) -Wall -Wextra -Werror -DDEBUG -g
CXXROPTS = $(ARCH) $(SHBITS) $(SHTHR) -Wall -Wextra -Werror -Wno-strict-aliasing -DNDEBUG -O2
LDLIBS = -ldl

DOBJS = debug/common.o debug/runtime.o debug/rtio.o \
    debug/parser.o debug/typesys.o debug/vm.o debug/vmcodegen.o \
    debug/vminfo.o debug/compexpr.o debug/compiler.o

ROBJS = release/common.o release/runtime.o release/rtio.o \
    release/parser.o release/typesys.o release/vm.o release/vmcodegen.o \
    release/vminfo.o release/compexpr.o release/compiler.o

SRCS = common.cpp runtime.cpp rtio.cpp \
    parser.cpp typesys.cpp vm.cpp vmcodegen.cpp \
    vminfo.cpp compexpr.cpp compiler.cpp \
    main.cpp main-ut.cpp

HDRS = version.h common.h runtime.h typesys.h parser.h vm.h compiler.h


all: dirs shannon shannon-ut release

release: dirs shn

ut unit-test: dirs shannon-ut
	@echo
	@if ./shannon-ut ; then echo "Unit tests succeeded." ; else echo "***** Unit tests failed *****" ; fi
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

%.s: %.cpp Makefile
	$(CXX) $(CXXROPTS) -S -fverbose-asm $< -o $@

clean:
	@rm -f $(DOBJS) debug/main.o debug/main-ut.o
	@rm -f $(ROBJS) release/main.o
	@rm -f shannon shannon-ut shn
	@rm -f core *.core

