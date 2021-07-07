#include <stdint.h>
#include <cstddef>
#include <cmath>
#include <bit>
#include <limits>
#include <cstdint>

/*

Basic stuff, ref: https://en.wikipedia.org/wiki/Two%27s_complement

- because of sign encoded in MSB, integer range is not symmetric.
- -n = ~n + 1
   Example:
   42 = 0010 1010
  -42 = 1101 0110

  n ^ (-1) <=> ~n

  -1 = 1111...111 (max uint)
- when a two's-complement number is shifted to the right, the most-significant bit,
   which contains magnitude and the sign information, must be maintained.

Ref: https://graphics.stanford.edu/~seander/bithacks.html

    - most stuff is obsolete but still quite interesting to see how to avoid branches
    - CHAR_BIT is important
    - MSB - most significant bit
    - many of below cases ma easly use lookup table - LUT.

    About popcnt
Ref: https://stackoverflow.com/questions/109023/how-to-count-the-number-of-set-bits-in-a-32-bit-integer
- it's https://en.wikipedia.org/wiki/Hamming_weight
TODO: cont this, especially 0x80 WM stuff. also: https://godbolt.org/z/q9cs9vdxd

*/

/*
For 32 bit integer sign depends on MSB (it's set for negative value).

Sign is: v < 0 = T => -(v < 0) = -1, otherwise 0
It's equivalent to v >> 31 for 32-bit integers or sar eax, 31 instruction.

Sar eax, 31 instruction:
- perform right shift of eax.
- sets or clears the most significant bit to correspond to the MSB of the original value in eax
  (look at basic stuff).

In consequence sar eax, 31 is 0 (for v >= 0) or -1 (as it preserve MSB = sign).
Notice that it returns 0 (all bits 0) or -1 (all bits 1)
*/
int sign_0(int v) {
    return  v >> 31;
}

int sign_1(int v) {
    return  -(v < 0);
}
/*
  Mathematical sign - it detects 0
*/
int sign_2(int v) {
    // -1, 0, or +1
    return  (v > 0) - (v < 0);
}

bool different_signs_3(int x, int y) {
    // xor + extract MSB
    return ((x ^ y) < 0);
}

/*
  For bit A, A XOR 1 happens to toggle A.
  A XOR 0 happens to leave A intact.
  if sign is 0 it's (v + 0) ^ 0 = v.
  if sign is -1 it's (v - 1) ^ (-1) = -v
*/
int abs_4(int v) {
   // MSB
   int const sign = v >> 31;
   return (v + sign) ^ sign;
}

int abs_41(int x) {
    return (x >= 0) ? x : -x;
}

int abs_5(int v) {
    return std::abs(v);
}

/*
    cmp + cmov
*/
int max_6(int x, int y) {
    // actually cmov - conditional move
    return x ^ ((x ^ y) & -(x < y));
}

int max_7(int x, int y) {
    return std::max(x, y);
}
/*
    Doesn't works for 0.
    If v is 2^n = 100..0 then x-1 is 011..1 so AND gives 0.
*/
bool is_power_of_2_8(int v) {
    return (v & (v - 1)) == 0;
}

int conditionally_set_9(int w, int m, bool f) {
    return (w & ~m) | (-f & m);
}

int conditionally_negate_10(int v, bool fNegate) {
    return (v ^ -fNegate) + fNegate;
}

/* popcnt
   GCC/clang doesn't recognize pattern
*/
unsigned count_bits_set_11( unsigned v) {
    unsigned int c;
    for (c = 0; v; v >>= 1) {
        c += v & 1;
    }
    return c;
}

// popcnt, ok pattern recognized
unsigned count_bits_set_12( unsigned v) {
    unsigned int c;
    for (c = 0; v; c++) {
        v &= v - 1; // clear the least significant bit set
    }
    return c;
}

/*
    GCC/clang doesn't recognize pattern
*/
void swap_13(int &x, int &y) {
    x ^= y;
    y ^= x;
    x ^= y;
}

/*
    It looks better than above (less instr, no xors).
    But wouldn't be SIMD permute faster?
*/
void swap_133(long * __restrict__ x, long * __restrict__ y) {
    std::swap(*x, *y);
}

// TO DO: cont below

unsigned char reverse_14(unsigned char b) {
    return (b * 0x0202020202ULL & 0x010884422010ULL) % 1023;
}

unsigned log2_15(unsigned v) {
    unsigned r = 0;
    while (v >>= 1) {
        r++;
    }
    return r;
}

unsigned log2_16(unsigned v) {
   return std::log2(v);
}

unsigned log10_17(unsigned v) {
    return (v >= 1000000000) ? 9 : (v >= 100000000) ? 8 : (v >= 10000000) ? 7 :
    (v >= 1000000) ? 6 : (v >= 100000) ? 5 : (v >= 10000) ? 4 :
    (v >= 1000) ? 3 : (v >= 100) ? 2 : (v >= 10) ? 1 : 0;
}

unsigned log10_18(unsigned v) {
   return std::log10(v);
}

unsigned next_highest_power_2_19(unsigned v) {
    // 1U << (lg(v - 1) + 1)
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

unsigned has_zero_byte_20(unsigned v) {
    return (((v) - 0x01010101UL) & ~(v) & 0x80808080UL);
}

unsigned has_byte_21(unsigned v, unsigned char n) {
    return has_zero_byte_20((v) ^ (~0UL/255 * (n)));
}

/* ref: https://en.wikipedia.org/wiki/Fast_inverse_square_root
  bit_cast instead reinterpret_casting (strict aliasing violation)
  or type punning though union (UB)

  Now rsqrtss is probably much faster
*/
 float Q_rsqrt(float number) noexcept {
    static_assert(std::numeric_limits<float>::is_iec559);
    // (enable only on IEEE 754)

    float const y = std::bit_cast<float>(
        0x5f3759df - (std::bit_cast<std::uint32_t>(number) >> 1));
    return y * (1.5f - (number * 0.5f * y * y));
}

int main() {
    return 0;
}
