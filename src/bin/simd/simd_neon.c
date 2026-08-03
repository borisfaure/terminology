/* NEON implementations of the intake kernels, using arm_neon.h intrinsics as
 * EFL's own aarch64 code does. */
#include "private.h"
#include "simd.h"
#include <string.h>

#if defined(TERMINOLOGY_HAVE_NEON)

#include <arm_neon.h>

size_t
simd_scan_plain_ascii_neon(const unsigned char *buf, size_t len)
{
   const uint8x16_t lo    = vdupq_n_u8(0x20);
   const uint8x16_t hi    = vdupq_n_u8(0x7f);
   size_t i = 0;

   for (; i + 16 <= len; i += 16)
     {
        uint8x16_t v = vld1q_u8(buf + i);
        uint8x16_t bad;
        uint64_t m;

        /* c < 0x20 || c >= 0x7f. The second test folds DEL and every
         * high-bit-set byte into one comparison, which is why the range is
         * expressed as ">= 0x7f" rather than "== 0x7f || >= 0x80". */
        bad = vorrq_u8(vcltq_u8(v, lo), vcgeq_u8(v, hi));

        /* Collapse the 16 lanes into 16 nibbles of one 64-bit word, so the
         * first offending byte is a trailing-zero count over four. aarch64 has
         * no PMOVMSKB equivalent. */
        m = vget_lane_u64(vreinterpret_u64_u8(
                             vshrn_n_u16(vreinterpretq_u16_u8(bad), 4)), 0);
        if (m) return i + (size_t)(__builtin_ctzll(m) >> 2);
     }

   /* Tail: fewer than 16 bytes left. */
   for (; i < len; i++)
     {
        unsigned char c = buf[i];

        if ((c < 0x20) || (c >= 0x7f)) return i;
     }
   return len;
}


#endif
