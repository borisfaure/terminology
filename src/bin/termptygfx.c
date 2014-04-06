#include "private.h"
#include <Elementary.h>
#include "termptygfx.h"

/* translates VT100 ACS escape codes to Unicode values.
 * Based on rxvt-unicode screen.C table.
 */
static const unsigned short vt100_to_unicode[62] =
{
// ?       ?       ?       ?       ?       ?       ?
// A=UPARR B=DNARR C=RTARR D=LFARR E=FLBLK F=3/4BL G=SNOMN
   0x2191, 0x2193, 0x2192, 0x2190, 0x2588, 0x259a, 0x2603,
// H=      I=      J=      K=      L=      M=      N=
   0,      0,      0,      0,      0,      0,      0,
// O=      P=      Q=      R=      S=      T=      U=
   0,      0,      0,      0,      0,      0,      0,
// V=      W=      X=      Y=      Z=      [=      \=
   0,      0,      0,      0,      0,      0,      0,
// ?       ?       v->0    v->1    v->2    v->3    v->4
// ]=      ^=      _=SPC   `=DIAMN a=HSMED b=HT    c=FF
   0,      0,      0x0020, 0x25c6, 0x2592, 0x2409, 0x240c,
// v->5    v->6    v->7    v->8    v->9    v->a    v->b   
// d=CR    e=LF    f=DEGRE g=PLSMN h=NL    i=VT    j=SL-BR
   0x240d, 0x240a, 0x00b0, 0x00b1, 0x2424, 0x240b, 0x2518,
// v->c    v->d    v->e    v->f    v->10   v->11   v->12   
// k=SL-TR l=SL-TL m=SL-BL n=SL-+  o=SL-T1 p=SL-T2 q=SL-HZ
   0x2510, 0x250c, 0x2514, 0x253c, 0x23ba, 0x23bb, 0x2500,
// v->13   v->14   v->15   v->16   v->17   v->18   v->19   
// r=SL-T4 s=SL-T5 t=SL-VR u=SL-VL v=SL-HU w=Sl-HD x=SL-VT
   0x23bc, 0x23bd, 0x251c, 0x2524, 0x2534, 0x252c, 0x2502,
// v->1a   v->1b   b->1c   v->1d   v->1e/a3 v->1f
// y=LT-EQ z=GT-EQ {=PI    |=NOTEQ }=POUND ~=DOT
   0x2264, 0x2265, 0x03c0, 0x2260, 0x20a4, 0x00b7
};

Eina_Unicode
_termpty_charset_trans(Eina_Unicode g, Termstate *state)
{
   switch (state->charsetch)
     {
      case '0': /* DEC Special Character & Line Drawing Set */
        if ((g >= 0x41) && (g <= 0x7e) &&
            (vt100_to_unicode[g - 0x41]))
          return vt100_to_unicode[g - 0x41];
        break;
      case 'A': /* UK, replaces # with GBP */
        if (g == '#') return 0x20a4;
        break;
      default:
        break;
     }
   if (state->att.fraktur)
     {
        if (g >= 'a' && g <= 'z')
          {
             g += 0x1d51e - 'a';
          }
        else if (g >= 'A' && g <= 'Z')
          {
             g += 0x1d504 - 'A';
          }
     }
   return g;
}
