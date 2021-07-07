gcc: CC := g++
gcc: CXXFLAGS = -Wall -W -Wextra -Wpedantic -Wformat-security -Walloca -Wduplicated-branches -std=c++20 -fconcepts -Ofast -march=native
#gcc: CXXFLAGS += -fsanitize=address -fsanitize-recover=address -fsanitize=undefined -fsanitize-address-use-after-scope -fsanitize=signed-integer-overflow -fsanitize=vptr
gcc: LDFLAGS = -lpthread
gcc: ../../src/main.cpp ../../src/rev3.cpp
	$(CC) $(CXXFLAGS) ../../src/main.cpp -o main $(LDFLAGS)
	$(CC) $(CXXFLAGS) ../../src/rev3.cpp -o rev3 $(LDFLAGS)

clean:
	@- $(RM) main rev3

