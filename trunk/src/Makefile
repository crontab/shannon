
DDEBUG = -DDEBUG -g
# DDEBUG = -DNDEBUG
CXXOPTS = -Os -Wall -Werror $(DDEBUG)

OBJS = obj/portatom.o obj/portmem.o obj/strbase.o obj/except.o obj/charset.o obj/strconv.o \
    obj/array.o obj/baseobj.o obj/source.o

all: dirs bin/shannon

ut: dirs bin/shannon-ut
	@echo ; if bin/shannon-ut ; then echo "Unit tests succeeded." ; else echo "***** Unit tests failed *****" ; fi ; echo

dirs:
	@mkdir -p obj bin

bin/shannon: $(OBJS) obj/main.o
	$(CXX) $^ -o $@

bin/shannon-ut: $(OBJS) obj/main-ut.o
	$(CXX) $^ -o $@

obj/portatom.o: portatom.cpp port.h
	$(CXX) $(CXXOPTS) -c $< -o $@

obj/portmem.o: portmem.cpp port.h
	$(CXX) $(CXXOPTS) -c $< -o $@

obj/strbase.o: strbase.cpp str.h port.h
	$(CXX) $(CXXOPTS) -c $< -o $@

obj/except.o: except.cpp except.h str.h port.h
	$(CXX) $(CXXOPTS) -c $< -o $@

obj/charset.o: charset.cpp charset.h port.h
	$(CXX) $(CXXOPTS) -c $< -o $@

obj/strconv.o: strconv.cpp str.h port.h
	$(CXX) $(CXXOPTS) -c $< -o $@

obj/array.o: array.cpp str.h array.h port.h
	$(CXX) $(CXXOPTS) -c $< -o $@

obj/baseobj.o: baseobj.cpp baseobj.h array.h str.h except.h port.h
	$(CXX) $(CXXOPTS) -c $< -o $@

obj/source.o: source.cpp source.h charset.h str.h except.h port.h
	$(CXX) $(CXXOPTS) -c $< -o $@

obj/main.o: main.cpp baseobj.h source.h charset.h array.h str.h port.h
	$(CXX) $(CXXOPTS) -c $< -o $@

obj/main-ut.o: main-ut.cpp baseobj.h source.h charset.h array.h str.h port.h
	$(CXX) $(CXXOPTS) -c $< -o $@

clean:
	@rm -f $(OBJS) obj/main.o obj/main-ut.o
	@rm -f bin/shannon bin/shannon-ut
	@rm -f core *.core
