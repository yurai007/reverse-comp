#include <utility>
#include <cstdio>
#include <cassert>
#include <cstring>
#include <sys/stat.h>
#include <cstdint>
#include <ctime>
#include <unistd.h>
#include <cctype>
#include <array>
#include <vector>
#include <thread>

static char *buffer;
static char rev_complements1[128];
unsigned short rev_complements[256*256];
constexpr auto margin = 60u;

static  char mytolower(char ch) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
}

static auto init() {
    constexpr auto pairs1 = "ATCGGCTAUAMKRYWWSSYRKMVBHDDHBVNN\n\n";
    for (const auto *s = pairs1; *s; s += 2) {
        rev_complements1[std::toupper(s[0])] = s[1];
        rev_complements1[std::tolower(s[0])] = s[1];
    }
    constexpr auto pairs = "CGGCATCGGCTAUAMKRYWWSSYRKMVBHDDHBVNN\n\n";
    const auto len = std::strlen(pairs);
    for (auto i = 0u; i < len; i += 2) {
        for (auto j = 0u; j < len; j += 2) {
        alignas(2) std::array froms = {pairs[i], pairs[j],
                    mytolower(pairs[i]), pairs[j],
                    pairs[i], mytolower(pairs[j]),
                    mytolower(pairs[i]), mytolower(pairs[j]),

                    pairs[j], pairs[i],
                    mytolower(pairs[j]), pairs[i],
                    pairs[j], mytolower(pairs[i]),
                    mytolower(pairs[j]), mytolower(pairs[i])};
        alignas(2) std::array tos = {pairs[j+1], pairs[i+1]};

        auto to = *reinterpret_cast<short*>(&tos[0]);
        for (auto k = 0u; k < froms.size()/2u; k += 2) {
            auto *from = reinterpret_cast<short*>(&froms[k]);
            rev_complements[*from] = to;
        }

        std::swap(tos[0], tos[1]);
        to = *reinterpret_cast<short*>(&tos[0]);
        for (auto k = froms.size()/2u; k < froms.size(); k += 2) {
            auto *from = reinterpret_cast<short*>(&froms[k]);
            rev_complements[*from] = to;
        }
    }
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

#define 	TRUNC(addr, align)   (((size_t)addr) & ~(((size_t)align) - 1))
#define 	ALIGN_DOWN(addr, align)   TRUNC(addr, align)
#define     ALIGN_UP(addr, align)   ( ((( size_t)addr) + (align) - 1) & (~((align) - 1)) )

static void fast_rev_comp(short *from, short *to) {
    short *end = to;
    short *begin = from;
    to = (short*)ALIGN_DOWN(to, 2);
    short *to2 = to;
    from = (short*)ALIGN_UP(from, 2);
    short *from2 = from;
    assert((((size_t)to & 1) == 0));
    assert((((size_t)from & 1) == 0));
     while (from <= to) {
         short tmp = *from;
         *from = rev_complements[*to];
         *to =  rev_complements[tmp];
         from++;
         to--;
     }
     if (end != to2 && begin != from2) {
         char front = *((char*)begin);
         *((char*)begin) = rev_complements1[*((char*)end + 1)];
         *((char*)end + 1) = rev_complements1[front];
     } else if (begin != from2) {
         char front = *((char*)begin);
         memmove(((char*)begin), ((char*)begin) + 1, (char*)end - (char*)begin + 1);
         *((char*)end + 1) = rev_complements1[front];
     } else if (end != to2) {
         char last = *((char*)end+1);
         memmove(((char*)begin) + 1, (char*)begin, (char*)end - (char*)begin + 1);
         *((char*)begin) = rev_complements1[last];
     }
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
    to--;
    fast_rev_comp(reinterpret_cast<short*>(from), reinterpret_cast<short*>(to-1));
}

static unsigned find_proper_separator(unsigned last) {
    auto *found = std::strchr(&buffer[last], '>');
    return found - &buffer[0];
}

static void reverse_complement_process() {
    init();
    const auto buffer_size = get_buffer_capacity();
    buffer = new char[buffer_size + 1];

    auto in = fileno(stdin);

    auto t0 = realtime_now();

    auto read_chunk = read(in, &buffer[0], buffer_size);
    assert(read_chunk == buffer_size);

    auto t1 = realtime_now(); //printf("read time: %zu ms\n", (t1-t0)/1'000'000);

    buffer[buffer_size] = '>';
    const auto threads_number = (buffer_size > 1'000'000)? std::thread::hardware_concurrency() : 1u;
    const auto range = buffer_size/threads_number;

    t0 = realtime_now();
    auto first = 0u;
    auto last = find_proper_separator(range - 1u);
    std::vector<std::thread> threads;

    for (auto i = 1u; i <= threads_number && first < buffer_size; i++) {
        threads.emplace_back([first, last, i](){
            //printf("%u: %u %u\n", i, first, last);
            auto *from =  &buffer[first], *to = &buffer[last - 1];
            while (to >= &buffer[first]) {
                for (from = to; *from != '>'; from--);
                process(from, to);
                to = from - 1;
            }
        });
        first = last;
        last = find_proper_separator(std::min((i+1u)*range - 1u, buffer_size));
    }

    for (auto&& t : threads)
        t.join();

    t1 = realtime_now(); //printf("process time: %zu ms\n", (t1-t0)/1'000'000);
    t0 = realtime_now();

    write(fileno(stdout), buffer, buffer_size);
    t1 = realtime_now(); //printf("write time: %zu ms\n", (t1-t0)/1'000'000);

    delete[] buffer;
}

int main() {
    reverse_complement_process();
    return 0;
}
