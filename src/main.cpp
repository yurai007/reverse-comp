#include <iostream>
#include <algorithm>
#include <vector>
#include <chrono>
#include <cassert>
#include <cstring>

const int margin = 60;
const int range_size = 'z'-'A'+1;
char upper_complements[range_size];

static inline void upper_complement_v2(std::string &str)
{
    for (size_t i = 0; i< str.size(); i++)
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

// we process accum_sequence from end to begin extracting chunks with size = margin
static void split_and_add_to_buffer(const std::string &accum_sequence, std::string &buffer)
{
    int current_id = accum_sequence.size();
    while (current_id > 0)
    {
        size_t pos = std::max(int(current_id) - margin, 0);
        std::string new_line(accum_sequence, pos, current_id - pos);
        std::reverse(new_line.begin(), new_line.end());
        buffer += new_line + "\n";
        current_id -= margin;
    }
}


/* SUMMARY:
 * Now 13ms
 * _Z9test_casev (inlined upper_complement_v2) - 20%
   _int_malloc + __memcpy_sse2_unaligned - 20%

 * Still 30.000 allocations (valgind) ~ 3 allocations per loop.
   ~30.000 cache miss (perf) ~ 4 per loop

   Symbols view from Qt Creator is buggy - still shows hashtable but they don't exist (objdump helped)
 */
void test_case()
{
    auto t1 = std::chrono::high_resolution_clock::now();

    std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);
    std::string accum_sequence, buffer, line;

    init_upper_complements();

    while (std::getline(std::cin, line))
    {
        if (line[0] == '>')
        {
            if (!accum_sequence.empty())
            {
                split_and_add_to_buffer(accum_sequence, buffer);
                accum_sequence.clear();
            }
            buffer += line + '\n'; // extra allocation in buffer + memcpy from line
        }
        else
        {
            upper_complement_v2(line);
            accum_sequence += line;
        }
    }

    if (!accum_sequence.empty())
    {
        split_and_add_to_buffer(accum_sequence, buffer);
        accum_sequence.clear();
    }

    std::cout << buffer;

    auto t2 = std::chrono::high_resolution_clock::now();
    std::chrono::milliseconds s = std::chrono::duration_cast<std::chrono::milliseconds> (t2-t1);
    std::cerr << "time: " << s.count() << " ms \n";
}

int main()
{
    test_case();
    return 0;
}
