clang: CC := clang++
clang: CXXFLAGS = -Wall -Weverything -g -std=c++2a -Wno-c99-extensions -Wno-c++98-compat-pedantic -stdlib=libc++
clang: CXXFLAGS += -fsanitize=address -fsanitize-recover=address -fsanitize=undefined -fsanitize-address-use-after-scope -fsanitize=signed-integer-overflow -fsanitize=vptr
clang: ../../src/main.cpp
	$(CC) $(CXXFLAGS) ../../src/main.cpp -o main $(LDFLAGS)

gcc: CC := g++
gcc: CXXFLAGS = -Wall -W -Wextra -Wpedantic -Wformat-security -Walloca -Wduplicated-branches -g -std=c++2a -fconcepts
gcc: CXXFLAGS += -fsanitize=address -fsanitize-recover=address -fsanitize=undefined -fsanitize-address-use-after-scope -fsanitize=signed-integer-overflow -fsanitize=vptr
gcc: ../../src/main.cpp
	$(CC) $(CXXFLAGS) ../../src/main.cpp -o main $(LDFLAGS)

clean:
	@- $(RM) main

distclean: clean
