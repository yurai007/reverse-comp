#include <utility>
#include <cstdio>
#include <cassert>
#include <cstring>

template <int fixed_size>
class fast_string
{
public:
	inline size_t size()
	{
		return current_size;
	}

	inline int new_end_in_chunk()
	{
		return strlen(&buffer[current_size]);
	}

	inline char *read_chunk_until(int bytes)
	{
		return fgets_unlocked(&buffer[current_size], bytes, stdin);
	}

	inline char last() const
	{
		return buffer[current_size];
	}

	void update_size_with(int bytes)
	{
		current_size += bytes;
	}

public:
	char buffer[fixed_size];
	size_t current_size {0};
};

const int buffer_size = 1024*1024;
fast_string<buffer_size> buffer;

const int margin = 60;
const int range_size = 'z'-'A'+1;

static char upper_complements[range_size];

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

static inline void validate_buffer(char *str, size_t str_size)
{
	for (size_t i = 0; i < str_size; i++)
	{
		assert('A' <= str[i] && str[i] <= 'z');
	}
}

static inline void upper_complement(char *str, size_t str_size)
{
	for (size_t i = 0; i < str_size; i++)
	{
		str[i] = upper_complements[str[i] - 'A'];
	}
}

/*
  SUMMARY: Now only 7ms !

  * Whole program does only 6 allocations (valgrind) (from std::ios_base::sync_with_stdio).
  So main processing loop is allocation free.
  Still ~27.000 cache misses (~ 3.5 per loop)
  Lots branches (320/loop/line). But only 3 branch misses per loop.
  75% time is spent in upper_complement

  faster2.asm - one extra jump for _Z25test_case_allocation_freev (23)
  OK. Adding accum_sequence sucks!!!! 18.9mln --> 19.5mln
  OK. So it has sth to do with reverse.

  Finally!!!
  Changing
			reverse(accum_sequence.buffer, pos, current_id - pos - 1);
				----> accum_sequence.reverse(pos, current_id - pos - 1);
  cause slow down!!!

  1. "Static inline" for reverse and reverse_chunk_by_chunk_and_add_to_buffer vs no "static inline" keywords.
	 The only difference is that for static inline compiler (-Ofast) doesn't generate extra symbols
	 in binary image. In both cases (so even if we don't use static inline) functions are inlined.
	 Moreover all fast_string methods are inline so inlining in nowadays compilers is very aggressive.

  2. Be careful with timings. Always run under perf. CPU speed may vary (I experienced
	 big variation on laptop between 1.2GHZ to 3.1 GHZ !!). This is Intel Turbo Boost.

  3. Allocation-free. Worst ~9ms for 1.25GHz

 Performance counter stats for './main':

		  8.469100      task-clock (msec)         #    0.937 CPUs utilized
				 3      context-switches          #    0.354 K/sec
				 0      cpu-migrations            #    0.000 K/sec
			   261      page-faults               #    0.031 M/sec
		10,721,721      cycles                    #    1.266 GHz
		 4,611,703      stalled-cycles-frontend   #   43.01% frontend cycles idle
   <not supported>      stalled-cycles-backend
		19,291,112      instructions              #    1.80  insns per cycle
												  #    0.24  stalled cycles per insn
		 3,354,973      branches                  #  396.143 M/sec
			43,043      branch-misses             #    1.28% of all branches

	   0.009036239 seconds time elapsed



 4. Copy-free. Worst ~8ms for 1.217 GHz

 Performance counter stats for './main':

		  7.781306      task-clock (msec)         #    0.943 CPUs utilized
				 1      context-switches          #    0.129 K/sec
				 0      cpu-migrations            #    0.000 K/sec
			   258      page-faults               #    0.033 M/sec
		 9,468,728      cycles                    #    1.217 GHz
		 3,844,625      stalled-cycles-frontend   #   40.60% frontend cycles idle
   <not supported>      stalled-cycles-backend
		18,037,766      instructions              #    1.90  insns per cycle
												  #    0.21  stalled cycles per insn
		 3,108,946      branches                  #  399.540 M/sec
			39,152      branch-misses             #    1.26% of all branches

	   0.008251506 seconds time elapsed

	- accumulated variable is redundant and I can remove it.
	- 'A terminating null character is automatically appended after the characters copied to str.'
	   ; fgets - so I don't need extra memset
	- about 50% branch misses and 50% cache misses comes from I/O (fgets + cout) so
	  probably only few % potential speed up leaves

	Performance counter stats for './main':

		  7.516152      task-clock (msec)         #    0.943 CPUs utilized
				 0      context-switches          #    0.000 K/sec
				 0      cpu-migrations            #    0.000 K/sec
			   257      page-faults               #    0.034 M/sec
		 9,303,017      cycles                    #    1.238 GHz
		 3,868,501      stalled-cycles-frontend   #   41.58% frontend cycles idle
   <not supported>      stalled-cycles-backend
		17,870,674      instructions              #    1.92  insns per cycle
												  #    0.22  stalled cycles per insn
		 3,101,603      branches                  #  412.658 M/sec
			39,875      branch-misses             #    1.29% of all branches

	   0.007974182 seconds time elapsed

	- removed totally iostreams and replaced cout on fwrite. 1mln instructions less:)

 Performance counter stats for './main':

		  6.552942      task-clock (msec)         #    0.936 CPUs utilized
				 0      context-switches          #    0.000 K/sec
				 0      cpu-migrations            #    0.000 K/sec
			   243      page-faults               #    0.037 M/sec
		 7,895,484      cycles                    #    1.205 GHz
		 2,883,699      stalled-cycles-frontend   #   36.52% frontend cycles idle
   <not supported>      stalled-cycles-backend
		16,607,023      instructions              #    2.10  insns per cycle
												  #    0.17  stalled cycles per insn
		 2,908,317      branches                  #  443.819 M/sec
			37,870      branch-misses             #    1.30% of all branches

	   0.007004715 seconds time elapsed

*/

static void reverse_chunk_by_chunk(fast_string<buffer_size> &buffer, int from, int to)
{
	int source = from;
	int destination = to;
	while (source < destination)
		if (buffer.buffer[source] == '\n')
			source++;
		else
			if (buffer.buffer[destination] == '\n')
				destination--;
			else
			{
				std::swap(buffer.buffer[source], buffer.buffer[destination]);
				source++;
				destination--;
			}
}

static void test_case_copy_free()
{
	const int max_line_size = margin+2;
	int line_size = 0;
	int last_body_start = 0;

	init_upper_complements();

	while (buffer.read_chunk_until(max_line_size) != NULL)
	{
		line_size = buffer.new_end_in_chunk();
		if (buffer.last() == '>')
		{
			reverse_chunk_by_chunk(buffer, last_body_start, buffer.current_size-1);
			last_body_start = buffer.size() + line_size;
		}
		else
			upper_complement(&buffer.buffer[buffer.current_size], line_size-1);

		buffer.update_size_with(line_size);
	}

	reverse_chunk_by_chunk(buffer, last_body_start, buffer.current_size-1);
	fwrite (buffer.buffer , sizeof(char), buffer.current_size, stdout);
}

int main()
{
	test_case_copy_free();
	return 0;
}


