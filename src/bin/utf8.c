#include "private.h"
#include "utf8.h"

/* Decode UTF-8 bytes into codepoints.
 *
 * 'buf' must hold 'len' bytes and be NUL-terminated at buf[len], as
 * eina_unicode_utf8_next_get() works on NUL-terminated strings. 'codepoints'
 * must have room for 'len' entries, which is always enough since multibyte
 * sequences only ever shrink the count.
 *
 * Returns the number of codepoints written. '*consumed' gets the number of
 * input bytes decoded; anything left over is a multibyte sequence truncated by
 * the end of the buffer, which the caller carries over and re-submits in front
 * of the next chunk.
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
             int prev_i = i;

             g = eina_unicode_utf8_next_get(buf, &i);
             /* EFL maps invalid and truncated sequences alike into the
              * surrogate-escape range; near the end of the buffer, assume
              * truncation and hand the tail back to the caller. */
             if ((0xdc80 <= g) && (g <= 0xdcff) &&
                 ((len - prev_i) <= UTF8_CARRY_MAX))
               {
                  i = prev_i;
                  break;
               }
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
