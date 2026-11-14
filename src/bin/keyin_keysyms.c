#include "private.h"

#include <Elementary.h>
#include "keyin_encode.h"

#include <string.h>

#include "keyin_keysyms.h"

static int
_hex_digit(char c)
{
   if ((c >= '0') && (c <= '9'))
     return c - '0';
   if ((c >= 'a') && (c <= 'f'))
     return c - 'a' + 10;
   if ((c >= 'A') && (c <= 'F'))
     return c - 'A' + 10;
   return -1;
}

Eina_Unicode
keyin_keysym_codepoint(const char *name)
{
   const struct keyin_keysym *ks;
   size_t len;
   size_t i;

   if (!name || !name[0])
     return 0;

   len = strlen(name);
   if ((len == 1) && (name[0] >= 0x20) && (name[0] < 0x7f))
     return name[0];

   /* XKeysymToString(3) names a Unicode keysym "U" and 4 to 6 hex digits */
   if ((name[0] == 'U') && (len >= 5) && (len <= 7))
     {
        Eina_Unicode cp = 0;

        for (i = 1; i < len; i++)
          {
             int d = _hex_digit(name[i]);

             if (d < 0)
               break;
             cp = cp * 16 + d;
          }
        if (i == len)
          return cp;
     }

   ks = keyin_keysym_lookup(name, len);
   return ks ? ks->cp : 0;
}
