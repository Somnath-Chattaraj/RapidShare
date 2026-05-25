CXX := clang++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2 -pthread -Iinclude
COMMON := src/net.cpp src/checksum.cpp

.PHONY: all test clean

all: build/rapidshare_server build/rapidshare_client

build:
	mkdir -p build

build/rapidshare_server: src/server.cpp $(COMMON) | build
	$(CXX) $(CXXFLAGS) $^ -o $@

build/rapidshare_client: src/client.cpp $(COMMON) | build
	$(CXX) $(CXXFLAGS) $^ -o $@

build/rapidshare_tests: tests/tests.cpp $(COMMON) | build
	$(CXX) $(CXXFLAGS) $^ -o $@

test: build/rapidshare_tests
	./build/rapidshare_tests

clean:
	rm -rf build
