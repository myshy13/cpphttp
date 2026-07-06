CXX := clang++
CXXFLAGS := -std=c++17 -Iinclude -I/opt/homebrew/include

SRCS := $(wildcard *.cpp src/*.cpp test/*.cpp)

LIB_SRCS := $(filter-out src/main.cpp,$(SRCS))

PREBUILD := cmake -S . -B build

NC := $(tput sgr0)
BOLD := $(tput bold)
GREEN := $(tput setaf 2)

.PHONY: test clean

test:
	$(CXX) $(CXXFLAGS) $(LIB_SRCS) -o testout && ./testout; rm -rf testout

clean:
	rm -f testout

all:
	echo "\033[1;32mBuild with 'cmake --build build'. Do it yourself\033[0m"