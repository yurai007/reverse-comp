#include <iostream>
#include <string>
#include <vector>
#include <fstream>


/*
Reverse group by group with reallocating unsafe_vector (max WS = 512MB).
Still there is penalty ~0.3s because of ~512MB WS.

real	0m1.072s

Is it possible to get into

TODO: we could use just ~333M or 402M which should speed up it.
*/

namespace {
    using std::istream;
    using std::ostream;
    using std::runtime_error;
    using std::string;
    using std::bad_alloc;
    using std::vector;

    constexpr size_t basepairs_in_line = 60;
    constexpr size_t line_len = basepairs_in_line + sizeof('\n');

   class unsafe_vector {
   public:
       unsafe_vector() {
           _buf = (char*)malloc(_capacity);
           if (_buf == nullptr) {
               throw bad_alloc{};
           }
       }

       unsafe_vector(const unsafe_vector& other) = delete;
       unsafe_vector(unsafe_vector&& other) = delete;
       unsafe_vector& operator=(unsafe_vector& other) = delete;
       unsafe_vector& operator=(unsafe_vector&& other) = delete;

       ~unsafe_vector() noexcept {
           free(_buf);
       }

       char* data() {
           return _buf;
       }

       // Resizes the vector to have a size of `count`. This method is
       // UNSAFE because any new vector entries are uninitialized.
       void resize_UNSAFE(size_t count) {
           std::cout << "resize_UNSAFE: " << count << std::endl;
           size_t rem = _capacity - _size;
           if (count > _capacity) {

               grow(count);
           }
           _size = count;
       }

       size_t size() const {
           return _size;
       }

   private:
       void grow(size_t min_cap) {
           size_t new_cap = _capacity;
           while (new_cap < min_cap) {
               new_cap = static_cast<size_t>(new_cap * 2.0f);
           }
           std::cout << "resice grow:  " << new_cap << std::endl;
           char* new_buf = (char*)realloc(_buf, new_cap);
           if (new_buf != nullptr) {
               _capacity = new_cap;
               _buf = new_buf;
           } else {
               // The POSIX definition of `realloc` states that a failed
               // reallocation leaves the supplied pointer untouched, so
               // throw here and let the class's destructor free the
               // untouched ptr (if necessary).
               throw bad_alloc{};
           }
       }

       char* _buf = nullptr;
       size_t _size = 0;
       size_t _capacity = 1024;
   };

    struct Sequence {
        string header;  // not incl. starting delim (>)
        unsafe_vector seq;  // basepair lines. all lines terminated by newline
    };

    void reverse_complement(Sequence& s) {
        char* begin = s.seq.data();
        char* end = s.seq.data() + s.seq.size();

        if (begin == end) {
            return;
        }
        end--;
        std::reverse(begin, end);
    }

    void read_up_to(istream& in, unsafe_vector& out, char delim) {
        constexpr size_t read_size = 1<<16;

        size_t bytes_read = 0;
        out.resize_UNSAFE(read_size);
        while (in) {
            in.getline(out.data() + bytes_read, read_size, delim);
            bytes_read += in.gcount();

            if (in.fail()) {
                // failed because it ran out of buffer space. Expand the
                // buffer and perform another read
                std::cout << "res fail: " << (bytes_read + read_size) << std::endl;
                out.resize_UNSAFE(bytes_read + read_size);
                in.clear(in.rdstate() & ~std::ios::failbit);
            } else if (in.eof()) {
                // hit EOF, rather than delmiter, but an EOF can be
                // treated almost identially to a delmiter, except that we
                // don't remove the delimiter from the read buffer.
                break;
            } else {
                // succeeded in reading *up to and including* the sequence
                // delimiter. Remove the delmiter.
                --bytes_read;
                break;
            }
        }
        std::cout << "res end: " << (bytes_read) << std::endl;
        out.resize_UNSAFE(bytes_read);
    }

    // Read a sequence, starting *after* the first delimiter (>)
    void read_sequence(istream& in, Sequence& out) {
        out.header.resize(0);
        std::getline(in, out.header);
        read_up_to(in, out.seq, '>');
    }

    void write_sequence(ostream& out, Sequence& s) {
        out << '>';
        out << s.header;
        out << '\n';
        out.write(s.seq.data(), s.seq.size());
    }
}

namespace revcomp {
    void reverse_complement_fasta_stream(istream& in, ostream& out) {
        // the read function assumes that '>' has already been read
        // (because istream::getline will read it per loop iteration:
        // prevents needing to 'peek' a bunch).
        if (in.get() != '>') {
            throw runtime_error{"unexpected input: next char should be the start of a seqence header"};
        }

        Sequence s;
        while (not in.eof()) {
            read_sequence(in, s);
            reverse_complement(s);
            write_sequence(out, s);
        }
    }
}

int main() {
    std::cin.sync_with_stdio(false);
    std::cout.sync_with_stdio(false);
//    std::ifstream in("../../data/output.txt");
//    std::ofstream out("out.txt");
    revcomp::reverse_complement_fasta_stream(std::cin, std::cout);
}
