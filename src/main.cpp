#include <iostream>
#include <chrono>
#include <cassert>
#include <cstring>

template <int fixed_size>
class fast_string
{
public:
    inline void reverse(int pos, int count)
    {
        int last = pos + count;
        for (int i = 0; i <= count/2; i++)
        {
            char temp = buffer[pos + i];
            buffer[pos + i] = buffer[last-i];
            buffer[last-i] = temp;
        }
    }

    inline void reset_whole()
    {
        current_size = 0;
        memset(buffer, 0, sizeof(buffer));
    }

    inline void refresh_size()
    {
        current_size = strlen(buffer);
    }

    template <int source_fixed_size>
    inline void append(fast_string<source_fixed_size> &source, int source_pos, int count)
    {
        memcpy(&buffer[current_size], &source.buffer[source_pos], count); //strcpy?
        current_size += count;
    }

    inline void append(char one)
    {
        buffer[current_size] = one;
        current_size++;
    }

    inline void clear()
    {
        current_size = 0;
    }

    inline size_t size()
    {
        return current_size;
    }

public:
    char buffer[fixed_size];
    size_t current_size {0};
};

const int margin = 60;
const int buffer_size = 1024*1024;
const int accum_sequence_size = 16*1024;
const int range_size = 'z'-'A'+1;

fast_string<buffer_size> buffer;
fast_string<accum_sequence_size> accum_sequence;
fast_string<margin + 2> line;
static char upper_complements[range_size];


static inline void upper_complement_v3(fast_string<margin + 2> &str)
{
    for (size_t i = 0; i < str.size(); i++)
    {
        assert('A' <= str.buffer[i] && str.buffer[i] <= 'z');
        str.buffer[i] = upper_complements[str.buffer[i] - 'A'];
    }
}

static void init_upper_complements()
{
    memset(upper_complements, 0, sizeof(upper_complements));
    upper_complements['A' - 'A'] =  'T';
    upper_complements['C' - 'A'] =    'G';
    upper_complements['G' - 'A'] =    'C';
    upper_complements['T' - 'A'] =    'A';
    upper_complements['U' - 'A'] =     'A';
    upper_complements['M' - 'A'] =    'K';
    upper_complements['R' - 'A'] =    'Y';
    upper_complements['W'- 'A'] =    'W';
    upper_complements['S'- 'A'] =    'S';
    upper_complements['Y'- 'A'] =    'R';
    upper_complements['K'- 'A'] =   'M';
    upper_complements['V' - 'A'] =   'B';
    upper_complements['H'- 'A'] =    'D';
    upper_complements['D' - 'A'] =   'H';
    upper_complements['B' - 'A'] =    'V';
    upper_complements['N' - 'A'] =    'N';

    upper_complements['a' - 'A'] =  'T';
    upper_complements['c' - 'A'] =    'G';
    upper_complements['g' - 'A'] =    'C';
    upper_complements['t' - 'A'] =    'A';
    upper_complements['u' - 'A'] =     'A';
    upper_complements['m' - 'A'] =    'K';
    upper_complements['r' - 'A'] =    'Y';
    upper_complements['w'- 'A'] =    'W';
    upper_complements['s'- 'A'] =    'S';
    upper_complements['y'- 'A'] =    'R';
    upper_complements['k'- 'A'] =   'M';
    upper_complements['v' - 'A'] =   'B';
    upper_complements['h'- 'A'] =    'D';
    upper_complements['d' - 'A'] =   'H';
    upper_complements['b' - 'A'] =    'V';
    upper_complements['n' - 'A'] =    'N';
}

static void split_and_add_to_buffer_v2(fast_string<accum_sequence_size> &accum_sequence,
                                       fast_string<buffer_size> &buffer)
{
    int current_id = accum_sequence.size();
    while (current_id > 0)
    {
        size_t pos = std::max(int(current_id) - margin, 0);
        accum_sequence.reverse(pos, current_id - pos - 1);

        buffer.append(accum_sequence, pos, current_id - pos);
        buffer.append('\n');

        current_id -= margin;
    }
}

/*
  SUMMARY: Now only 7ms !

  * Whole program does only 6 allocations (valgrind) (from std::ios_base::sync_with_stdio).
  So main processing loop is allocation free.
  Still ~27.000 cache misses (~ 3.5 per loop)
  Lots branches (320/loop/line). But only 3 branch misses per loop.
  75% time is spent in upper_complement_v3.
  In _Z25test_case_allocation_freev only memcpy and fgets_unlocked weren't inlined.

  TO DO: Copy-free implementation
 */


void test_case_allocation_free()
{
    auto t1 = std::chrono::high_resolution_clock::now();

    std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);

    line.reset_whole();
    init_upper_complements();

    while (fgets_unlocked(line.buffer, margin+2, stdin))
    {
        // line contains sequence \n,0 but we ommit that immediatly after reading
        line.refresh_size();
        line.current_size--;

        if (line.buffer[0] == '>')
        {
            if (accum_sequence.size() != 0)
            {
                split_and_add_to_buffer_v2(accum_sequence, buffer);
                accum_sequence.clear();
            }

            buffer.append(line, 0, line.size());
            buffer.append('\n');
        }
        else
        {
            upper_complement_v3(line);
            accum_sequence.append(line, 0, line.size());
        }
    }

    if (accum_sequence.size() != 0)
    {
        split_and_add_to_buffer_v2(accum_sequence, buffer);
        accum_sequence.clear();
    }

    std::cout << buffer.buffer;

    auto t2 = std::chrono::high_resolution_clock::now();
    std::chrono::milliseconds s = std::chrono::duration_cast<std::chrono::milliseconds> (t2-t1);
    std::cerr << "time: " << s.count() << " ms \n";
}


int main()
{
    test_case_allocation_free();
    return 0;
}
