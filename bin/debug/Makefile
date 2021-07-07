clang: CC := clang++
clang: CXXFLAGS = -Wall -g -std=c++20 -Wno-c99-extensions -Wno-c++98-compat-pedantic 
#-stdlib=libc++
clang: CXXFLAGS += -fsanitize=address -fsanitize-recover=address -fsanitize=undefined -fsanitize-address-use-after-scope -fsanitize=signed-integer-overflow -fsanitize=vptr
clang: ../../src/main.cpp
	$(CC) $(CXXFLAGS) ../../src/bit_twiddling_hacks.cc -o bit_twiddling_hacks $(LDFLAGS)
	$(CC) $(CXXFLAGS) ../../src/main.cpp -o main $(LDFLAGS)
	$(CC) $(CXXFLAGS) ../../src/rev3.cpp -o rev3 $(LDFLAGS)
	$(CC) $(CXXFLAGS) ../../src/rev4.cpp -o rev4 $(LDFLAGS)	

gcc: CC := g++
gcc: CXXFLAGS = -Wall -W -Wextra -Wpedantic -Wformat-security -Walloca -Wduplicated-branches -g -std=c++20 -fconcepts
gcc: CXXFLAGS += -fsanitize=address -fsanitize-recover=address -fsanitize=undefined -fsanitize-address-use-after-scope -fsanitize=signed-integer-overflow -fsanitize=vptr
gcc: CXXFLAGS += -march=native
gcc: LDFLAGS = -lpthread
gcc: ../../src/main.cpp ../../src/rev3.cpp
	$(CC) $(CXXFLAGS) ../../src/main.cpp -o main $(LDFLAGS)
	$(CC) $(CXXFLAGS) ../../src/rev3.cpp -o rev3 $(LDFLAGS)
	$(CC) $(CXXFLAGS) ../../src/rev4.cpp -o rev4 $(LDFLAGS)
clean:
	@- $(RM) main rev3 rev4

distclean: clean
