all: libredispp.a libredispp.so unittests perftest multitest transtest

CXX ?= g++
CXXFLAGS ?= -std=c++11 -g -O0 -Isrc $(EXTRA_CXXFLAGS) -Werror

VPATH += src test

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $^ -o $@

libredispp.a: redispp.o
	ar cr libredispp.a redispp.o

%.pic.o: %.cpp
	$(CXX) -fPIC $(CXXFLAGS) -c $^ -o $@

libredispp.so: redispp.pic.o
	$(CXX) -shared $^ -o $@

unittests: test.o libredispp.a
	$(CXX) $^ libredispp.a -o $@

perftest: perf.o libredispp.a
	$(CXX) $^ libredispp.a -o $@

multitest: multi.o libredispp.a
	$(CXX) $^ libredispp.a -o $@

transtest: trans.o libredispp.a
	$(CXX) $^ libredispp.a -o $@

clang-format:
	for f in src/*.cpp src/*.h test/*.cpp; do clang-format $$f | sponge $$f; done

clean:
	rm -f *.o libredispp.a libredispp.so perftest unittests multitest transtest
