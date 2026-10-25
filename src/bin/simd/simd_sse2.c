/* SSE2 implementations of the intake kernels. SSE2 is baseline on x86-64, so
 * like NEON on aarch64 no runtime probe is needed. */
#include "private.h"
#include "simd.h"
#include <string.h>

#if defined(TERMINOLOGY_HAVE_SSE2)

#include <emmintrin.h>

size_t
simd_scan_plain_ascii_sse2(const unsigned char *buf, size_t len)
{
   const __m128i lo = _mm_set1_epi8(0x20);
   const __m128i hi = _mm_set1_epi8(0x7e);
   size_t i = 0;

   for (; i + 16 <= len; i += 16)
     {
        __m128i v = _mm_loadu_si128((const __m128i *)(buf + i));
        __m128i bad;
        unsigned int m;

        /* c < 0x20 || c >= 0x7f. Signed compares cover both: bytes with the
         * high bit set are negative, so they fail the first test, and 0x7f
         * alone fails "signed > 0x7e". */
        bad = _mm_or_si128(_mm_cmplt_epi8(v, lo), _mm_cmpgt_epi8(v, hi));
        m = (unsigned int)_mm_movemask_epi8(bad);
        if (m) return i + (size_t)__builtin_ctz(m);
     }

   /* Tail: fewer than 16 bytes left. */
   for (; i < len; i++)
     {
        unsigned char c = buf[i];

        if ((c < 0x20) || (c >= 0x7f)) return i;
     }
   return len;
}

size_t
simd_scan_plain_ascii_u32_sse2(const Eina_Unicode *buf, size_t len)
{
   /* SSE2 has no unsigned compare: a wrapping subtraction folds g < 0x20
    * and g >= 0x80000000 into values with a byte whose top bit is set, and a
    * signed compare takes the rest of g >= 0x7f. */
   const __m128i bias = _mm_set1_epi32(0x20);
   const __m128i span = _mm_set1_epi32(0x7e);
   size_t i = 0;

   for (; i + 4 <= len; i += 4)
     {
        __m128i v = _mm_loadu_si128((const __m128i *)(buf + i));
        __m128i under = _mm_sub_epi32(v, bias);
        __m128i over = _mm_cmpgt_epi32(v, span);
        /* movemask keeps the top bit of every byte, so lane k owns bits
         * 4k..4k+3 and the lowest set bit gives the first offending lane
         * through ctz >> 2. 'over' sets all four bits of a lane for
         * 0x7f..0x7fffffff. 'under' catches the rest: g - 0x20 has a byte
         * with its top bit set for every g below 0x20 and every g from
         * 0x80000000 up (the low byte for 0x80000000..0x8000001f, the high
         * byte beyond), while it is below 0x80 in every byte for 0x20..0x7e. */
        unsigned int m = (unsigned int)(_mm_movemask_epi8(under) |
                                        _mm_movemask_epi8(over));

        if (m) return i + (size_t)(__builtin_ctz(m) >> 2);
     }

   for (; i < len; i++)
     {
        Eina_Unicode g = buf[i];

        if ((g < 0x20) || (g >= 0x7f)) return i;
     }
   return len;
}

size_t
simd_rscan_nonzero_sse2(const unsigned char *buf, size_t len)
{
   size_t i = len;

   while (i >= 16)
     {
        __m128i v = _mm_loadu_si128((const __m128i *)(buf + i - 16));
        unsigned int m;

        /* 0xff in every lane whose byte is zero. */
        m = (unsigned int)_mm_movemask_epi8(_mm_cmpeq_epi8(v,
                                                           _mm_setzero_si128()));
        if (m != 0xffffu)
          {
             /* Highest clear bit is the last non-zero byte of this block. */
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

void
simd_records_or_byte_sse2(void *buf, size_t n, size_t rec, size_t off,
                          unsigned char bit)
{
   unsigned char *p = (unsigned char *)buf;
   size_t i = 0;

   /* A 12-byte stride lines up with the vector every four records, so the mask
    * repeats every 48 bytes. A 16-byte stride gives each record its own vector,
    * so the mask is one register. Other strides use the scalar form. */
   if ((rec == 12) && (off < rec))
     {
        unsigned char pat[48];
        __m128i m0, m1, m2;

        memset(pat, 0, sizeof(pat));
        pat[off] = bit;
        pat[rec + off] = bit;
        pat[2 * rec + off] = bit;
        pat[3 * rec + off] = bit;
        m0 = _mm_loadu_si128((const __m128i *)pat);
        m1 = _mm_loadu_si128((const __m128i *)(pat + 16));
        m2 = _mm_loadu_si128((const __m128i *)(pat + 32));

        for (; i + 4 <= n; i += 4, p += 48)
          {
             __m128i *q = (__m128i *)p;

             _mm_storeu_si128(q,     _mm_or_si128(_mm_loadu_si128(q),     m0));
             _mm_storeu_si128(q + 1, _mm_or_si128(_mm_loadu_si128(q + 1), m1));
             _mm_storeu_si128(q + 2, _mm_or_si128(_mm_loadu_si128(q + 2), m2));
          }
     }
   else if ((rec == 16) && (off < rec))
     {
        unsigned char pat[16];
        __m128i m;

        memset(pat, 0, sizeof(pat));
        pat[off] = bit;
        m = _mm_loadu_si128((const __m128i *)pat);

        for (; i + 4 <= n; i += 4, p += 64)
          {
             __m128i *q = (__m128i *)p;

             _mm_storeu_si128(q,     _mm_or_si128(_mm_loadu_si128(q),     m));
             _mm_storeu_si128(q + 1, _mm_or_si128(_mm_loadu_si128(q + 1), m));
             _mm_storeu_si128(q + 2, _mm_or_si128(_mm_loadu_si128(q + 2), m));
             _mm_storeu_si128(q + 3, _mm_or_si128(_mm_loadu_si128(q + 3), m));
          }
     }

   for (; i < n; i++, p += rec)
     p[off] |= bit;
}

void
simd_widen_ascii_sse2(const unsigned char *buf, size_t len, Eina_Unicode *out)
{
   const __m128i zero = _mm_setzero_si128();
   size_t i = 0;

   for (; i + 16 <= len; i += 16)
     {
        __m128i v = _mm_loadu_si128((const __m128i *)(buf + i));
        /* Zero-extend 8 -> 16 -> 32 bits in two steps per half. */
        __m128i w0 = _mm_unpacklo_epi8(v, zero);
        __m128i w1 = _mm_unpackhi_epi8(v, zero);

        _mm_storeu_si128((__m128i *)(out + i +  0), _mm_unpacklo_epi16(w0, zero));
        _mm_storeu_si128((__m128i *)(out + i +  4), _mm_unpackhi_epi16(w0, zero));
        _mm_storeu_si128((__m128i *)(out + i +  8), _mm_unpacklo_epi16(w1, zero));
        _mm_storeu_si128((__m128i *)(out + i + 12), _mm_unpackhi_epi16(w1, zero));
     }

   for (; i < len; i++)
     out[i] = buf[i];
}

#endif
