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

size_t
simd_scan_plain_ascii_u32_scalar(const Eina_Unicode *buf, size_t len)
{
   size_t i;

   for (i = 0; i < len; i++)
     {
        Eina_Unicode g = buf[i];

        if ((g < 0x20) || (g >= 0x7f)) return i;
     }
   return len;
}

size_t
simd_rscan_nonzero_scalar(const unsigned char *buf, size_t len)
{
   while (len > 0)
     {
        if (buf[len - 1]) return len;
        len--;
     }
   return 0;
}

void
simd_records_or_byte_scalar(void *buf, size_t n, size_t rec, size_t off,
                            unsigned char bit)
{
   unsigned char *p = (unsigned char *)buf + off;
   size_t i;

   for (i = 0; i < n; i++, p += rec)
     *p |= bit;
}

void
simd_widen_ascii_scalar(const unsigned char *buf, size_t len, Eina_Unicode *out)
{
   size_t i;

   for (i = 0; i < len; i++)
     out[i] = buf[i];
}
