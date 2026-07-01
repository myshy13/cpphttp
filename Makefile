all:
	rm -rf build; g++ -std=c++23 server.cpp -o build/server && ./build/server