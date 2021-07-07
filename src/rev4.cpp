#include <cstring>
#include <sys/stat.h>
#include <ctime>
#include <unistd.h>
#include <array>
#include <cstdio>
#include <thread>
#include <algorithm>
#include <sys/mman.h>
#include <string.h>
#include <cstdlib>
#include <cassert>
#include <immintrin.h>
#include <iostream>

/*
  INTRINSIC TESTS - PRELIMINARIES

  *  ref0: MAIN REF: https://software.intel.com/sites/landingpage/IntrinsicsGuide/
     ref1: http://0x80.pl/articles/simd-byte-lookup.html
     ref2: https://www.felixcloutier.com/x86/pshufb

  *  dummy basics
     - x mod 2^i == x & 2^i - 1 which is (11..11) (i times); taking low
     - x div 2^i == x >> i; taking high
     - x & (10..0); checking n-th bit
     - folding from high and low: high << 4 | low


  * _mm256i _mm256_set_epi8 (char e31, ..., char e0);
    _mm_set_epi32 (int __i3, ..., int __i0)
    _mm_setr_epi8(char e0, ..., char e15) - "normal order" set_epi
    _mm_set1_epi8(char a) - vpbroadcastb

      It sets vectors byte after byte starting from highest byte (al'a Big Endian order)

  *  _mm256_loadu_epi8(void const* mem_addr) - vmovdqu8

      Load 256 bit (composed of 32 packed bytes) from memory into dst.
      mem_addr does not need to be aligned on any particular boundary.

  *  __m128i _mm_and_si128(__m128i __a, __m128i __b) - pand

      It does dst[127:0] := (a[127:0] AND b[127:0])

  *   __m128i _mm_cmpeq_epi32(__m128i __a, __m128i __b) - pcmpeqd

      It does dst[i+31:i] := ( a[i+31:i] == b[i+31:i] ) ? 0xFFFFFFFF : 0

  *   __m128i _mm_cmplt_epi8 (__m128i a, __m128i b) - pcmpgtb

      Similar to above but it does "less than" on 1B (instead 4B) granularity:
        dst[i+7:i] := ( a[i+7:i] < b[i+7:i] ) ? 0xFF : 0

  *   __m128i _mm_srli_epi32(__m128i __a, int __count) - psrld

      It shifts packed 32-bit integers in right by count while shifting in zeros, and store the results in dst.

  *  _mm_shuffle_epi8(__m128i a, __m128i b),
     _mm256_shuffle_epi8(__m256i a, __m256i b),
     _mm512_shuffle_epi8(__m512i a, __m512i b) - vpshufb

     a - source
     b - control mask

    The instruction _permutes_ the data in the dst operand, leaving the shuffle (control) mask unaffected.
    Permutation is done with 1 byte granularity.
    If the most significant bit (bit[7]) of each byte of the shuffle control mask is set, then
    zero is written in the result byte. software_scalar_shuffle for more details.

  * _mm256_permute2x128_si256 (__m256i a, __m256i b, const int imm8) - vperm2i128

    Shuffle 128-bits (composed of integer data) selected by imm8 (mask) from a and b, and store the results in dst.
    Permutation is done with 16 bytes granularity. Basically for imm8=1 it does:

        dst[127:0] := a[255:128]
        dst[255:128] := a[127:0]

    In this case b is unsused. Another imm8 change behaviour (we can take bytes from b instead a, etc.).
    In this case it acts just as simple swap ymm.L with ymm.H. Well, fastest possible std::swap.

  * __m128i _mm_blendv_epi8 (__m128i a, __m128i b, __m128i mask) - pblendvb

    IF mask[i+7]
        dst[i+7:i] := b[i+7:i]
    ELSE
        dst[i+7:i] := a[i+7:i]

  * std::reverse implementation
    ref: https://dev.to/wunk/fast-array-reversal-with-simd-j3p

    - for 256 bit vector __m256i it's required to shuffle and do extra permute -
      _mm256_shuffle_epi8 act like 2x _mm_shuffle_epi8 for two halfs.

  TODO: would be good to see it under MCA.
*/

struct __attribute__ ((aligned (16))) vec
{
    unsigned int i0, i1, i2, i3;

    bool operator==(const vec &other) const {
        return i0 == other.i0 && i1 == other.i1 && i2 == other.i2 && i3 == other.i3;
    }
};

/*
     * _mm_sra_epi32 - is bad because treats input vector as ints so
      if input = 0xffffffff after shifting by 31 it's still 0xffffffff!
      That's why _mm_srli_epi32 is needed.
*/

constexpr unsigned max_uint = 0xffffffff; // equivalent to uint(-1)
constexpr unsigned in = 0b10000000'00000000'00000000'00000000; //0x80000000

static void test_intrinsics1() {
    vec msb = {in, in, in, in};
    __m128i *MSB = (__m128i *)&msb;
    vec tmp2 = {0, 0, 0, 0};
    vec zer = {0, 0, 0, 0};
    __m128i *TMP2 = (__m128i *)&tmp2;
    __m128i *ZER = (__m128i *)&zer;
    __m128i V1 = _mm_set_epi32(0, 0, 0, 0);

    *TMP2 = _mm_and_si128(V1, *MSB);
    assert(tmp2 == vec(0, 0, 0, 0));
    // if TMP2 = ZER =>  max_uint = -1
    *TMP2 = _mm_cmpeq_epi32(*TMP2, *ZER);
    assert(tmp2 == vec(-1, -1, -1, -1));

    //__m128i SHIFT_31 = _mm_set_epi32(31, 31, 31, 31);
    *TMP2 = _mm_srli_epi32(*TMP2, 31); //SHIFT_31
    assert(tmp2 == vec(1, 1, 1, 1));
}

static void test_intrinsics2() {
    vec msb = {in, in, in, in};
    __m128i *MSB = (__m128i *)&msb;
    vec tmp2 = {0, 0, 0, 0};
    vec zer = {0, 0, 0, 0};
    __m128i *TMP2 = (__m128i *)&tmp2;
    __m128i *ZER = (__m128i *)&zer;
    __m128i V1 = _mm_set_epi32(max_uint, max_uint, max_uint, max_uint);

    *TMP2 = _mm_and_si128(V1, *MSB);
    assert(tmp2 == vec(in, in, in, in));
    // if TMP2 = ZER =>  max_uint = -1
    *TMP2 = _mm_cmpeq_epi32(*TMP2, *ZER);
    assert(tmp2 == vec(0, 0, 0, 0));

    //__m128i SHIFT_31 = _mm_set_epi32(31, 31, 31, 31);
    *TMP2 = _mm_srli_epi32(*TMP2, 31); //SHIFT_31
    assert(tmp2 == vec(0, 0, 0, 0));
}

static void test_intrinsics3() {
    vec msb = {in, in, in, in};
    // MAX_INT+1 == MIN_INT-1 ??
    __m128i *MSB = (__m128i *)&msb;
    vec tmp2 = {0, 0, 0, 0};
    vec zer = {0, 0, 0, 0};
    __m128i *TMP2 = (__m128i *)&tmp2;
    __m128i *ZER = (__m128i *)&zer;
    __m128i V1 = _mm_set_epi32(in, 0, in, 0);

    *TMP2 = _mm_and_si128(V1, *MSB);
    assert(tmp2 == vec(0, in, 0, in));

    *TMP2 = _mm_cmpeq_epi32(*TMP2, *ZER);
    assert(tmp2 == vec(-1, 0, -1, 0));

    unsigned i0 = tmp2.i0;
    unsigned r4 = (i0>>31u);

    assert(1 != (tmp2.i0>>30));
    //assert(1 != (tmp2.i0>>31));
    assert(1 == r4);
    assert(0 == (tmp2.i1>>31));
}

constexpr auto VECTOR_SIZE = 16u;
using vector = std::array<uint8_t, VECTOR_SIZE>;

static vector software_scalar_shuffle(const vector &a, const vector &b) {
    vector result;
    for (int i = 0; i < VECTOR_SIZE; i++) {
        uint8_t index = b[i]; //4bit index
        // check if bit[7] is set
        if (index & 0x80)
            result[i] = 0x0;
        else
            result[i] = a[index & 0x0f]; // index mod 15
    }
    return result;
}

/*
 * Here in reverse we do:
        shuffle, shuffle, perm, perm + std::swap
   In below std::reverse there is sequence:
        perm, shuffle, perm, shuffle + vmovdqu8

  * GCC std::reverse (since 8.1) - vectorized with 32B (AVX2) granularity.
    LLVM - not vectorized, only 1B granularity.

.L6:
    // load 32B from beginning
        vmovdqu8        ymm0, YMMWORD PTR [rdx+1]
    // load 32B from end
        vmovdqu8        ymm1, YMMWORD PTR [rsi]

    // handle end - swap(ymm1.L, ymm1.H)
        vperm2i128      ymm1, ymm1, ymm1, 1
    // shuffle ymm1.L and ymm.H following pattern from ymm2 from LC0
        vpshufb ymm1, ymm1, ymm2
    // move result to beginning then do same operations for beginning
        vmovdqu8        YMMWORD PTR [rdx+1], ymm1
        vperm2i128      ymm0, ymm0, ymm0, 1
        vpshufb ymm0, ymm0, ymm2
        vmovdqu8        YMMWORD PTR [rsi], ymm0
    // move to next 32B
        sub     rsi, 32
        add     rdx, 32
        cmp     rdx, rdi
        jne     .L6

.LC0:
        .byte   15
        .byte   14
       ...

    GCC speed is ~4B/cycle. Clang speed much lower ~1B/cycle.
*/
// __m256i = LOWER.UPPER -> (shuffle) -> REWOL.REPPU -> (permute) -> REPPU.REWOL
static void reverse( __m256i &lower,  __m256i &upper) {
    // order is fine, above explanation
    const __m256i rev_mask = _mm256_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);

    // shuffle 2x 32B = 64B working set
    lower = _mm256_shuffle_epi8(lower, rev_mask);
    upper = _mm256_shuffle_epi8(upper, rev_mask);

    // permute each 256 bit vector
    lower = _mm256_permute2x128_si256(lower, lower, 1);
    upper = _mm256_permute2x128_si256(upper, upper, 1);

    std::swap(lower, upper);
}

static void test_hardware_reverse() {
    // left = 01234..3031, right = 3233..63
    __m256i left = _mm256_set_epi8(31, 30, 29, 28, 27, 26,
                                   25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6,
                                   5, 4, 3, 2, 1, 0),
            right = _mm256_set_epi8(63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45,
                                    44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32);
    reverse(left, right);
    uint8_t result[64];
    std::memcpy(&result[0], &left, 32);
    std::memcpy(&result[32], &right, 32);
    uint8_t expected[64] = {63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45,
                            44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26,
                            25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6,
                            5, 4, 3, 2, 1, 0};
    assert(std::memcmp(result, expected, 64) == 0);
}

/*

  Ref: http://0x80.pl/articles/simd-byte-lookup.html

  * in general having shuffle let us map small universum to small iniversum. Consider mapping:
    'a'->'c', 'd'->'a', 'b'->'d', 'a' = 0, 'd' = 3. Then shuffling our lut = <2,3,-,0> with mask (input) do the job:
        res[i] = lut[mask & 0x0f];
    The challenge is when input item is > 4bit.

  * In scalar_in_set1 bitmap is 16 bits x 16 bits. if byte_item = LH it's enough to check if
    bitmap[L][H] is set. Notice every bitmap[L] represents 16 items that's why we need to
    move 1 << hi_nibble.

  * to move to real vector impl in scalar_in_set2 we need to use 2 bitmaps because pshufb limitations.

  * in vector_in_set we process (do lookup) for 16 byte_items "at once".

  * there are many alternative implementations and special cases.
*/

bool scalar_in_set1(uint16_t bitmap[16], uint8_t byte_item) {

    // take low = row and high = column
    const uint8_t lo_nibble = byte_item & 0xf;
    const uint8_t hi_nibble = byte_item >> 4;

    // column = index in row (bitmap[lo_nibble])
    // bitmask for checking column bit
    const uint16_t bitmask = uint16_t(1) << hi_nibble;
    const uint16_t bitset  = bitmap[lo_nibble];

    // if column bit is set byte_item is here
    return (bitset & bitmask) != 0;
}


bool scalar_in_set2(uint8_t bitmap_0_7[16], uint8_t bitmap_8_15[16], uint8_t byte_item) {

    const uint8_t lo_nibble  = byte_item & 0x0f;
    const uint8_t hi_nibble = byte_item >> 4;
    // so far same as before

    // because 2B bitset is stored in two bitmaps take low bitset - bitset_0_7
    // and high bitset_8_15.
    // Both 'lookups' will be pshufb
    const uint8_t bitset_0_7  = bitmap_0_7[lo_nibble];
    const uint8_t bitset_8_15 = bitmap_8_15[lo_nibble];

    // we do mod 8 only for pshufb purposes.
    // Instead shifting we can map hi_nibble to 2^hi_nibble.
    const uint8_t bitmask = (1 << (hi_nibble & 0x7));

    uint8_t bitset;
    // depending of index value we take low or high bitmap. We use blend instr.
    if (hi_nibble < 8)
        bitset = bitset_0_7;
    else
        bitset = bitset_8_15;

    // if column bit is set byte_item is here
    return (bitset & bitmask) != 0;
}

// check 16 byte items at once
void vector_in_set(uint8_t *ptr) {
    const __m128i input = _mm_loadu_si128(ptr);
    const __m128i lower_nibbles = _mm_and_si128(input, _mm_set1_epi8(0x0f));
    const __m128i higher_nibbles = _mm_and_si128(_mm_srli_epi16(input, 4), _mm_set1_epi8(0x0f));

    // Example of already filled set from 'Implementation' section
    static const __m128i bitmap_0_7 = _mm_setr_epi8(
        /* 0 */ 0x43, /* 01000011 */
        /* 1 */ 0x6f, /* 01101111 */
        /* 2 */ 0x52, /* 01010010 */
        /* 3 */ 0x86, /* 10000110 */
        /* 4 */ 0x00, /* 00000000 */
        /* 5 */ 0xd3, /* 11010011 */
        /* 6 */ 0xa1, /* 10100001 */
        /* 7 */ 0x04, /* 00000100 */
        /* 8 */ 0x0c, /* 00001100 */
        /* 9 */ 0x9c, /* 10011100 */
        /* a */ 0x40, /* 01000000 */
        /* b */ 0x48, /* 01001000 */
        /* c */ 0x11, /* 00010001 */
        /* d */ 0xb8, /* 10111000 */
        /* e */ 0x85, /* 10000101 */
        /* f */ 0x43  /* 01000011 */
    );
    static const __m128i bitmap_8_15 = _mm_setr_epi8(
        /* 0 */ 0x24, /* 00100100 */
        /* 1 */ 0xb0, /* 10110000 */
        /* 2 */ 0x24, /* 00100100 */
        /* 3 */ 0x54, /* 01010100 */
        /* 4 */ 0xf0, /* 11110000 */
        /* 5 */ 0xc5, /* 11000101 */
        /* 6 */ 0x14, /* 00010100 */
        /* 7 */ 0x48, /* 01001000 */
        /* 8 */ 0x80, /* 10000000 */
        /* 9 */ 0x04, /* 00000100 */
        /* a */ 0x84, /* 10000100 */
        /* b */ 0x00, /* 00000000 */
        /* c */ 0xc0, /* 11000000 */
        /* d */ 0x0c, /* 00001100 */
        /* e */ 0x0a, /* 00001010 */
        /* f */ 0x70  /* 01110000 */
    );

    const __m128i row_0_7 = _mm_shuffle_epi8(bitmap_0_7, lower_nibbles);
    const __m128i row_8_15 = _mm_shuffle_epi8(bitmap_8_15, lower_nibbles);

    static const __m128i bitmask_lookup = _mm_setr_epi8( // 2^0, 2^1, ..., -2^7
            1, 2, 4, 8, 16, 32, 64, -128,
            1, 2, 4, 8, 16, 32, 64, -128);

    const __m128i bitmask = _mm_shuffle_epi8(bitmask_lookup, higher_nibbles);
    const __m128i mask    = _mm_cmplt_epi8(higher_nibbles, _mm_set1_epi8(8));
    const __m128i bitsets = _mm_blendv_epi8(row_0_7, row_8_15, mask);
    const __m128i tmp    = _mm_and_si128(bitsets, bitmask);
    const __m128i result = _mm_cmpeq_epi8(tmp, bitmask);
}


/*
Reverse_complement_simd

ref: https://benchmarksgame-team.pages.debian.net/benchmarksgame/program/revcomp-gpp-2.html

TODO: would be good to see how moving to AVX speed up things.
      Compare reverse_complement_sse with reverse_complement_avx?
*/
    // SIMD helper, there is _mm_set1_epi8 so no needed.
    __m128i packed(char c) {
        return _mm_set_epi8(c, c, c, c, c, c, c, c, c, c, c, c, c, c, c, c);
    }

    __m128i reverse_complement_sse(__m128i v) {
        // part of reverse - set + shuffle v
        v =  _mm_shuffle_epi8(v, _mm_set_epi8(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15));

        // part of const __m128i lower_nibbles = _mm_and_si128(input, _mm_set1_epi8(0x0f));

        // AND all elements with 0x1f (11111)b, so that a smaller LUT (< 32 bytes)
        // can be used. This is important with SIMD because, unlike
        // single-char complement (above), SIMD uses 16-byte shuffles. The
        // single-char LUT would require four shuffles, this LUT requires
        // two.
        v = _mm_and_si128(v, packed(0x1f));


        // Lookup for all v elements < 16
        // ok, compare v[i] with 16, where smaller 0xFF
        __m128i lt16_mask = _mm_cmplt_epi8(v, packed(16));
        // ok, extract smaller v[i] elements
        __m128i lt16_els = _mm_and_si128(v, lt16_mask);
        // lut for smaller
        // ok, we store indices for input elements from lt16_els: A -> T, B -> V, C-> G, D -> H, etc.
        __m128i lt16_lut = _mm_set_epi8('\0', 'N', 'K', '\0', 'M', '\n', '\0', 'D',
                                        'C', '\0', '\0', 'H', 'G', 'V', 'T', '\0');
        // ok, that's crucial part.
        // lt16_vals[i] := lt16_lut[lt16_els[i]]
        __m128i lt16_vals = _mm_shuffle_epi8(lt16_lut, lt16_els);


        // Lookup for all elements >16
        // ok, similar approach as above
        __m128i g16_els = _mm_sub_epi8(v, packed(16));
        __m128i g16_lut = _mm_set_epi8('\0', '\0', '\0', '\0', '\0', '\0', 'R', '\0',
                                       'W', 'B', 'A', 'A', 'S', 'Y', '\0', '\0');
        __m128i g16_vals = _mm_shuffle_epi8(g16_lut, g16_els);

        // OR both lookup results - merge both vectors
        return _mm_or_si128(lt16_vals, g16_vals);
}

    // TODO: http://0x80.pl/articles/sse-popcount.html + measure with google benchmark?

int main() {
    // identity
    vector a = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    vector b = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    auto r = software_scalar_shuffle(a, b);
    assert(r == a);
    // now reverse
    vector rev_b = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
    auto r2 = software_scalar_shuffle(a, rev_b);
    assert(r2 == rev_b);
    test_hardware_reverse();

    test_intrinsics1();
    test_intrinsics2();
    test_intrinsics3();
    return 0;
}
