/* Kernel selection and the runtime kill-switch. */
#include "private.h"
#include "simd.h"
#include <stdlib.h>
#include <string.h>

#if defined(TERMINOLOGY_HAVE_NEON)
static Eina_Bool _use_simd = EINA_TRUE;
#else
static Eina_Bool _use_simd = EINA_FALSE;
#endif

void
simd_init(void)
{
#if defined(TERMINOLOGY_HAVE_NEON)
   const char *s = getenv("TERMINOLOGY_SIMD_DISABLE");

   /* Any non-empty value other than "0" disables. */
   if (s && s[0] && strcmp(s, "0") != 0)
     _use_simd = EINA_FALSE;
#endif
}

Eina_Bool
simd_enabled(void)
{
   return _use_simd;
}

size_t
simd_scan_plain_ascii(const unsigned char *buf, size_t len)
{
#if defined(TERMINOLOGY_HAVE_NEON)
   if (EINA_LIKELY(_use_simd))
     return simd_scan_plain_ascii_neon(buf, len);
#endif
   return simd_scan_plain_ascii_scalar(buf, len);
}
/* Parity tests: each vector kernel must agree with its scalar reference.
 *
 * Buffers are guard-padded so a kernel writing outside its range fails even
 * when the in-range bytes are right, and every length and alignment around the
 * vector width is walked, since the bugs live in the tail and at the seam
 * between the vector body and the scalar remainder. */
#if defined(BINARY_TYTEST)
#include <assert.h>

/* Only the NEON build has two kernels to compare, so the whole harness --
 * helpers included -- is compiled only there. */
#if defined(TERMINOLOGY_HAVE_NEON)

#define GUARD 32
#define GUARD_BYTE 0xA5

/* Deterministic: a parity failure has to be reproducible to be debuggable. */
static unsigned int _seed = 0x9e3779b9;

static unsigned int
_rnd(void)
{
   _seed ^= _seed << 13;
   _seed ^= _seed >> 17;
   _seed ^= _seed << 5;
   return _seed;
}

static unsigned char *
_alloc_guarded(size_t len)
{
   unsigned char *base = malloc(len + 2 * GUARD);

   assert(base != NULL);
   memset(base, GUARD_BYTE, len + 2 * GUARD);
   return base;
}

static Eina_Bool
_guards_intact(const unsigned char *base, size_t len)
{
   size_t i;

   for (i = 0; i < GUARD; i++)
     {
        if (base[i] != GUARD_BYTE)
          return EINA_FALSE;
     }
   for (i = 0; i < GUARD; i++)
     {
        if (base[GUARD + len + i] != GUARD_BYTE)
          return EINA_FALSE;
     }
   return EINA_TRUE;
}

/* Mostly printable ASCII so runs reach the vector body, salted with the exact
 * boundary values the kernels test against. */
static void
_fill(unsigned char *p, size_t len, int density)
{
   size_t i;

   for (i = 0; i < len; i++)
     {
        if ((int)(_rnd() % 100) < density)
          {
             switch (_rnd() % 6)
               {
                case 0: p[i] = 0x00; break;
                case 1: p[i] = 0x1f; break;
                case 2: p[i] = 0x7f; break;
                case 3: p[i] = 0x80; break;
                case 4: p[i] = 0xff; break;
                default: p[i] = (unsigned char)(_rnd() % 0x20); break;
               }
          }
        else
          p[i] = (unsigned char)(0x20 + (_rnd() % 0x5f));
     }
}

static void
_test_scan(void)
{
   size_t len, off;
   int density;

   for (len = 0; len <= 70; len++)
     {
        for (density = 0; density <= 100; density += 10)
          {
             for (off = 0; off < 16; off++)
               {
                  unsigned char *base = _alloc_guarded(off + len);
                  unsigned char *p = base + GUARD + off;

                  _fill(p, len, density);
                  assert(simd_scan_plain_ascii_scalar(p, len) ==
                         simd_scan_plain_ascii_neon(p, len));
                  assert(_guards_intact(base, off + len));
                  free(base);
               }
          }
     }
}

/* Every byte value, at every position, exhaustively. */
static void
_test_every_byte(void)
{
   unsigned int v;
   size_t len, pos;

   for (v = 0; v < 256; v++)
     {
        for (len = 1; len <= 40; len++)
          {
             for (pos = 0; pos < len; pos++)
               {
                  unsigned char buf[64];

                  memset(buf, 'x', sizeof(buf));
                  buf[pos] = (unsigned char)v;
                  assert(simd_scan_plain_ascii_scalar(buf, len) ==
                         simd_scan_plain_ascii_neon(buf, len));
               }
          }
     }
}

#endif

int
tytest_simd_parity(void)
{
#if defined(TERMINOLOGY_HAVE_NEON)
   _test_scan();
   _test_every_byte();
#endif
   /* Without a vector kernel the scalar path is the only path. */
   return 0;
}

#endif
