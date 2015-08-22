#include <iostream>
#include <chrono>
#include <cassert>
#include <cstring>

const int margin = 60;
const int range_size = 'z'-'A'+1;
static char line[margin + 2];
static char upper_complements[range_size];

static inline void upper_complement_v3(char *str, int str_size)
{
    for (size_t i = 0; i < str_size; i++)
    {
        assert('A' <= str[i] && str[i] <= 'z');
        str[i] = upper_complements[str[i] - 'A'];
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

static inline void reverse(char *buffer, int pos, int count)
{
    int last = pos + count;
    for (int i = 0; i <= count/2; i++)
    {
        char temp = buffer[pos + i];
        buffer[pos + i] = buffer[last-i];
        buffer[last-i] = temp;
    }
}

static void split_and_add_to_buffer_v2(char *accum_sequence,
                                       int accum_sequence_size,
                                       char *buffer,
                                       int &buffer_size)
{
    int current_id = accum_sequence_size;
    while (current_id > 0)
    {
        size_t pos = std::max(int(current_id) - margin, 0);
        reverse(accum_sequence, pos, current_id - pos - 1);

        memcpy(&buffer[buffer_size], &accum_sequence[pos], current_id - pos); //strcpy?
        buffer_size += current_id - pos;
        buffer[buffer_size] = '\n';
        buffer_size++;

        current_id -= margin;
    }
}

/*
  SUMMARY: Now only 7ms !

  * Whole program does only 6 allocations (valgrind) (from std::ios_base::sync_with_stdio).
  So main processing loop is allocation free.
  Still ~27.000 cache misses (~ 3.5 per loop)
  Lots branches (320/loop/line). But only 3 branch misses per loop.
  75% time is spent in upper_complement_v3


  hey, what about custom fast_string class?
 */
void test_case_allocation_free()
{
    auto t1 = std::chrono::high_resolution_clock::now();

    std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);

    static char accum_sequence[16*1024];
    int accum_seq_size = 0;

    static char buffer[1024*1024];
    int buffer_size = 0;

    int line_size = 0;
    memset(line, 0, sizeof(line));

    init_upper_complements();

    while (fgets_unlocked(line, margin+2, stdin))
    {
        // line contains sequence \n,0 but we ommit that immediatly after reading
        line_size = strlen(line);
        line_size--;
        if (line[0] == '>')
        {
            if (accum_seq_size != 0)
            {
                split_and_add_to_buffer_v2(accum_sequence, accum_seq_size,
                                           buffer, buffer_size);
                accum_seq_size = 0;
            }

            memcpy(&buffer[buffer_size], line, line_size);
            buffer_size += line_size;
            buffer[buffer_size] = '\n';
            buffer_size++;
        }
        else
        {
            upper_complement_v3(line, line_size);

            memcpy(&accum_sequence[accum_seq_size], line, line_size);
            accum_seq_size += line_size;
        }
    }

    if (accum_seq_size != 0)
    {
        split_and_add_to_buffer_v2(accum_sequence, accum_seq_size,
                                   buffer, buffer_size);
        accum_seq_size = 0;
    }

    std::cout << buffer;

    auto t2 = std::chrono::high_resolution_clock::now();
    std::chrono::milliseconds s = std::chrono::duration_cast<std::chrono::milliseconds> (t2-t1);
    std::cerr << "time: " << s.count() << " ms \n";
}

static void unit_tests()
{
    {
        char buffer[] = "0123456789";
        reverse(buffer, 0, 0);
    }
    {
        char buffer[] = "0123456789";
        reverse(buffer, 0, 1);
    }
    {
        char buffer[] = "0123456789";
        reverse(buffer, 0, 2);
    }
    {
        char buffer[] = "0123456789";
        reverse(buffer, 0, 3);
    }
    {
        char buffer[] = "0123456789";
        reverse(buffer, 0, 9);
    }
    {
        char buffer[] = "0123456789";
        reverse(buffer, 0, 9);
    }

    char accum_sequence[] = "123CGGGCGCGGTGGCTCACGCCTGTAATCCCAGCACTTTGGGAGGCCGAGGCGGGC456"
    "012CCTGAGGTCAGGAGTTCGAGACCAGCCTGGCCAACATGGTGAAACCCCGTCTCT789"
    "AAAAATACAAAAATTAGCCGGGCGTGGTGGCGCGCGCCTGTAATCCCAGCTACTCGGGAG"
    "GCTGAGGCAGGAGAATCGCTT";
    int accum_sequence_size = strlen(accum_sequence);

    char buffer[512];
    int buffer_size = 0;


    split_and_add_to_buffer_v2(accum_sequence, accum_sequence_size, buffer,
                               buffer_size);
    printf(":)\n");

}


int main()
{
    test_case_allocation_free();
    return 0;
}
