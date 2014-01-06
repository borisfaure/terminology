#ifndef _TERMPTY_DBL_H__
#define _TERMPTY_DBL_H__ 1

Eina_Bool _termpty_is_dblwidth_slow_get(Termpty *ty, int g);

static inline Eina_Bool
_termpty_is_dblwidth_get(Termpty *ty, int g)
{
#if defined(SUPPORT_DBLWIDTH)
   // check for east asian full-width (F), half-width (H), wide (W),
   // narrow (Na) or ambiguous (A) codepoints
   // ftp://ftp.unicode.org/Public/UNIDATA/EastAsianWidth.txt

   // optimize for latin1 non-ambiguous
   if (g <= 0xa0)
     return EINA_FALSE;
   // (F)
   if ((g == 0x3000) ||
       ((g >= 0xff01) && (g <= 0xff60)) ||
       ((g >= 0xffe0) && (g <= 0xffe6)))
     return EINA_TRUE;

   return _termpty_is_dblwidth_slow_get(ty, g);
#else
   return EINA_FALSE;
#endif
}

#endif
