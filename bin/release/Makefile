gcc: CC := g++
gcc: CXXFLAGS = -Wall -W -Wextra -Wpedantic -Wformat-security -Walloca -Wduplicated-branches -g -std=c++2a -fconcepts -Ofast -march=native
#gcc: CXXFLAGS += -fsanitize=address -fsanitize-recover=address -fsanitize=undefined -fsanitize-address-use-after-scope -fsanitize=signed-integer-overflow -fsanitize=vptr
gcc: LDFLAGS = -lpthread
gcc: ../../src/main.cpp
	$(CC) $(CXXFLAGS) ../../src/main.cpp -o main $(LDFLAGS)

clean:
	@- $(RM) main

