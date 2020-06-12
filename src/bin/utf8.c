#include "private.h"
#include "utf8.h"

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
