#include "private.h"
#include "utf8.h"

/* How many bytes the sequence introduced by this lead byte occupies, or 0 if it
 * cannot start one (a continuation byte, or a length this decoder will not
 * carry across a boundary). */
static inline int
_utf8_seq_len(unsigned char d)
{
   if (d < 0x80) return 1;
   if ((d & 0xe0) == 0xc0) return 2;
   if ((d & 0xf0) == 0xe0) return 3;
   if ((d & 0xf8) == 0xf0) return 4;
   return 0;
}

/* True when the 'avail' bytes at 'p' are the beginning of a longer sequence
 * that has simply not arrived yet: a lead byte followed only by continuation
 * bytes, with at least one still missing. */
static Eina_Bool
_utf8_truncated(const char *p, int avail)
{
   int need = _utf8_seq_len((unsigned char)p[0]);
   int i;

   if ((need < 2) || (need > UTF8_CARRY_MAX)) return EINA_FALSE;
   if (avail >= need) return EINA_FALSE;

   for (i = 1; i < avail; i++)
     {
        if (((unsigned char)p[i] & 0xc0) != 0x80) return EINA_FALSE;
     }
   return EINA_TRUE;
}

/* Decode UTF-8 bytes into codepoints.
 *
 * 'buf' must hold 'len' bytes and be NUL-terminated at buf[len], as
 * eina_unicode_utf8_next_get() works on NUL-terminated strings. 'codepoints'
 * must have room for 'len' entries, which is always enough since multibyte
 * sequences only ever shrink the count.
 *
 * Returns the number of codepoints written. '*consumed' gets the number of
 * input bytes decoded; anything left over is a multibyte sequence truncated by
 * the end of the buffer, which the caller is expected to carry over and
 * re-submit in front of the next chunk. At most UTF8_CARRY_MAX - 1 bytes are
 * ever left behind.
 *
 * Truncation is decided from the bytes themselves, so that where the read
 * boundaries fall cannot change how the same input decodes.
 */
int
utf8_to_codepoints(const char *buf, int len, Eina_Unicode *codepoints,
                   int *consumed)
{
   int i = 0, j = 0;

   while (i < len)
     {
        Eina_Unicode g;

        if (buf[i])
          {
             /* Only the last few bytes can possibly hold a cut-off sequence, so
              * lead with that test: it is false for all but the tail of the
              * buffer and keeps this to one comparison per character. */
             if (EINA_UNLIKELY((len - i) < UTF8_CARRY_MAX) &&
                 _utf8_truncated(buf + i, len - i))
               break;
             g = eina_unicode_utf8_next_get(buf, &i);
          }
        else
          {
             /* eina_unicode_utf8_next_get() stops at NUL, so an embedded one
              * has to be stepped over by hand. */
             g = 0;
             i++;
          }
        codepoints[j] = g;
        j++;
     }

   *consumed = i;
   return j;
}

int
codepoint_to_utf8(Eina_Unicode g, char *txt)
{
   if (g < (1 << (7)))
     { // 0xxxxxxx
        txt[0] = g & 0x7f;
        txt[1] = 0;
        return 1;
     }
   else if (g < (1 << (5 + 6)))
     { // 110xxxxx 10xxxxxx
        txt[0] = (char)(0xc0 | ((g >> 6) & 0x1f));
        txt[1] = (char)(0x80 | ((g     ) & 0x3f));
        txt[2] = 0;
        return 2;
     }
   else if (g < (1 << (4 + 6 + 6)))
     { // 1110xxxx 10xxxxxx 10xxxxxx
        txt[0] = (char)(0xe0 | ((g >> 12) & 0x0f));
        txt[1] = (char)(0x80 | ((g >> 6 ) & 0x3f));
        txt[2] = (char)(0x80 | ((g      ) & 0x3f));
        txt[3] = 0;
        return 3;
     }
   else if (g < (1 << (3 + 6 + 6 + 6)))
     { // 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
        txt[0] = (char)(0xf0 | ((g >> 18) & 0x07));
        txt[1] = (char)(0x80 | ((g >> 12) & 0x3f));
        txt[2] = (char)(0x80 | ((g >> 6 ) & 0x3f));
        txt[3] = (char)(0x80 | ((g      ) & 0x3f));
        txt[4] = 0;
        return 4;
     }
   else
     { // error - can't encode this in utf8
        txt[0] = 0;
        return 0;
     }
}
