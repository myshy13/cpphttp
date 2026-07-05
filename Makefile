CXX := clang++
CXXFLAGS := -std=c++17 -Iinclude -I/opt/homebrew/include

SRCS := $(wildcard *.cpp src/*.cpp test/*.cpp)

LIB_SRCS := $(filter-out src/main.cpp,$(SRCS))

.PHONY: test clean

test:
	$(CXX) $(CXXFLAGS) $(LIB_SRCS) -o testout && ./testout && rm -rf testout

clean:
	rm -f testout
