/* The Computer Language Benchmarks Game
 * https://salsa.debian.org/benchmarksgame-team/benchmarksgame/

   Contributed by yurai007
   Based on idea from Mr Ledrug implementation
*/
#include <cstring>
#include <sys/stat.h>
#include <ctime>
#include <unistd.h>
#include <array>
#include <vector>
#include <cstdio>
#include <thread>
#include <algorithm>
#include <sys/mman.h>
#include <string.h>

/*
 Reverse group by group
 0. grouping via manual search + std::reverse. 0.68 GB/s.
 1. grouping via strchr + std::reverse. 1 GB/s.
 2. mmap instead new. 1.03 GB/s
    I/O time from 0.9s to 0.85s.
    Process time - 130ms (with ~8 GB/s) - on gcc. LLVM has problems with std::reverse
    vectorization.
    Cost of 0.85s is high but mostly because of allocation lot of virtual memory
    (it takes ~0.2s). For now it's impossible to get rid if this cost.
 3. For now every byte is touched 4 times:
    - read
    - search
    - std::reverse
    - write
 4. Summary:
        real	0m0.975s
*/

static inline uint64_t realtime_now() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1'000'000'000ULL + ts.tv_nsec;
}

static unsigned get_buffer_capacity() {
    struct stat fileinfo;
    fstat(fileno(stdin), &fileinfo);
    return fileinfo.st_size;
}

static void process1(char *from, char *to) {
    std::reverse(from, to);
}

int main() {
    const auto buffer_size = get_buffer_capacity();
    auto buffer = (char*) mmap (NULL, buffer_size+1, PROT_READ|PROT_WRITE,
             MAP_PRIVATE|MAP_POPULATE|MAP_ANONYMOUS, -1, 0);
    auto in = fileno(stdin);

    auto t0 = realtime_now();
    read(in, &buffer[0], buffer_size);
    auto t1 = realtime_now();
    printf("read time: %zu ms\n", (t1-t0)/1'000'000);

    buffer[buffer_size] = '>';
    t0 = realtime_now();

    auto last = buffer_size;
    auto *from = &buffer[0], *to = &buffer[last];
    while (from < &buffer[last]) {
        from = (char*)memchr(from, '\n', &buffer[last] - from + 1) + 1;
        to = (char*)memchr(from, '>', &buffer[last] - from + 1);
        process1(from, to - 1);
        from = to;
    }

    t1 = realtime_now();
    printf("process time: %zu ms\n", (t1-t0)/1'000'000);
    t0 = realtime_now();

    write(fileno(stdout), buffer, buffer_size);

    t1 = realtime_now();
    printf("write time: %zu ms\n", (t1-t0)/1'000'000);
    munmap(buffer, buffer_size+1);
    return 0;
}
