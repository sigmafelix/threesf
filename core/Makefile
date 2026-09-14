CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -fPIC -Iinclude -DTHREESF_BUILDING
UNAME_S  := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  SHLIB := build/libthreesf.dylib
  SHFLAGS := -dynamiclib
else
  SHLIB := build/libthreesf.so
  SHFLAGS := -shared
endif

all: $(SHLIB) build/test_threesf build/test_io build/test_constructive

build:
	mkdir -p build

$(SHLIB): src/threesf_c.cpp include/threesf/threesf.hpp include/threesf/constructive.hpp include/threesf/io.hpp include/threesf/threesf_c.h | build
	$(CXX) $(CXXFLAGS) $(SHFLAGS) -o $@ src/threesf_c.cpp

build/test_threesf: ../tests/test_threesf.cpp src/threesf_c.cpp include/threesf/threesf.hpp include/threesf/constructive.hpp | build
	$(CXX) $(CXXFLAGS) -o $@ ../tests/test_threesf.cpp src/threesf_c.cpp

build/test_io: ../tests/test_io.cpp src/threesf_c.cpp include/threesf/threesf.hpp include/threesf/constructive.hpp include/threesf/io.hpp | build
	$(CXX) $(CXXFLAGS) -o $@ ../tests/test_io.cpp src/threesf_c.cpp

build/test_constructive: ../tests/test_constructive.cpp src/threesf_c.cpp include/threesf/threesf.hpp include/threesf/constructive.hpp include/threesf/io.hpp include/threesf/threesf_c.h | build
	$(CXX) $(CXXFLAGS) -o $@ ../tests/test_constructive.cpp src/threesf_c.cpp

test: build/test_threesf build/test_io build/test_constructive
	./build/test_threesf
	./build/test_io
	./build/test_constructive

# Copy the shared library next to the Python package so `import threesf` finds it.
python: $(SHLIB)
	cp $(SHLIB) ../python/threesf/

# Sync the header into the R package (it is compiled directly, no linking).
r-sync:
	cp include/threesf/threesf.hpp include/threesf/constructive.hpp include/threesf/io.hpp include/threesf/threesf_c.h ../r/threesf/inst/include/threesf/
	cp src/threesf_c.cpp ../r/threesf/src/threesf_c.cpp

clean:
	rm -rf build ../build ../python/build ../python/dist ../python/threesf.egg-info ../python/threesf/_native
	rm -f ../python/threesf/libthreesf.* ../python/threesf/_libthreesf.* ../r/threesf/src/*.o ../r/threesf/src/threesf.so ../r/threesf/src/threesf.dll

.PHONY: all test python r-sync clean
