all: libredispp.a libredispp.so unittests perftest multitest transtest

CXXFLAGS ?= -std=c++11 -g -O0 -Isrc $(EXTRA_CXXFLAGS) -Werror

VPATH += src test

%.o: %.cpp
	g++ $(CXXFLAGS) -c $^ -o $@

libredispp.a: redispp.o
	ar cr libredispp.a redispp.o

%.pic.o: %.cpp
	g++ -fPIC $(CXXFLAGS) -c $^ -o $@

libredispp.so: redispp.pic.o
	g++ -shared $^ -o $@

unittests: test.o libredispp.a
	g++ $^ libredispp.a -o $@

perftest: perf.o libredispp.a
	g++ $^ libredispp.a -o $@

multitest: multi.o libredispp.a
	g++ $^ libredispp.a -o $@

transtest: trans.o libredispp.a
	g++ $^ libredispp.a -o $@

clean:
	rm -f *.o libredispp.a libredispp.so perftest unittests multitest transtest
