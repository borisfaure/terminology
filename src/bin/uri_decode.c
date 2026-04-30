#include "uri_decode.h"

void
uri_percent_decode_inplace(char *s)
{
   char *r;  /* read cursor */
   char *w;  /* write cursor */

   if (!s) return;

   for (r = w = s; *r; )
     {
        if (r[0] == '%' && r[1] && r[2])
          {
             int hi = -1, lo = -1;
             char c1 = r[1], c2 = r[2];

             if      (c1 >= '0' && c1 <= '9') hi = c1 - '0';
             else if (c1 >= 'A' && c1 <= 'F') hi = c1 - 'A' + 10;
             else if (c1 >= 'a' && c1 <= 'f') hi = c1 - 'a' + 10;

             if      (c2 >= '0' && c2 <= '9') lo = c2 - '0';
             else if (c2 >= 'A' && c2 <= 'F') lo = c2 - 'A' + 10;
             else if (c2 >= 'a' && c2 <= 'f') lo = c2 - 'a' + 10;

             if (hi >= 0 && lo >= 0)
               {
                  char decoded = (char)((hi << 4) | lo);
                  if (decoded == '\0')
                    {
                       /* %00 NUL byte: truncate here.
                        *
                        * A NUL mid-path silently truncates at all downstream
                        * strlen() / open(2) calls, enabling path-confusion attacks:
                        *   file:///etc/passwd%00.safe.txt  → opens /etc/passwd
                        *
                        * RFC 3986 §2.2 reserves NUL for transport, not paths.
                        * Truncation here matches what the OS would silently do
                        * anyway, but makes the intent explicit and auditable. */
                       break;
                    }
                  *w++ = decoded;
                  r += 3;
                  continue;
               }
          }
        *w++ = *r++;
     }
   *w = '\0';
}
