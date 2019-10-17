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
#include <thread>
#include <cstdio>

static char *buffer;
static std::array<char, 128> rev_complements1;
static std::array<unsigned short, 256*256> rev_complements;
constexpr auto margin = 60u;

static void preprocess_rev_complements() {
    auto lower = [](auto c){
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    };
    auto upper = [](auto c){
        return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    };
    constexpr auto pairs1 = "ATCGGCTAUAMKRYWWSSYRKMVBHDDHBVNN\n\n";
    for (const auto *s = pairs1; *s; s += 2) {
        rev_complements1[upper(s[0])] = s[1];
        rev_complements1[lower(s[0])] = s[1];
    }
    constexpr auto pairs = "CGGCATCGGCTAUAMKRYWWSSYRKMVBHDDHBVNN\n\n";
    const auto length = std::strlen(pairs);
    for (auto i = 0u; i < length; i += 2u) {
        for (auto j = 0u; j < length; j += 2u) {
            alignas(2) std::array froms = {pairs[i], pairs[j],
                        lower(pairs[i]), pairs[j],
                        pairs[i], lower(pairs[j]),
                        lower(pairs[i]), lower(pairs[j]),

                        pairs[j], pairs[i],
                        lower(pairs[j]), pairs[i],
                        pairs[j], lower(pairs[i]),
                        lower(pairs[j]), lower(pairs[i])};
            alignas(2) std::array tos = {pairs[j+1], pairs[i+1]};

            auto to = *reinterpret_cast<short*>(&tos[0]);
            for (auto k = 0u; k < froms.size()/2u; k += 2u) {
                auto *from = reinterpret_cast<short*>(&froms[k]);
                rev_complements[*from] = to;
            }

            std::swap(tos[0], tos[1]);
            to = *reinterpret_cast<short*>(&tos[0]);
            for (auto k = froms.size()/2u; k < froms.size(); k += 2u) {
                auto *from = reinterpret_cast<short*>(&froms[k]);
                rev_complements[*from] = to;
            }
        }
    }
}

#define 	TRUNC(addr, align)   (((size_t)addr) & ~(((size_t)align) - 1))
#define 	ALIGN_DOWN(addr, align)   TRUNC(addr, align)
#define     ALIGN_UP(addr, align)   ( ((( size_t)addr) + (align) - 1) & (~((align) - 1)) )

static void fast_reverse_complements(short *from, short *to) {
    auto *end = to;
    auto *begin = from;
    to = reinterpret_cast<short*>ALIGN_DOWN(to, 2);
    auto *to2 = to;
    from = reinterpret_cast<short*>ALIGN_UP(from, 2);
    auto *from2 = from;
    while (from <= to) {
         auto tmp = *from;
         *from = rev_complements[*to];
         *to =  rev_complements[tmp];
         from++;
         to--;
     }
     auto *beginc = reinterpret_cast<char*>(begin);
     auto *endc = reinterpret_cast<char*>(end);
     if (end != to2 && begin != from2) {
         auto front = *beginc;
         *beginc = rev_complements1[*(endc + 1)];
         *(endc + 1) = rev_complements1[front];
     } else if (begin != from2) {
         auto front = *beginc;
         std::memmove(beginc, beginc + 1, endc - beginc + 1);
         *(endc + 1) = rev_complements1[front];
     } else if (end != to2) {
         auto last = *(endc + 1);
         std::memmove(beginc + 1, beginc, endc - beginc + 1);
         *beginc = rev_complements1[last];
     }
}

static void process(char *from, char *to) {
    while (*from++ != '\n');
    const auto length = to - from;
    const auto offset = margin - (length % (margin+1));
    if (offset) {
        for (auto m = from + margin - offset; m < to; m += (margin+1)) {
            std::memmove(m + 1, m, offset);
            *m = '\n';
        }
    }
    to--;
    fast_reverse_complements(reinterpret_cast<short*>(from),
                             reinterpret_cast<short*>(to-1));
}

static unsigned find_proper_separator(unsigned last) {
    auto *found = std::strchr(&buffer[last], '>');
    return found - &buffer[0];
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

static void reverse_complement_process() {
    preprocess_rev_complements();
    const auto buffer_size = get_buffer_capacity();
    buffer = new char[buffer_size + 1];

    auto in = fileno(stdin);

    auto t0 = realtime_now();

    read(in, &buffer[0], buffer_size);

    auto t1 = realtime_now(); //printf("read time: %zu ms\n", (t1-t0)/1'000'000);

    buffer[buffer_size] = '>';
    const auto max_threads_number = (buffer_size > 1'000'000u)?
                std::thread::hardware_concurrency() : 1u;
    const auto range = buffer_size/max_threads_number;

    t0 = realtime_now();
    auto first = 0u, last = find_proper_separator(range - 1u);
    std::vector<std::thread> threads;

    for (auto i = 1u; i <= max_threads_number && first < buffer_size; i++) {
        threads.emplace_back([first, last](){
            //printf("%u: %u %u\n", i, first, last);
            auto *from =  &buffer[first], *to = &buffer[last - 1u];
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
