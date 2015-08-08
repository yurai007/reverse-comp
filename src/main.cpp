#include <iostream>
#include <algorithm>
#include <vector>
#include <ctype.h>
#include <unordered_map>
#include <sstream>
#include <chrono>

std::unordered_map<char, char> complements =
{
        {'A',   'T'},
        {'C',   'G'},
        {'G',   'C'},
        {'T',   'A'},
        {'U',    'A'},
        {'M',   'K'},
        {'R',   'Y'},
        {'W',   'W'},
        {'S',   'S'},
        {'Y',   'R'},
        {'K',   'M'},
        {'V',   'B'},
        {'H',   'D'},
        {'D',   'H'},
        {'B',   'V'},
        {'N',   'N'}
};

const int margin = 60;


// this is hotspot now
void upper_complement(std::string &str)
{
    for (size_t i = 0; i< str.size(); i++)
        str[i] = complements[toupper(str[i])];
}

// we process accum_sequence from end to begin extracting chunks with size = margin
void split_and_add_to_buffer(const std::string &accum_sequence, std::string &buffer)
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
 * now 37ms. With fgets ~35ms, but line has probably extra '\n' on the end.
 * ~60% in _Z16upper_complementRSs + 10% in to_upper
   So IO is not a problem now.
 * 30.000 allocations (valgind) ~ 3 allocations per loop.
   ~35.000 cache miss (perf) ~ 4 per loop
 */
void test_case()
{
    auto t1 = std::chrono::high_resolution_clock::now();

    std::ios_base::sync_with_stdio(false);
    std::cin.tie(0);
    std::string accum_sequence, buffer, line;

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
            upper_complement(line);
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
