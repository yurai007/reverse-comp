#include <utility>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <sys/stat.h>

#include <cstdint>
#include <ctime>
#include <unistd.h>
#include <cctype>

static char *buffer;
static auto buffer_size = 0u;
static char upper_complements[128];
constexpr auto margin = 60u;

static void init_upper_complements() {
    constexpr auto pairs = "ATCGGCTAUAMKRYWWSSYRKMVBHDDHBVNN\n\n";
    for (const auto *s = pairs; *s; s += 2) {
        upper_complements[std::toupper(s[0])] = s[1];
        upper_complements[std::tolower(s[0])] = s[1];
    }
}

static unsigned get_buffer_capacity() {
    struct stat fileinfo;
    fstat(fileno(stdin), &fileinfo);
    return fileinfo.st_size;
}

static inline uint64_t realtime_now() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1'000'000'000ULL + ts.tv_nsec;
}

static void process(char *from, char *to) {
    while (*from++ != '\n');

    auto len = to - from;
    auto off = margin - (len % (margin+1));

    if (off) {
        for (auto m = from + margin - off; m < to; m += (margin+1)) {
            memmove(m + 1, m, off);
            *m = '\n';
        }
    }
    char tmp;
    for (to--; from <= to; from++, to--)
       tmp = upper_complements[(int)*from], *from = upper_complements[(int)*to], *to = tmp;
}

static void reverse_complement_process() {
    init_upper_complements();
    buffer_size = get_buffer_capacity();
    buffer = new char[buffer_size + 1];

    auto in = fileno(stdin);

    auto t0 = realtime_now();

    auto read_chunk = read(in, &buffer[0], buffer_size);
    assert(read_chunk == buffer_size);

    auto t1 = realtime_now(); printf("read time: %zu ms\n", (t1-t0)/1'000'000);

    buffer[buffer_size] = '>';

    t0 = realtime_now();

    auto *from =  &buffer[0], *to = &buffer[buffer_size -1];
    while (to >= buffer) {
       for (from = to; *from != '>'; from--);
       process(from, to);
       to = from - 1;
    }

    t1 = realtime_now(); printf("process time: %zu ms\n", (t1-t0)/1'000'000);
    t0 = realtime_now();

    write(fileno(stdout), buffer, buffer_size);
     t1 = realtime_now(); printf("write time: %zu ms\n", (t1-t0)/1'000'000);

    delete[] buffer;

}

int main() {
    reverse_complement_process();
    return 0;
}
