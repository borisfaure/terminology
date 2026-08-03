/* Scalar reference implementations. The vector kernels must agree with these
 * byte for byte; keep them obvious rather than clever. */
#include "private.h"
#include "simd.h"

size_t
simd_scan_plain_ascii_scalar(const unsigned char *buf, size_t len)
{
   size_t i;

   for (i = 0; i < len; i++)
     {
        unsigned char c = buf[i];

        if ((c < 0x20) || (c >= 0x7f)) return i;
     }
   return len;
}

void
simd_widen_ascii_scalar(const unsigned char *buf, size_t len, Eina_Unicode *out)
{
   size_t i;

   for (i = 0; i < len; i++)
     out[i] = buf[i];
}
