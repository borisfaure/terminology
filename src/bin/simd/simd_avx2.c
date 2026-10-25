/* AVX2 intake kernels. The target attribute keeps the rest of the binary on
 * baseline x86-64; the dispatcher only reaches these after a runtime probe. */
#include "private.h"
#include "simd.h"
#include <string.h>

#if defined(TERMINOLOGY_HAVE_AVX2)

#include <immintrin.h>

#if defined(__GNUC__)
# define AVX2_TARGET __attribute__((target("avx2")))
#else
# error AVX2 kernels need GCC or clang target attributes
#endif

AVX2_TARGET
size_t
simd_scan_plain_ascii_avx2(const unsigned char *buf, size_t len)
{
   const __m256i lo = _mm256_set1_epi8(0x20);
   const __m256i hi = _mm256_set1_epi8(0x7e);
   size_t i = 0;

   for (; i + 32 <= len; i += 32)
     {
        __m256i v = _mm256_loadu_si256((const __m256i *)(buf + i));
        __m256i bad;
        unsigned int m;

        /* c < 0x20 || c >= 0x7f. Signed compares cover both: bytes with the
         * high bit set are negative, so they fail the first test, and 0x7f
         * alone fails "signed > 0x7e". */
        bad = _mm256_or_si256(_mm256_cmpgt_epi8(lo, v),
                              _mm256_cmpgt_epi8(v, hi));
        m = (unsigned int)_mm256_movemask_epi8(bad);
        if (m) return i + (size_t)__builtin_ctz(m);
     }

   /* Runs are short in terminal output: do not fall from 32 bytes to scalar. */
   if (i + 16 <= len)
     {
        __m128i v = _mm_loadu_si128((const __m128i *)(buf + i));
        __m128i bad;
        unsigned int m;

        bad = _mm_or_si128(_mm_cmplt_epi8(v, _mm_set1_epi8(0x20)),
                           _mm_cmpgt_epi8(v, _mm_set1_epi8(0x7e)));
        m = (unsigned int)_mm_movemask_epi8(bad);
        if (m) return i + (size_t)__builtin_ctz(m);
        i += 16;
     }

   for (; i < len; i++)
     {
        unsigned char c = buf[i];

        if ((c < 0x20) || (c >= 0x7f)) return i;
     }
   return len;
}

AVX2_TARGET
size_t
simd_scan_plain_ascii_u32_avx2(const Eina_Unicode *buf, size_t len)
{
   /* AVX2 has no unsigned 32-bit compare either, so g < 0x20 || g >= 0x7f is
    * split across a wrapping subtraction and a signed compare, as in the SSE2
    * kernel. */
   const __m256i bias = _mm256_set1_epi32(0x20);
   const __m256i span = _mm256_set1_epi32(0x7e);
   size_t i = 0;

   for (; i + 8 <= len; i += 8)
     {
        __m256i v = _mm256_loadu_si256((const __m256i *)(buf + i));
        __m256i under = _mm256_sub_epi32(v, bias);
        __m256i over = _mm256_cmpgt_epi32(v, span);
        /* movemask keeps the top bit of every byte, so lane k owns bits
         * 4k..4k+3 and the lowest set bit gives the first offending lane
         * through ctz >> 2. 'over' sets all four bits of a lane for
         * 0x7f..0x7fffffff. 'under' catches the rest: g - 0x20 has a byte
         * with its top bit set for every g below 0x20 and every g from
         * 0x80000000 up (the low byte for 0x80000000..0x8000001f, the high
         * byte beyond), while it is below 0x80 in every byte for 0x20..0x7e. */
        unsigned int m = (unsigned int)(_mm256_movemask_epi8(under) |
                                        _mm256_movemask_epi8(over));

        if (m) return i + (size_t)(__builtin_ctz(m) >> 2);
     }

   if (i + 4 <= len)
     {
        __m128i v = _mm_loadu_si128((const __m128i *)(buf + i));
        __m128i under = _mm_sub_epi32(v, _mm_set1_epi32(0x20));
        __m128i over = _mm_cmpgt_epi32(v, _mm_set1_epi32(0x7e));
        unsigned int m = (unsigned int)(_mm_movemask_epi8(under) |
                                        _mm_movemask_epi8(over));

        if (m) return i + (size_t)(__builtin_ctz(m) >> 2);
        i += 4;
     }

   for (; i < len; i++)
     {
        Eina_Unicode g = buf[i];

        if ((g < 0x20) || (g >= 0x7f)) return i;
     }
   return len;
}

AVX2_TARGET
size_t
simd_rscan_nonzero_avx2(const unsigned char *buf, size_t len)
{
   const __m256i zero = _mm256_setzero_si256();
   size_t i = len;

   while (i >= 32)
     {
        __m256i v = _mm256_loadu_si256((const __m256i *)(buf + i - 32));
        unsigned int m;

        /* 0xff in every lane whose byte is zero. */
        m = (unsigned int)_mm256_movemask_epi8(_mm256_cmpeq_epi8(v, zero));
        if (m != 0xffffffffu)
          {
             /* Highest clear bit is the last non-zero byte of this block. */
             size_t lane = 31 - (size_t)__builtin_clz(~m);

             return i - 32 + lane + 1;
          }
        i -= 32;
     }

   if (i >= 16)
     {
        __m128i v = _mm_loadu_si128((const __m128i *)(buf + i - 16));
        unsigned int m;

        m = (unsigned int)_mm_movemask_epi8(_mm_cmpeq_epi8(v,
                                                           _mm_setzero_si128()));
        if (m != 0xffffu)
          {
             size_t lane = 31 - (size_t)__builtin_clz(~m & 0xffffu);

             return i - 16 + lane + 1;
          }
        i -= 16;
     }

   while (i > 0)
     {
        if (buf[i - 1]) return i;
        i--;
     }
   return 0;
}

AVX2_TARGET
void
simd_records_or_byte_avx2(void *buf, size_t n, size_t rec, size_t off,
                          unsigned char bit)
{
   unsigned char *p = (unsigned char *)buf;
   size_t i = 0;

   /* Only the 16-byte stride, which is Termcell's: a 256-bit register covers
    * two records and an odd last record goes to the scalar loop. The 12-byte
    * stride the SSE2 and NEON kernels also handle predates the current
    * Termcell and is left to the scalar form. */
   if ((rec == 16) && (off < rec))
     {
        unsigned char pat[32];
        __m256i m;

        memset(pat, 0, sizeof(pat));
        pat[off] = bit;
        pat[rec + off] = bit;
        m = _mm256_loadu_si256((const __m256i *)pat);

        for (; i + 2 <= n; i += 2, p += 32)
          {
             __m256i v = _mm256_loadu_si256((const __m256i *)p);

             _mm256_storeu_si256((__m256i *)p, _mm256_or_si256(v, m));
          }
     }

   for (; i < n; i++, p += rec)
     p[off] |= bit;
}

AVX2_TARGET
void
simd_widen_ascii_avx2(const unsigned char *buf, size_t len, Eina_Unicode *out)
{
   size_t i = 0;

   /* vpmovzxbd widens the low eight bytes of a 128-bit register, so four of
    * them cover 32 input bytes. */
   for (; i + 32 <= len; i += 32)
     {
        const unsigned char *in = buf + i;
        Eina_Unicode *o = out + i;
        __m256i a = _mm256_cvtepu8_epi32(_mm_loadl_epi64((const __m128i *)in));
        __m256i b = _mm256_cvtepu8_epi32(
           _mm_loadl_epi64((const __m128i *)(in + 8)));
        __m256i c = _mm256_cvtepu8_epi32(
           _mm_loadl_epi64((const __m128i *)(in + 16)));
        __m256i d = _mm256_cvtepu8_epi32(
           _mm_loadl_epi64((const __m128i *)(in + 24)));

        _mm256_storeu_si256((__m256i *)o, a);
        _mm256_storeu_si256((__m256i *)(o + 8), b);
        _mm256_storeu_si256((__m256i *)(o + 16), c);
        _mm256_storeu_si256((__m256i *)(o + 24), d);
     }

   for (; i + 8 <= len; i += 8)
     _mm256_storeu_si256((__m256i *)(out + i),
                         _mm256_cvtepu8_epi32(
                            _mm_loadl_epi64((const __m128i *)(buf + i))));

   for (; i < len; i++)
     out[i] = buf[i];
}

#endif
