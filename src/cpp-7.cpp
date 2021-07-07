// The Computer Language Benchmarks Game
// https://salsa.debian.org/benchmarksgame-team/benchmarksgame/
//
// contributed by roman blog

#include<boost/hana.hpp>
#include<limits>
#include<array>
#include<sys/mman.h>
#include<unistd.h>
#undef NDEBUG
#include<cassert>
#include<filesystem>
#include<string_view>
#include<vector>
#include<fcntl.h>

#include<chrono>
#include<compare>
#include<sys/sendfile.h>
#include<string.h>

// --dj Just for hana literals and llong_c
namespace hana = boost::hana;
// --dj just for fs::path ?
namespace fs = std::filesystem;

using namespace hana::literals;

using sv = std::string_view;
using namespace std::literals;
using hana::_;

namespace {
constexpr uint8_t swmap(uint8_t c) {
  switch(c) {
    case 'A': case 'a': return 'T';// 'A' | 'a' => 'T',
    case 'C': case 'c': return 'G';// 'C' | 'c' => 'G',
    case 'G': case 'g': return 'C';// 'G' | 'g' => 'C',
    case 'T': case 't': return 'A';// 'T' | 't' => 'A',
    case 'U': case 'u': return 'A';// 'U' | 'u' => 'A',
    case 'M': case 'm': return 'K';// 'M' | 'm' => 'K',
    case 'R': case 'r': return 'Y';// 'R' | 'r' => 'Y',
    case 'W': case 'w': return 'W';// 'W' | 'w' => 'W',
    case 'S': case 's': return 'S';// 'S' | 's' => 'S',
    case 'Y': case 'y': return 'R';// 'Y' | 'y' => 'R',
    case 'K': case 'k': return 'M';// 'K' | 'k' => 'M',
    case 'V': case 'v': return 'B';// 'V' | 'v' => 'B',
    case 'H': case 'h': return 'D';// 'H' | 'h' => 'D',
    case 'D': case 'd': return 'H';// 'D' | 'd' => 'H',
    case 'B': case 'b': return 'V';// 'B' | 'b' => 'V',
    case 'N': case 'n': return 'N';// 'N' | 'n' => 'N',
    default: return '_';
  }
}

// for 2B replacements, LUT computed in complile time? --dj
// using hana: https://www.boost.org/doc/libs/1_61_0/libs/hana/doc/html/index.html
constexpr auto map = ([] {
  constexpr auto max = std::numeric_limits<uint8_t>::max() + size_t{1};
  std::array<uint16_t, max * max> map{};
  for(size_t it = 0; it < map.size(); ++it) {
    uint8_t hi = (it >> 8), lo = it;
    map[it] = (swmap(lo) << 8) | (swmap(hi));
  }
  return map;
})();

constexpr auto map256 = ([] {
  constexpr auto max = std::numeric_limits<uint8_t>::max() + size_t{1};
  std::array<uint8_t, max> map{};
  for(size_t it = 0; it < max; ++it)
    map[it] = swmap(it);
  return map;
})();

template<size_t noffset> void replace60(const char * in, char * out) {
  constexpr auto offset = hana::llong_c<noffset>;

  auto op = [&] {
    *(uint16_t *)out = map[*(const uint16_t *)(in -= 2)];
    out += 2;
  };

  auto tail_size = ((60_c - offset) / 2_c);
  tail_size.times(op);

  if constexpr(offset % 2_c) {
//   ...1\n
//   0...
    *out++ = map256[*(--in)];
    --in;
//     assert(*in == '\n');
    *out++ = map256[*(--in)];
    (29_c - tail_size).times(op);
  } else {// even
//   ...\n
//   ...
    in -= 1;
//     assert(*in == '\n');
    (30_c - tail_size).times(op);
  }
  *(out++) = '\n';
}
}

struct range{
  size_t begin{}, size{};
  auto operator<=>(const range &) const = default;
};

auto select_replace60 = [](range r) {
  constexpr static auto replace60_map = ([] {
    std::array<decltype(replace60<0>) *, 60> map{};
    (60_c).times.with_index([&](auto index) {
      map[index()] = replace60<index()>;
    });
    return map;
  })();
  return replace60_map.at(60 - (r.size % 61));
};


/* pread/pwrite - take extra file offset from where it will read/write to.
                It may be good for random read/write operations.

The pread() and pwrite() system calls are especially useful in
multithreaded applications.  They allow multiple threads to
perform I/O on the same file descriptor without being affected by
changes to the file offset by other threads.

Should I expect 20% speed up with > 1 thread?
    https://stackoverflow.com/questions/5057737/simultaneous-read-on-file-descriptor-from-two-threads


high_resolution_clock
sendfile
*/


void replace(int fd, range r) {
  auto op = select_replace60(r);
  constexpr size_t line_size = 61;
  constexpr size_t block_size = line_size * 1024;
  char buf[block_size]{};
  char outbuf[block_size]{};
  auto nblock = r.size / block_size;
  auto tail = r.size - (nblock * block_size);

  for(size_t n = 1; n <= nblock; ++n) {
    pread(fd, buf, block_size, r.begin + r.size - n * block_size);
    auto it = std::end(buf), oit = std::begin(outbuf), oend = std::end(outbuf);
    while(oit < oend) {
      op(it, oit); it -= line_size; oit += line_size;
    }
    write(STDOUT_FILENO, outbuf, block_size);
  }

  pread(fd, buf, tail, r.begin);
  auto it = std::begin(buf) + tail, oit = std::begin(outbuf);

  for(size_t n = 0; n < tail / line_size; ++n) {
    op(it, oit); it -= line_size; oit += line_size;
  }

  for(size_t n = 0; n < (tail - (tail / line_size) * line_size); ++n) {
    *oit++ = map256[*(--it)];
  }

  write(STDOUT_FILENO, outbuf, tail);
  write(STDOUT_FILENO, "\n", 1);
}


auto find_first_of(int fd, char c, size_t pos, size_t & endfile) {
  constexpr size_t block_size = 1024 * 32;
  uint8_t mem[block_size]{};
  if(pos == sv::npos) return pos;
  while(true) {
    auto bytes = pread(fd, mem, block_size, pos);
    assert(bytes >= 0);
    if(!bytes) { endfile = pos; return sv::npos; }
    auto r = sv{(const char *)mem, size_t(bytes)}.find_first_of(c);
    if(r != sv::npos) return pos + r;
    pos += bytes;
  }
}

int main() {
  fs::path path{"/dev/stdin"};
  int fd = open(path.c_str(), O_RDONLY);
  assert(fd != -1);
  auto start = std::chrono::high_resolution_clock::now();


  auto next = [=, prev = 0ul]() mutable -> std::pair<range, range> {
    size_t endfile{};
    auto arrow_pos = find_first_of(fd, '>', prev, endfile);
    auto begin_pos = find_first_of(fd, '\n', arrow_pos, endfile);
    if(begin_pos == sv::npos) return {};
    prev = find_first_of(fd, '>', begin_pos, endfile);
    prev = (prev == sv::npos) ? endfile : prev;
    return {{arrow_pos, begin_pos - arrow_pos + 1}, {begin_pos + 1, prev - begin_pos - 2}};
  };

  std::vector<std::pair<range, range>> index;
  for(auto pair = next(); pair != std::pair<range, range>{}; pair = next()) index.emplace_back(pair);

//   fprintf(stderr, "%.3f\n", std::chrono::duration<double>{std::chrono::high_resolution_clock::now() - start}.count());

  start = std::chrono::high_resolution_clock::now();

  for(auto [h, q]: index) {
    off_t begin = h.begin;
    sendfile(STDOUT_FILENO, fd, &begin, h.size);
    replace(fd, q);
  };

//   fprintf(stderr, "%.3f\n", std::chrono::duration<double>{std::chrono::high_resolution_clock::now() - start}.count());
}
