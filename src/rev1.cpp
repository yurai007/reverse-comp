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

constexpr auto margin = 60u;

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

int main() {
    const auto buffer_size = get_buffer_capacity();
    auto buffer = new char[buffer_size + 1];
    auto in = fileno(stdin);

    auto t0 = realtime_now();
    read(in, &buffer[0], buffer_size);
    auto t1 = realtime_now();
    //printf("read time: %zu ms\n", (t1-t0)/1'000'000);

    buffer[buffer_size] = '>';

    t0 = realtime_now();
    auto last = buffer_size;
    auto *from = &buffer[0], *to = &buffer[last];
    while (from < &buffer[last]) {
        from = strchr(from, '\n')+1;
        to = strchr(from, '>');
        std::reverse(from, to-1);
        from = to;
    }

    t1 = realtime_now();
    t0 = realtime_now();

    write(fileno(stdout), buffer, buffer_size);
    t1 = realtime_now();
    //printf("write time: %zu ms\n", (t1-t0)/1'000'000);
    delete[] buffer;
    return 0;
}
