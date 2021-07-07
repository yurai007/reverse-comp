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
#include <cstdlib>
#include <cassert>
#include<sys/sendfile.h>

/*
Rust:

real	0m0.976s
user	0m0.371s
sys	0m0.863s

tac is ~1.5s but it just reverse line by line (first line become last)!
      Anyway interesting approach by seeking every 64k from end.
*/

static unsigned get_file_size() {
    struct stat fileinfo;
    fstat(fileno(stdin), &fileinfo);
    return fileinfo.st_size;
}

/*
    Fastest possible I/O - faster than cp!
    It use sendfile and transfer files entirely in kernel space so it's fully
    'copy-free' approach from user POV.

    real	0m0.539s
    user	0m0.000s
    sys	0m0.538s

    * ref: https://lwn.net/Articles/659523/ (2015)

    * copy_file_range - "accalarated" in-kernel space files copying

       - ssize_t copy_file_range(int fd_in, loff_t *off_in, int fd_out, loff_t *off_out,
                                 size_t len, unsigned int flags);
         flags are
            COPY_FR_COPY - copy the data normally, accelerating the work at the FS level if possible.
            COPY_FR_REFLINK - "copy" lazily (like Copy-On-Write) - asks for the destination file to refer to
                         the existing copy of the data without actually copying it.
                         Some FS (Btrfs, for example) are able to share references to file blocks in this way.
            COPY_FR_DEDUP is similar to COPY_FR_REFLINK with some details

       - COPY_FR_COPY if there is no FS-level accelaration fallback to splice syscall.
         By default it just copies references by COPY_FR_REFLINK.

       - offsets are pointers because we need to distinguish NULL from zero.

       - limitations - source and destination must be on same FS.

       - it may introduce same problem as "sendfile": If an error is returned it's unclear
         if it was while reading from the source or writing to the destination
         (or internally in the function itself).

    * "How useful should copy_file_range() be?" ref: https://lwn.net/Articles/846403/ (2021)

       - merged to kernel 4.5

       - lot of fixes, improvements. Since kernel 5.3 copy_file_range works with src located
            on different FS than dst. But it triggers problems (described below) so they want to
            go back to version without 2 FS support.

       - problems when working with 0 size file (procfs/sysfs) and long discussion
         between kernel devs and golang devs

       - copy_file_range() will never be a generic file-copy mechanism, but it will at least
         fail with an explicit error, so user space will know that it must copy the data some other way.

       - There is currently no way to distinguish between copies that were cut short on the read side
         (by hitting the end of the file, perhaps) and those that were stopped on the write side
         (which may well indicate a write error).

         TODO: comments :)

    * another FS-related - double close() on fd returns error, that's why sometimes it's wort to check
      error code from close().
*/
int main00() {
    auto bytes = get_file_size();
    auto written = sendfile(STDOUT_FILENO, STDIN_FILENO, 0, bytes);
    assert(written == bytes);
    return 0;
}

/*
    Fastest possible I/O (read+write) if data need to touch userland.
    It's ~0.6s like cp.
    One big read to big buffer + one big write from buffer is only 0.9 s because of
    big virtual WS overhead.
    The best approach is to read and write chunk by chunk with 64KB size like here.
    Then 0.9s -> 0.6s.
    Notice that with small chunks like 256B it's terribly slow.
    So big allocations/big WS ~1GB cost ~0.3s.

    real	0m0.575s
    user	0m0.000s
    sys	0m0.574s
*/

int main0() {
    const auto buffer_size = 1<<16;
    auto buffer = (char*) mmap (NULL, buffer_size+1, PROT_READ|PROT_WRITE,
             MAP_PRIVATE|MAP_POPULATE|MAP_ANONYMOUS, -1, 0);
    assert(buffer != MAP_FAILED);
    size_t size = buffer_size;
    while ((size = read(fileno(stdin), buffer, buffer_size)) != 0) {
        write(fileno(stdout), buffer, size);
    }
    munmap(buffer, buffer_size+1);
    return 0;
}

/*
   We populate buffer every buffer_size like ring buffer.
   Following times for alloc_size (WS):
  (1G)1<<30      0m0.840s
      1<<29      0m0.735s
      1<<28      0m0.690s
      1<<27      0m0.663s
      ...
      1<<23      0m0.635s
 */
int main1() {
    const auto buffer_size = 1<<16;
    const auto alloc_size = 1<<29;
    auto buffer = (char*) mmap (NULL, alloc_size+1, PROT_READ|PROT_WRITE,
             MAP_PRIVATE|MAP_POPULATE|MAP_ANONYMOUS, -1, 0);
    assert(buffer != MAP_FAILED);
    size_t size = buffer_size;
    auto current = buffer;
    while ((size = read(fileno(stdin), current, buffer_size)) != 0) {
        write(fileno(stdout), current, size);
        current += size;
        if ((current - buffer) + buffer_size >= alloc_size)
            current = buffer;
    }
    munmap(buffer, alloc_size+1);
    return 0;
}

static inline uint64_t realtime_now() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1'000'000'000ULL + ts.tv_nsec;
}

/*
  * Std::reverse chunk after chunk - still ~0.6s. Sys ~0.59s.
  * CHANGELOG:
    -  mmap 128k + std::reverse every 128k
  * By tuning alloc_size + step we see the best approach is to work with buffer (alloc_size)
    which fit to L2/L3.
    It doesn't matter we perform tousands of std::reverse.
    If we mmap WS >> L3 it add extra ~0.1s. If we use this memory it's additional 0.1s,
    so we get 0.8s.
 */
int main2() {
    constexpr auto buffer_size = 1<<16;
    constexpr auto alloc_size = (1<<17);
    constexpr auto step = (1<<17) - buffer_size;

    auto buffer = (char*) mmap (NULL, alloc_size+1, PROT_READ|PROT_WRITE,
             MAP_PRIVATE|MAP_POPULATE|MAP_ANONYMOUS, -1, 0);
    assert(buffer != MAP_FAILED);
    auto size = buffer_size;
    auto read_bytes = 0u, all = 0u, reverses = 0u;
    while ((size = read(fileno(stdin), buffer + read_bytes, buffer_size)) != 0) {
        read_bytes += size;
        if (read_bytes >= step || size < buffer_size) {
            std::reverse(buffer, buffer + read_bytes);
            write(fileno(stdout), buffer, read_bytes);
            all += read_bytes;
            read_bytes = 0;
            reverses++;
        }
    }
    munmap(buffer, alloc_size+1);
    printf("summary:    %u B    %u\n", all, reverses);
    return 0;
}

/*
 ~ 0m0.830s
  * CHANGELOG:
    -  mmap 400m + std::reverse every 400m (assuming it's biggest group)
 */
int main3() {
    constexpr auto buffer_size = 1<<16;
    constexpr auto alloc_size = (1<<28) + (1<<27);
    constexpr auto step = alloc_size - buffer_size;

    auto buffer = (char*) mmap (NULL, alloc_size+1, PROT_READ|PROT_WRITE,
             MAP_PRIVATE|MAP_POPULATE|MAP_ANONYMOUS, -1, 0);
    assert(buffer != MAP_FAILED);
    auto size = buffer_size;
    auto read_bytes = 0u, all = 0u, reverses = 0u;
    while ((size = read(fileno(stdin), buffer + read_bytes, buffer_size)) != 0) {
        read_bytes += size;
        if (read_bytes >= step || size < buffer_size) {
            std::reverse(buffer, buffer + read_bytes);
            write(fileno(stdout), buffer, read_bytes);
            all += read_bytes;
            read_bytes = 0;
            reverses++;
        }
    }
    munmap(buffer, alloc_size+1);
    printf("summary:    %u B    %u\n", all, reverses);
    return 0;
}

/*
 ~ 0m0.849s
   sys: 0m0.755s
  * CHANGELOG:
    - std::reverse every 400m (assuming it's biggest group)
    - growing remapped buffer from 128k to 512m.
      using malloc + growing realloc'd memory gives extra ~40ms over mmap + mremap.
  * hugepages - gives 60ms boost

    real	0m0.789s
    user	0m0.087s
    sys	0m0.701s

  * hugepages are very important in DPDK for packet buffers
    ref: https://doc.dpdk.org/guides/linux_gsg/sys_reqs.html#use-of-hugepages-in-the-linux-environment
 */
int main4() {
    constexpr auto buffer_size = 1u<<16u;
    auto alloc_size = buffer_size<<1u;
    constexpr auto max_alloc_size = (1<<29);
    constexpr auto step = (1<<28) + (1<<27) - buffer_size;
    auto buffer = (char*) mmap (NULL, alloc_size, PROT_READ|PROT_WRITE,
             MAP_PRIVATE|MAP_POPULATE|MAP_ANONYMOUS, -1, 0);

    assert(buffer != MAP_FAILED);
    auto size = buffer_size;
    auto read_bytes = 0u, all = 0u, reverses = 0u;
    while ((size = read(fileno(stdin), buffer + read_bytes, buffer_size)) != 0) {
        read_bytes += size;

        if (read_bytes >= step || size < buffer_size) {
            std::reverse(buffer, buffer + read_bytes);
            write(fileno(stdout), buffer, read_bytes);
            all += read_bytes;
            read_bytes = 0;
            reverses++;
        }
        if (read_bytes + buffer_size >= alloc_size) {
            auto old_alloc_size = alloc_size;
            alloc_size = static_cast<int>(alloc_size * 2.0f);
            buffer = (char*)mremap(buffer, old_alloc_size, alloc_size, MREMAP_MAYMOVE); //MREMAP_FIXED
            assert(buffer != MAP_FAILED );
            assert(alloc_size <= max_alloc_size);
        }
    }
    munmap(buffer, alloc_size);
    return 0;
}

/*
    real	0m1.087s
    user	0m0.177s
    sys	    0m0.909s

    * CHANGELOG:
      - everything done out of the box by getdelim - finding group + malloc + realloc
 */
int main5() {
    size_t buffer_size = 0;
    ssize_t size = buffer_size;
    auto reverses = 0u;
    char *current = nullptr;
    while ((size = getdelim(&current, &buffer_size, '>', stdin)) > 0) {
        std::reverse(current, current + size);
        write(fileno(stdout), current, size);
        reverses++;
    }
    free(current);
    return 0;
}

/*
    real	0m0.937s
    user	0m0.107s
    sys	0m0.823s

    * CHANGELOG:
      - based on main4 - proper group detection + std::reverse per group with growing buffer
      - extra memchr costs 937ms-850ms = ~87ms.
      - not sure if there is possibility to parallize anything
      - works ONLY for input > buffer_size, it reverses whole group together with headline '>'

    General - moving files to tmpfs speed up things a lot!
    With combo hugetlbs + tmpfs it's possible to get massive boost reducing sys to only 0.5s
    and whole time with ~0.3s!

    real	0m0.603s
    user	0m0.103s
    sys	0m0.499s
 */
int main6() {
    constexpr auto buffer_size = 1u<<16u;
    auto alloc_size = 1u<<17u;
    constexpr auto max_alloc_size = (1<<29);
    auto buffer = (char*) mmap (NULL, alloc_size, PROT_READ|PROT_WRITE,
             MAP_PRIVATE|MAP_POPULATE|MAP_ANONYMOUS, -1, 0);

    assert(buffer != MAP_FAILED);
    auto size = buffer_size;
    auto read_bytes = 0u, all = 0u, reverses = 0u;
    char *found = nullptr;
    int first = -1, last = -1;

    while ((size = read(fileno(stdin), buffer + read_bytes, buffer_size)) != 0) {
        found = (char*)memchr(buffer + read_bytes, '>', size);
        if (found) {
            if (first == -1)
                first = found - buffer;
            if (last == -1) {
                if (first == found - buffer) {
                    auto lastp = (char*)memchr(buffer + first + 1, '>', size - (found - buffer));
                    last = (lastp != nullptr)? (lastp - buffer) : -1;
                }
                else
                    last = found - buffer;
            }
        }
        read_bytes += size;

        if ((first != -1 && last != -1) || size < buffer_size) {
            if (last == -1)
                last = read_bytes;
            std::reverse(buffer + first, buffer + last);
            write(fileno(stdout), buffer + first, last - first);
            all += read_bytes;
            memmove(buffer, buffer + last, read_bytes - last);
            read_bytes -= last;
            last = 0;
            reverses++;
        }

        if (read_bytes + buffer_size >= alloc_size) {
            auto old_alloc_size = alloc_size;
            alloc_size = static_cast<int>(alloc_size * 2.0f);
            buffer = (char*)mremap(buffer, old_alloc_size, alloc_size, MREMAP_MAYMOVE); //MREMAP_FIXED
            assert(buffer != MAP_FAILED );
            assert(alloc_size <= max_alloc_size);
        }
        if (last != -1) {
            first = last;
            last = -1;
        }
    }
    munmap(buffer, alloc_size);
    return 0;
}

#include <fcntl.h>

/*
 * as above but with debug logs
 */
int main7() {
    constexpr auto buffer_size = 1u<<16u;
    auto alloc_size = 1u<<17u;
    constexpr auto max_alloc_size = (1<<29);
    auto buffer = (char*) mmap (NULL, alloc_size, PROT_READ|PROT_WRITE,
             MAP_PRIVATE|MAP_POPULATE|MAP_ANONYMOUS, -1, 0);

    assert(buffer != MAP_FAILED);
    auto size = buffer_size;
    auto read_bytes = 0u, all = 0u, reverses = 0u;
    char *found = nullptr;
    int first = -1, last = -1;

    //int fd = open("insmall.txt",  O_RDONLY);
    //assert(fd != -1);

    while ((size = read(fileno(stdin), buffer + read_bytes, buffer_size)) != 0) { //fd
        found = (char*)memchr(buffer + read_bytes, '>', size);
        //if (found)
        //    printf("found= %lu\n", found - buffer);
        if (found) {
            if (first == -1)
                first = found - buffer;
            if (last == -1) {
                if (first == found - buffer) {
                    auto lastp = (char*)memchr(buffer + first + 1, '>', size - (found - buffer));
                    last = (lastp != nullptr)? (lastp - buffer) : -1;
                }
                else
                    last = found - buffer;
                //if (last != -1)
                //    printf("found 2= %d\n", last);
            }
        }
        //printf("first= %d last = %d\n", first, last);
        read_bytes += size;

        if ((first != -1 && last != -1) || size < buffer_size) {
            if (last == -1)
                last = read_bytes;
            //printf("rev:  %d  %d\n", first, last);
            all += read_bytes;
            //printf("size=%u  rb=%u  memcpy= %u, all = %u\n", size, read_bytes,  read_bytes - last, all);
            std::reverse(buffer + first, buffer + last);
            write(fileno(stdout), buffer + first, last - first);

            memmove(buffer, buffer + last, read_bytes - last);
            read_bytes -= last;
            last = 0;
            reverses++;
        }

        if (read_bytes + buffer_size >= alloc_size) {
            auto old_alloc_size = alloc_size;
            alloc_size = static_cast<int>(alloc_size * 2.0f);
            //printf("alloc to %u\n", alloc_size);
            buffer = (char*)mremap(buffer, old_alloc_size, alloc_size, MREMAP_MAYMOVE); //MREMAP_FIXED
            assert(buffer != MAP_FAILED );
            assert(alloc_size <= max_alloc_size);
        }
        if (last != -1) {
            first = last;
            last = -1;
        }
    }
    //close(fd);
    munmap(buffer, alloc_size);
    return 0;
}

/*
 * Forward 'grep', nice 10GB/s

   real	0m0.118s
   user	0m0.007s
   sys	0m0.111s
 */
int main8() {
    const auto buffer_size = 1<<16;
    auto buffer = (char*) mmap (NULL, buffer_size+1, PROT_READ|PROT_WRITE,
             MAP_PRIVATE|MAP_POPULATE|MAP_ANONYMOUS, -1, 0);
    assert(buffer != MAP_FAILED);
    size_t size = buffer_size;
    auto all = 0u;
    while ((size = read(fileno(stdin), buffer, buffer_size)) != 0) {
       auto found = (char*)memchr(buffer, '>', size);
       if (found)
           printf("pos = %u\n", all + unsigned(found - buffer));
       all += size;
    }
    munmap(buffer, buffer_size+1);
    return 0;
}

/*
 * Backward 'grep' with pread approach. Still 10GB/s.

   real	0m0.118s
   user	0m0.007s
   sys	0m0.111s
*/
int main9() {
    const auto buffer_size = 1<<16;
    auto buffer = (char*) mmap (NULL, buffer_size+1, PROT_READ|PROT_WRITE,
             MAP_PRIVATE|MAP_POPULATE|MAP_ANONYMOUS, -1, 0);
    assert(buffer != MAP_FAILED);
    size_t size = buffer_size;
    off_t start_from = get_file_size() - buffer_size;
    while ((size = pread(fileno(stdin), buffer, buffer_size, start_from))) {
       auto found = (char*)memchr(buffer, '>', size);
       if (found)
           printf("pos = %u\n", unsigned(start_from) + unsigned(found - buffer));
       start_from -= size;
       if (size == (size_t)-1)
           break;
    }
    munmap(buffer, buffer_size+1);
    return 0;
}

/*
 * Reversing group by group with pread approach.
   It's 0.2s faster than main6! We work with small 64KB buffer.
   Thanks to pread we can quickly jump over buffered stdin without need of holding
   big buffer for std::reverse purpose.

    real	0m0.729s
    user	0m0.060s
    sys	0m0.667s
 */
int main10() {
    const auto buffer_size = 1<<16;
    auto buffer = (char*) mmap (NULL, buffer_size+1, PROT_READ|PROT_WRITE,
             MAP_PRIVATE|MAP_POPULATE|MAP_ANONYMOUS, -1, 0);
    assert(buffer != MAP_FAILED);
    size_t size = buffer_size;
    auto read_bytes = 0u, reverses = 0u;
    int first = -1, last = -1;
    while ((size = read(fileno(stdin), buffer, buffer_size)) != 0) {
       auto found = (char*)memchr(buffer, '>', size);
       if (found) {
           if (first == -1)
               first = found - buffer + read_bytes;
           else
              if (last == -1)
                 last = found - buffer + read_bytes;
       }

       read_bytes += size;
       if ((first != -1 && last != -1) || size < buffer_size) {
           if (last == -1)
               last = read_bytes;
           //printf("rev:  %d  %d\n", first , last);

           off_t start_from = last - buffer_size;
           write(fileno(stdout), ">>\n", 3);
           while ((size = pread(fileno(stdin), buffer, buffer_size, start_from))) {
              std::reverse(buffer, buffer + size);
              write(fileno(stdout), buffer,  size);
              start_from -= size;
              if (start_from <= first || size == (size_t)-1)
                  break;
           }
           reverses++;
           if (last != -1) {
               first = last;
               last = -1;
           }
       }
    }
    munmap(buffer, buffer_size+1);
    return 0;
}

int main() {
    return main00();
}
