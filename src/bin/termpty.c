#include "private.h"

#include <Elementary.h>
#include "termpty.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <string.h>
#include <errno.h>

/* specific log domain to help debug only terminal code parser */
static int _termpty_log_dom = -1;

#undef CRITICAL
#undef ERR
#undef WRN
#undef INF
#undef DBG

#define CRITICAL(...) EINA_LOG_DOM_CRIT(_termpty_log_dom, __VA_ARGS__)
#define ERR(...)      EINA_LOG_DOM_ERR(_termpty_log_dom, __VA_ARGS__)
#define WRN(...)      EINA_LOG_DOM_WARN(_termpty_log_dom, __VA_ARGS__)
#define INF(...)      EINA_LOG_DOM_INFO(_termpty_log_dom, __VA_ARGS__)
#define DBG(...)      EINA_LOG_DOM_DBG(_termpty_log_dom, __VA_ARGS__)

void
termpty_init(void)
{
   if (_termpty_log_dom >= 0) return;

   _termpty_log_dom = eina_log_domain_register("termpty", NULL);
   if (_termpty_log_dom < 0)
     EINA_LOG_CRIT("could not create log domain 'termpty'.");
}

void
termpty_shutdown(void)
{
   if (_termpty_log_dom < 0) return;
   eina_log_domain_unregister(_termpty_log_dom);
   _termpty_log_dom = -1;
}

#if defined(SUPPORT_DBLWIDTH)
static Eina_Bool
_is_dblwidth_get(Termpty *ty, int g)
{
   // check for east asian full-width (F), half-width (H), wide (W),
   // narrow (Na) or ambiguous (A) codepoints
   // ftp://ftp.unicode.org/Public/UNIDATA/EastAsianWidth.txt
   
   // optimize for latin1 non-ambiguous
   if (g <= 0xa0)
     return EINA_FALSE;
   // (F)
   if ((g == 0x3000) ||
       ((g >= 0xff01) && (g <= 0xffe6)))
     return EINA_TRUE;
   // (W)
   if (((g >= 0x1100) && (g <= 0x11ff)) ||
       ((g >= 0x2329) && (g <= 0x232A)) ||
       ((g >= 0x2E80) && (g <= 0x4dbf)) ||
       ((g >= 0x4e00) && (g <= 0x9fff)) ||
       ((g >= 0xa000) && (g <= 0xa4c6)) ||
       ((g >= 0xa960) && (g <= 0xa97c)) ||
       ((g >= 0xac00) && (g <= 0xd7a3)) ||
       ((g >= 0xd7b0) && (g <= 0xd7fb)) ||
       ((g >= 0xf900) && (g <= 0xfaff)) ||
       ((g >= 0xfe10) && (g <= 0xfe6b)) ||
       ((g >= 0x1b000) && (g <= 0x1b001)) ||
       ((g >= 0x1f200) && (g <= 0x1f202)) ||
       ((g >= 0x1f210) && (g <= 0x1f251)) ||
       ((g >= 0x20000) && (g <= 0x2fffd)) ||
       ((g >= 0x30000) && (g <= 0x3FFFD)))
     return EINA_TRUE;
   // (A)
   if (ty->state.cjk_ambiguous_wide)
     {
        // grep ';A #' EastAsianWidth.txt | wc -l
        // :(
        if ((g == 0x00a1) ||
            (g == 0x00a4) ||
            ((g >= 0x00a7) && (g <= 0x00a8)) ||
            (g == 0x00aa) ||
            ((g >= 0x00ad) && (g <= 0x00ae)) ||
            ((g >= 0x00b0) && (g <= 0x00bf)) ||
            (g == 0x00c6) ||
            (g == 0x00d0) ||
            ((g >= 0x00d7) && (g <= 0x00d8)) ||
            ((g >= 0x00de) && (g <= 0x00df)) ||
            (g == 0x00e0) ||
            (g == 0x00e1) ||
            (g == 0x00e6) ||
            ((g >= 0x00e8) && (g <= 0x00e9)) ||
            (g == 0x00ea) ||
            ((g >= 0x00ec) && (g <= 0x00ed)) ||
            (g == 0x00f0) ||
            ((g >= 0x00f2) && (g <= 0x00f3)) ||
            ((g >= 0x00f7) && (g <= 0x00f9)) ||
            (g == 0x00fa) ||
            (g == 0x00fc) ||
            (g == 0x00fe) ||
            (g == 0x0101) ||
            (g == 0x0111) ||
            (g == 0x0113) ||
            (g == 0x011b) ||
            ((g >= 0x0126) && (g <= 0x0127)) ||
            (g == 0x012b) ||
            ((g >= 0x0131) && (g <= 0x0133)) ||
            (g == 0x0138) ||
            ((g >= 0x013f) && (g <= 0x0142)) ||
            (g == 0x0144) ||
            ((g >= 0x0148) && (g <= 0x014b)) ||
            (g == 0x014d) ||
            ((g >= 0x0152) && (g <= 0x0153)) ||
            ((g >= 0x0166) && (g <= 0x0167)) ||
            (g == 0x016b) ||
            (g == 0x01ce) ||
            (g == 0x01d0) ||
            (g == 0x01d2) ||
            (g == 0x01d4) ||
            (g == 0x01d6) ||
            (g == 0x01d8) ||
            (g == 0x01da) ||
            (g == 0x01dc) ||
            (g == 0x0251) ||
            (g == 0x0261) ||
            (g == 0x02c4) ||
            (g == 0x02c7) ||
            (g == 0x02c9) ||
            ((g >= 0x02ca) && (g <= 0x02cb)) ||
            (g == 0x02cd) ||
            (g == 0x02d0) ||
            ((g >= 0x02d8) && (g <= 0x02d9)) ||
            ((g >= 0x02da) && (g <= 0x02db)) ||
            (g == 0x02dd) ||
            (g == 0x02df) ||
            ((g >= 0x0300) && (g <= 0x036f)) ||
            ((g >= 0x0391) && (g <= 0x03c9)) ||
            (g == 0x0401) ||
            ((g >= 0x0410) && (g <= 0x044f)) ||
            (g == 0x0451) ||
            (g == 0x2010) ||
            ((g >= 0x2013) && (g <= 0x2016)) ||
            ((g >= 0x2018) && (g <= 0x2019)) ||
            (g == 0x201c) ||
            (g == 0x201d) ||
            ((g >= 0x2020) && (g <= 0x2022)) ||
            ((g >= 0x2024) && (g <= 0x2027)) ||
            (g == 0x2030) ||
            ((g >= 0x2032) && (g <= 0x2033)) ||
            (g == 0x2035) ||
            (g == 0x203b) ||
            (g == 0x203e) ||
            (g == 0x2074) ||
            (g == 0x207f) ||
            ((g >= 0x2081) && (g <= 0x2084)) ||
            (g == 0x20ac) ||
            (g == 0x2103) ||
            (g == 0x2105) ||
            (g == 0x2109) ||
            (g == 0x2113) ||
            (g == 0x2116) ||
            ((g >= 0x2121) && (g <= 0x2122)) ||
            (g == 0x2126) ||
            (g == 0x212b) ||
            ((g >= 0x2153) && (g <= 0x2154)) ||
            ((g >= 0x215b) && (g <= 0x215e)) ||
            ((g >= 0x2160) && (g <= 0x216b)) ||
            ((g >= 0x2170) && (g <= 0x2179)) ||
            ((g >= 0x2189) && (g <= 0x2199)) ||
            ((g >= 0x21b8) && (g <= 0x21b9)) ||
            (g == 0x21d2) ||
            (g == 0x21d4) ||
            (g == 0x21e7) ||
            (g == 0x2200) ||
            ((g >= 0x2202) && (g <= 0x2203)) ||
            ((g >= 0x2207) && (g <= 0x2208)) ||
            (g == 0x220b) ||
            (g == 0x220f) ||
            (g == 0x2211) ||
            (g == 0x2215) ||
            (g == 0x221a) ||
            ((g >= 0x221d) && (g <= 0x221f)) ||
            (g == 0x2220) ||
            (g == 0x2223) ||
            (g == 0x2225) ||
            ((g >= 0x2227) && (g <= 0x222e)) ||
            ((g >= 0x2234) && (g <= 0x2237)) ||
            ((g >= 0x223c) && (g <= 0x223d)) ||
            (g == 0x2248) ||
            (g == 0x224c) ||
            (g == 0x2252) ||
            ((g >= 0x2260) && (g <= 0x2261)) ||
            ((g >= 0x2264) && (g <= 0x2267)) ||
            ((g >= 0x226a) && (g <= 0x226b)) ||
            ((g >= 0x226e) && (g <= 0x226f)) ||
            ((g >= 0x2282) && (g <= 0x2283)) ||
            ((g >= 0x2286) && (g <= 0x2287)) ||
            (g == 0x2295) ||
            (g == 0x2299) ||
            (g == 0x22a5) ||
            (g == 0x22bf) ||
            (g == 0x2312) ||
            ((g >= 0x2460) && (g <= 0x2595)) ||
            ((g >= 0x25a0) && (g <= 0x25bd)) ||
            ((g >= 0x25c0) && (g <= 0x25c1)) ||
            ((g >= 0x25c6) && (g <= 0x25c7)) ||
            (g == 0x25c8) ||
            (g == 0x25cb) ||
            ((g >= 0x25ce) && (g <= 0x25cf)) ||
            ((g >= 0x25d0) && (g <= 0x25d1)) ||
            ((g >= 0x25e2) && (g <= 0x25e3)) ||
            ((g >= 0x25e4) && (g <= 0x25e5)) ||
            (g == 0x25ef) ||
            ((g >= 0x2605) && (g <= 0x2606)) ||
            (g == 0x2609) ||
            ((g >= 0x260e) && (g <= 0x260f)) ||
            ((g >= 0x2614) && (g <= 0x2615)) ||
            (g == 0x261c) ||
            (g == 0x261e) ||
            (g == 0x2640) ||
            (g == 0x2642) ||
            ((g >= 0x2660) && (g <= 0x2661)) ||
            ((g >= 0x2663) && (g <= 0x2665)) ||
            ((g >= 0x2667) && (g <= 0x266a)) ||
            ((g >= 0x266c) && (g <= 0x266d)) ||
            (g == 0x266f) ||
            ((g >= 0x269e) && (g <= 0x269f)) ||
            ((g >= 0x26be) && (g <= 0x26bf)) ||
            ((g >= 0x26c4) && (g <= 0x26cd)) ||
            (g == 0x26cf) ||
            ((g >= 0x26d0) && (g <= 0x26e1)) ||
            (g == 0x26e3) ||
            ((g >= 0x26e8) && (g <= 0x26ff)) ||
            (g == 0x273d) ||
            (g == 0x2757) ||
            ((g >= 0x2776) && (g <= 0x277f)) ||
            ((g >= 0x2b55) && (g <= 0x2b59)) ||
            ((g >= 0x3248) && (g <= 0x324f)) ||
            ((g >= 0xe000) && (g <= 0xf8ff)) ||
            ((g >= 0xfe00) && (g <= 0xfe0f)) ||
            (g == 0xfffd) ||
            ((g >= 0x1f100) && (g <= 0x1f12d)) ||
            ((g >= 0x1f130) && (g <= 0x1f169)) ||
            ((g >= 0x1f170) && (g <= 0x1f19a)) ||
            ((g >= 0xe0100) && (g <= 0xe01ef)) ||
            ((g >= 0xf0000) && (g <= 0xffffd)) ||
            ((g >= 0x100000) && (g <= 0x10fffd)))
          return EINA_TRUE;
     }
   
   // Na, H -> not checked
   return EINA_FALSE;
}
#endif

static void
_text_clear(Termpty *ty, Termcell *cells, int count, int val, Eina_Bool inherit_att)
{
   int i;
   Termatt clear;

   memset(&clear, 0, sizeof(clear));
   if (inherit_att)
     {
        for (i = 0; i < count; i++)
          {
             cells[i].codepoint = val;
             cells[i].att = ty->state.att;
          }
     }
   else
     {
        for (i = 0; i < count; i++)
          {
             cells[i].codepoint = val;
             cells[i].att = clear;
          }
     }
}

static void
_text_copy(Termpty *ty __UNUSED__, Termcell *cells, Termcell *dest, int count)
{
   memcpy(dest, cells, sizeof(*(cells)) * count);
}

static void
_text_save_top(Termpty *ty)
{
   Termsave *ts;

   if (ty->backmax <= 0) return;
   ts = malloc(sizeof(Termsave) + ((ty->w - 1) * sizeof(Termcell)));
   ts->w = ty->w;
   _text_copy(ty, ty->screen, ts->cell, ty->w);
   if (!ty->back) ty->back = calloc(1, sizeof(Termsave *) * ty->backmax);
   if (ty->back[ty->backpos]) free(ty->back[ty->backpos]);
   ty->back[ty->backpos] = ts;
   ty->backpos++;
   if (ty->backpos >= ty->backmax) ty->backpos = 0;
   ty->backscroll_num++;
   if (ty->backscroll_num >= ty->backmax) ty->backscroll_num = ty->backmax - 1;
}

static void
_text_scroll(Termpty *ty)
{
   Termcell *cells = NULL, *cells2;
   int y, start_y = 0, end_y = ty->h - 1;

   if (ty->state.scroll_y2 != 0)
     {
        start_y = ty->state.scroll_y1;
        end_y = ty->state.scroll_y2 - 1;
     }
   else
     {
        if (!ty->altbuf)
          {
             _text_save_top(ty);
             if (ty->cb.scroll.func) ty->cb.scroll.func(ty->cb.scroll.data);
          }
        else
          if (ty->cb.cancel_sel.func)
            ty->cb.cancel_sel.func(ty->cb.cancel_sel.data);
     }
   DBG("... scroll!!!!! [%i->%i]", start_y, end_y);
   cells2 = &(ty->screen[end_y * ty->w]);
   for (y = start_y; y < end_y; y++)
     {
        cells = &(ty->screen[y * ty->w]);
        cells2 = &(ty->screen[(y + 1) * ty->w]);
        _text_copy(ty, cells2, cells, ty->w);
     }
   _text_clear(ty, cells2, ty->w, ' ', EINA_TRUE);
}

static void
_text_scroll_rev(Termpty *ty)
{
   Termcell *cells, *cells2 = NULL;
   int y, start_y = 0, end_y = ty->h - 1;

   if (ty->state.scroll_y2 != 0)
     {
        start_y = ty->state.scroll_y1;
        end_y = ty->state.scroll_y2 - 1;
     }
   DBG("... scroll rev!!!!! [%i->%i]", start_y, end_y);
   cells = &(ty->screen[end_y * ty->w]);
   for (y = end_y; y > start_y; y--)
     {
        cells = &(ty->screen[(y - 1) * ty->w]);
        cells2 = &(ty->screen[y * ty->w]);
        _text_copy(ty, cells, cells2, ty->w);
     }
   _text_clear(ty, cells, ty->w, ' ', EINA_TRUE);
}

static void
_text_scroll_test(Termpty *ty)
{
   int e = ty->h;

   if (ty->state.scroll_y2 != 0) e = ty->state.scroll_y2;
   if (ty->state.cy >= e)
     {
        _text_scroll(ty);
        ty->state.cy = e - 1;
     }
}

static void
_text_scroll_rev_test(Termpty *ty)
{
   int b = 0;

   if (ty->state.scroll_y2 != 0) b = ty->state.scroll_y1;
   if (ty->state.cy < b)
     {
        _text_scroll_rev(ty);
        ty->state.cy = b;
     }
}

/* translates VT100 ACS escape codes to Unicode values.
 * Based on rxvt-unicode screen.C table.
 */
static const int vt100_to_unicode[62] =
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

static void
_text_append(Termpty *ty, const int *codepoints, int len)
{
   Termcell *cells;
   int i, j;

   cells = &(ty->screen[ty->state.cy * ty->w]);
   for (i = 0; i < len; i++)
     {
        int g;

        if (ty->state.wrapnext)
          {
             cells[ty->state.cx].att.autowrapped = 1;
             ty->state.wrapnext = 0;
             ty->state.cx = 0;
             ty->state.cy++;
             _text_scroll_test(ty);
             cells = &(ty->screen[ty->state.cy * ty->w]);
          }
        if (ty->state.insert)
          {
             for (j = ty->w - 1; j > ty->state.cx; j--)
               cells[j] = cells[j - 1];
          }

        g = codepoints[i];
        switch (ty->state.charsetch)
          {
           case '0': /* DEC Special Character & Line Drawing Set */
              if ((g >= 0x41) && (g <= 0x7e) && 
                  (vt100_to_unicode[g - 0x41]))
                g = vt100_to_unicode[g - 0x41];
              break;
           case 'A': /* UK, replaces # with GBP */
              if (g == '#') g = 0x20a4;
              break;
           default:
             break;
          }

        cells[ty->state.cx].codepoint = g;
        cells[ty->state.cx].att = ty->state.att;
#if defined(SUPPORT_DBLWIDTH)
        cells[ty->state.cx].att.dblwidth = _is_dblwidth_get(ty, g);
        if ((cells[ty->state.cx].att.dblwidth) && (ty->state.cx < (ty->w - 1)))
          {
             cells[ty->state.cx + 1].codepoint = 0;
             cells[ty->state.cx + 1].att = cells[ty->state.cx].att;
          }
#endif        
        if (ty->state.wrap)
          {
             ty->state.wrapnext = 0;
#if defined(SUPPORT_DBLWIDTH)
             if (cells[ty->state.cx].att.dblwidth)
               {
                  if (ty->state.cx >= (ty->w - 2)) ty->state.wrapnext = 1;
                  else ty->state.cx += 2;
               }
             else
               {
                  if (ty->state.cx >= (ty->w - 1)) ty->state.wrapnext = 1;
                  else ty->state.cx++;
               }
#else
             if (ty->state.cx >= (ty->w - 1)) ty->state.wrapnext = 1;
             else ty->state.cx++;
#endif
          }
        else
          {
             ty->state.wrapnext = 0;
             ty->state.cx++;
#if defined(SUPPORT_DBLWIDTH)
             if (cells[ty->state.cx].att.dblwidth)
               {
                  ty->state.cx++;
                  if (ty->state.cx >= (ty->w - 1))
                    ty->state.cx = ty->w - 2;
               }
             else
               {
                  if (ty->state.cx >= ty->w)
                    ty->state.cx = ty->w - 1;
               }
#else
             if (ty->state.cx >= ty->w)
               ty->state.cx = ty->w - 1;
#endif
          }
     }
}

static void
_term_write(Termpty *ty, const char *txt, int size)
{
   if (write(ty->fd, txt, size) < 0) ERR("write: %s", strerror(errno));
}
#define _term_txt_write(ty, txt) _term_write(ty, txt, sizeof(txt) - 1)

#define CLR_END   0
#define CLR_BEGIN 1
#define CLR_ALL   2

static void
_clear_line(Termpty *ty, int mode, int limit)
{
   Termcell *cells;
   int n = 0;

   cells = &(ty->screen[ty->state.cy * ty->w]);
   switch (mode)
     {
      case CLR_END:
        n = ty->w - ty->state.cx;
        cells = &(cells[ty->state.cx]);
        break;
      case CLR_BEGIN:
        n = ty->state.cx + 1;
        break;
      case CLR_ALL:
        n = ty->w;
        break;
      default:
        return;
     }
   if (n > limit) n = limit;
   _text_clear(ty, cells, n, 0, EINA_TRUE);
}

static void
_clear_screen(Termpty *ty, int mode)
{
   Termcell *cells;

   cells = ty->screen;
   switch (mode)
     {
      case CLR_END:
        _clear_line(ty, mode, ty->w);
        if (ty->state.cy < (ty->h - 1))
          {
             cells = &(ty->screen[(ty->state.cy + 1) * ty->w]);
             _text_clear(ty, cells, ty->w * (ty->h - ty->state.cy - 1), 0, EINA_TRUE);
          }
        break;
      case CLR_BEGIN:
        if (ty->state.cy > 0)
          _text_clear(ty, cells, ty->w * ty->state.cy, 0, EINA_TRUE);
        _clear_line(ty, mode, ty->w);
        break;
      case CLR_ALL:
        _text_clear(ty, cells, ty->w * ty->h, 0, EINA_TRUE);
        break;
      default:
        break;
     }
   if (ty->cb.cancel_sel.func)
     ty->cb.cancel_sel.func(ty->cb.cancel_sel.data);
}

static void
_clear_all(Termpty *ty)
{
   if (!ty->screen) return;
   memset(ty->screen, 0, sizeof(*(ty->screen)) * ty->w * ty->h);
}

static void
_reset_att(Termatt *att)
{
   att->fg = COL_DEF;
   att->bg = COL_DEF;
   att->bold = 0;
   att->faint = 0;
#if defined(SUPPORT_ITALIC)
   att->italic = 0;
#elif defined(SUPPORT_DBLWIDTH)
   att->dblwidth = 0;
#endif
   att->underline = 0;
   att->blink = 0;
   att->blink2 = 0;
   att->inverse = 0;
   att->invisible = 0;
   att->strike = 0;
   att->fg256 = 0;
   att->bg256 = 0;
   att->fgintense = 0;
   att->bgintense = 0;
   att->autowrapped = 0;
   att->newline = 0;
   att->tab = 0;
}

static void
_reset_state(Termpty *ty)
{
   ty->state.cx = 0;
   ty->state.cy = 0;
   ty->state.scroll_y1 = 0;
   ty->state.scroll_y2 = 0;
   ty->state.had_cr_x = 0;
   ty->state.had_cr_y = 0;
   _reset_att(&(ty->state.att));
   ty->state.charset = 0;
   ty->state.charsetch = 'B';
   ty->state.chset[0] = 'B';
   ty->state.chset[1] = 'B';
   ty->state.chset[2] = 'B';
   ty->state.chset[3] = 'B';
   ty->state.multibyte = 0;
   ty->state.alt_kp = 0;
   ty->state.insert = 0;
   ty->state.appcursor = 0;
   ty->state.wrap = 1;
   ty->state.wrapnext = 0;
   ty->state.hidecursor = 0;
   ty->state.crlf = 0;
   ty->state.had_cr = 0;
}

static void
_cursor_copy(Termstate *state, Termstate *dest)
{
   dest->cx = state->cx;
   dest->cy = state->cy;
}

static int
_csi_arg_get(Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int octal = 0;
   int sum = 0;

   while ((*b) && (!isdigit(*b))) b++;
   if (!*b)
     {
        *ptr = NULL;
        return 0;
     }
   if (*b == '0') octal = 1;
   while (isdigit(*b))
     {
        if (octal) sum *= 8;
        else sum *= 10;
        sum += *b - '0';
        b++;
     }
   *ptr = b;
   return sum;
}

static int
_handle_esc_csi(Termpty *ty, const Eina_Unicode *c, Eina_Unicode *ce)
{
   Eina_Unicode *cc;
   int arg, first = 1, i;
   Eina_Unicode buf[4096], *b;

   cc = (Eina_Unicode *)c;
   b = buf;
   while ((cc < ce) && (*cc >= '0') && (*cc <= '?'))
     {
        *b = *cc;
        b++;
        cc++;
     }
   // if cc == ce then we got to the end of the string with no end marker
   // so return -2 to indicate to go back to the escape beginning when
   // there is more bufer available
   if (cc == ce) return -2;
   *b = 0;
   b = buf;
//   DBG(" CSI: '%c' args '%s'", *cc, buf);
   switch (*cc)
     {
      case 'm': // color set
        while (b)
          {
             arg = _csi_arg_get(&b);
             if ((first) && (!b))
               _reset_att(&(ty->state.att));
             else if (b)
               {
                  first = 0;
                  switch (arg)
                    {
                     case 0: // reset to normal
                       _reset_att(&(ty->state.att));
                       break;
                     case 1: // bold/bright
                       ty->state.att.bold = 1;
                       break;
                     case 2: // faint
                       ty->state.att.faint = 1;
                       break;
                     case 3: // italic
#if defined(SUPPORT_ITALIC)
                       ty->state.att.italic = 1;
#endif                       
                       break;
                     case 4: // underline
                       ty->state.att.underline = 1;
                       break;
                     case 5: // blink
                       ty->state.att.blink = 1;
                       break;
                     case 6: // blink rapid
                       ty->state.att.blink2 = 1;
                       break;
                     case 7: // reverse
                       ty->state.att.inverse = 1;
                       break;
                     case 8: // invisible
                       ty->state.att.invisible = 1;
                       break;
                     case 9: // strikethrough
                       ty->state.att.strike = 1;
                       break;
                     case 21: // no bold/bright
                       ty->state.att.bold = 0;
                       break;
                     case 22: // no faint
                       ty->state.att.faint = 0;
                       break;
                     case 23: // no italic
#if defined(SUPPORT_ITALIC)
                       ty->state.att.italic = 0;
#endif                       
                       break;
                     case 24: // no underline
                       ty->state.att.underline = 0;
                       break;
                     case 25: // no blink
                       ty->state.att.blink = 0;
                       ty->state.att.blink2 = 0;
                       break;
                     case 27: // no reverse
                       ty->state.att.inverse = 0;
                       break;
                     case 28: // no invisible
                       ty->state.att.invisible = 0;
                       break;
                     case 29: // no strikethrough
                       ty->state.att.strike = 0;
                       break;
                     case 30: // fg
                     case 31:
                     case 32:
                     case 33:
                     case 34:
                     case 35:
                     case 36:
                     case 37:
                       ty->state.att.fg256 = 0;
                       ty->state.att.fg = (arg - 30) + COL_BLACK;
                       ty->state.att.fgintense = 0;
                       break;
                     case 38: // xterm 256 fg color ???
                       // now check if next arg is 5
                       arg = _csi_arg_get(&b);
                       if (arg != 5) ERR("Failed xterm 256 color fg esc 5");
                       else
                         {
                            // then get next arg - should be color index 0-255
                            arg = _csi_arg_get(&b);
                            if (!b) ERR("Failed xterm 256 color fg esc val");
                            else
                              {
                                 ty->state.att.fg256 = 1;
                                 ty->state.att.fg = arg;
                              }
                         }
                       ty->state.att.fgintense = 0;
                       break;
                     case 39: // default fg color
                       ty->state.att.fg256 = 0;
                       ty->state.att.fg = COL_DEF;
                       ty->state.att.fgintense = 0;
                       break;
                     case 40: // bg
                     case 41:
                     case 42:
                     case 43:
                     case 44:
                     case 45:
                     case 46:
                     case 47:
                       ty->state.att.bg256 = 0;
                       ty->state.att.bg = (arg - 40) + COL_BLACK;
                       ty->state.att.bgintense = 0;
                       break;
                     case 48: // xterm 256 bg color ???
                       // now check if next arg is 5
                       arg = _csi_arg_get(&b);
                       if (arg != 5) ERR("Failed xterm 256 color bg esc 5");
                       else
                         {
                            // then get next arg - should be color index 0-255
                            arg = _csi_arg_get(&b);
                            if (!b) ERR("Failed xterm 256 color bg esc val");
                            else
                              {
                                 ty->state.att.bg256 = 1;
                                 ty->state.att.bg = arg;
                              }
                         }
                       ty->state.att.bgintense = 0;
                       break;
                     case 49: // default bg color
                       ty->state.att.bg256 = 0;
                       ty->state.att.bg = COL_DEF;
                       ty->state.att.bgintense = 0;
                       break;
                     case 90: // fg
                     case 91:
                     case 92:
                     case 93:
                     case 94:
                     case 95:
                     case 96:
                     case 97:
                       ty->state.att.fg256 = 0;
                       ty->state.att.fg = (arg - 90) + COL_BLACK;
                       ty->state.att.fgintense = 1;
                       break;
                     case 98: // xterm 256 fg color ???
                       // now check if next arg is 5
                       arg = _csi_arg_get(&b);
                       if (arg != 5) ERR("Failed xterm 256 color fg esc 5");
                       else
                         {
                            // then get next arg - should be color index 0-255
                            arg = _csi_arg_get(&b);
                            if (!b) ERR("Failed xterm 256 color fg esc val");
                            else
                              {
                                 ty->state.att.fg256 = 1;
                                 ty->state.att.fg = arg;
                              }
                         }
                       ty->state.att.fgintense = 1;
                       break;
                     case 99: // default fg color
                       ty->state.att.fg256 = 0;
                       ty->state.att.fg = COL_DEF;
                       ty->state.att.fgintense = 1;
                       break;
                     case 100: // bg
                     case 101:
                     case 102:
                     case 103:
                     case 104:
                     case 105:
                     case 106:
                     case 107:
                       ty->state.att.bg256 = 0;
                       ty->state.att.bg = (arg - 100) + COL_BLACK;
                       ty->state.att.bgintense = 1;
                       break;
                     case 108: // xterm 256 bg color ???
                       // now check if next arg is 5
                       arg = _csi_arg_get(&b);
                       if (arg != 5) ERR("Failed xterm 256 color bg esc 5");
                       else
                         {
                            // then get next arg - should be color index 0-255
                            arg = _csi_arg_get(&b);
                            if (!b) ERR("Failed xterm 256 color bg esc val");
                            else
                              {
                                 ty->state.att.bg256 = 1;
                                 ty->state.att.bg = arg;
                              }
                         }
                       ty->state.att.bgintense = 1;
                       break;
                     case 109: // default bg color
                       ty->state.att.bg256 = 0;
                       ty->state.att.bg = COL_DEF;
                       ty->state.att.bgintense = 1;
                       break;
                     default: //  not handled???
                       ERR("  color cmd [%i] not handled", arg);
                       break;
                    }
               }
          }
        break;
      case '@': // insert N blank chars
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
          {
             int pi = ty->state.insert;
             int blank[1] = { ' ' };
             int cx = ty->state.cx;

             ty->state.wrapnext = 0;
             ty->state.insert = 1;
             for (i = 0; i < arg; i++)
               _text_append(ty, blank, 1);
             ty->state.insert = pi;
             ty->state.cx = cx;
          }
        break;
      case 'A': // cursor up N
      case 'e': // cursor up N
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        ty->state.wrapnext = 0;
        for (i = 0; i < arg; i++)
          {
             ty->state.cy--;
             _text_scroll_rev_test(ty);
          }
        break;
      case 'B': // cursor down N
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        ty->state.wrapnext = 0;
        for (i = 0; i < arg; i++)
          {
             ty->state.cy++;
             _text_scroll_test(ty);
          }
        break;
      case 'D': // cursor left N
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        ty->state.wrapnext = 0;
        for (i = 0; i < arg; i++)
          {
             ty->state.cx--;
             if (ty->state.cx < 0) ty->state.cx = 0;
          }
        break;
      case 'C': // cursor right N
      case 'a': // cursor right N
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        ty->state.wrapnext = 0;
        for (i = 0; i < arg; i++)
          {
             ty->state.cx++;
             if (ty->state.cx >= ty->w) ty->state.cx = ty->w - 1;
          }
        break;
      case 'H': // cursor pos set
      case 'f': // cursor pos set
        ty->state.wrapnext = 0;
        if (!*b)
          {
             ty->state.cx = 0;
             ty->state.cy = 0;
          }
        else
          {
             arg = _csi_arg_get(&b);
             if (arg < 1) arg = 1;
             arg--;
             if (arg < 0) arg = 0;
             else if (arg >= ty->h) arg = ty->h - 1;
             if (b) ty->state.cy = arg;
             if (b)
               {
                  arg = _csi_arg_get(&b);
                  if (arg < 1) arg = 1;
                  arg--;
               }
             else arg = 0;
             if (arg < 0) arg = 0;
             else if (arg >= ty->w) arg = ty->w - 1;
             if (b) ty->state.cx = arg;
          }
       break;
      case 'G': // to column N
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        ty->state.wrapnext = 0;
        ty->state.cx = arg - 1;
        if (ty->state.cx < 0) ty->state.cx = 0;
        else if (ty->state.cx >= ty->w) ty->state.cx = ty->w - 1;
        break;
      case 'd': // to row N
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        ty->state.wrapnext = 0;
        ty->state.cy = arg - 1;
        if (ty->state.cy < 0) ty->state.cy = 0;
        else if (ty->state.cy >= ty->h) ty->state.cy = ty->h - 1;
        break;
      case 'E': // down relative N rows, and to col 0
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        ty->state.wrapnext = 0;
        ty->state.cy += arg;
        if (ty->state.cy < 0) ty->state.cy = 0;
        else if (ty->state.cy >= ty->h) ty->state.cy = ty->h - 1;
        ty->state.cx = 0;
        break;
      case 'F': // up relative N rows, and to col 0
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        ty->state.wrapnext = 0;
        ty->state.cy -= arg;
        if (ty->state.cy < 0) ty->state.cy = 0;
        else if (ty->state.cy >= ty->h) ty->state.cy = ty->h - 1;
        ty->state.cx = 0;
        break;
      case 'X': // erase N chars
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        _clear_line(ty, CLR_END, arg);
        break;
      case 'S': // scroll up N lines
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        for (i = 0; i < arg; i++) _text_scroll(ty);
        break;
      case 'T': // scroll down N lines
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        for (i = 0; i < arg; i++) _text_scroll_rev(ty);
        break;
      case 'M': // delete N lines - cy
      case 'L': // insert N lines - cy
        arg = _csi_arg_get(&b);
          {
             int sy1, sy2;

             sy1 = ty->state.scroll_y1;
             sy2 = ty->state.scroll_y2;
             if (ty->state.scroll_y2 == 0)
               {
                  ty->state.scroll_y1 = ty->state.cy;
                  ty->state.scroll_y2 = ty->h;
               }
             else
               {
                  ty->state.scroll_y1 = ty->state.cy;
                  if (ty->state.scroll_y2 <= ty->state.scroll_y1)
                    ty->state.scroll_y2 = ty->state.scroll_y1 + 1;
               }
             if (arg < 1) arg = 1;
             for (i = 0; i < arg; i++)
               {
                  if (*cc == 'M') _text_scroll(ty);
                  else _text_scroll_rev(ty);
               }
             ty->state.scroll_y1 = sy1;
             ty->state.scroll_y2 = sy2;
          }
        break;
      case 'P': // erase and scrollback N chars
        arg = _csi_arg_get(&b);
          {
             Termcell *cells;
             int x, lim;

             if (arg < 1) arg = 1;
             cells = &(ty->screen[ty->state.cy * ty->w]);
             lim = ty->w - arg;
             for (x = ty->state.cx; x < (ty->w); x++)
               {
                  if (x < lim)
                    cells[x] = cells[x + arg];
                  else
                    memset(&(cells[x]), 0, sizeof(*cells));
               }
          }
        break;
      case 'c': // query device id
          {
             char bf[32];
//             0  Base VT100, no options
//             1  Preprocessor option (STP)
//             2  Advanced video option (AVO)
//             3  AVO and STP
//             4  Graphics processor option (GO)
//             5  GO and STP
//             6  GO and AVO
//             7  GO, STP, and AVO
             snprintf(bf, sizeof(bf), "\033[?1;%ic", 0);
             _term_write(ty, bf, strlen(bf));
          }
        break;
      case 'J': // "2j" erases the screen, 1j erase from screen start to curs, 0j erase cursor to end of screen
        arg = _csi_arg_get(&b);
        if (b)
          {
             if ((arg >= CLR_END) && (arg <= CLR_ALL))
               _clear_screen(ty, arg);
             else
               ERR("invalid clr scr %i", arg);
          }
        else _clear_screen(ty, CLR_END);
        break;
      case 'K': // 0K erase to end of line, 1K erase from screen start to cursor, 2K erase all of line
        arg = _csi_arg_get(&b);
        if (b)
          {
             if ((arg >= CLR_END) && (arg <= CLR_ALL))
               _clear_line(ty, arg, ty->w);
             else
               ERR("invalid clr lin %i", arg);
          }
        else _clear_line(ty, CLR_END, ty->w);
        break;
      case 'h': // list - set screen mode or line wrap ("7h" == turn on line wrap, "7l" disables line wrap , ...)
      case 'l':
          {
             int mode = 0, priv = 0;
             int handled = 0;

             if (*cc == 'h') mode = 1;
             if (*b == '?')
               {
                  priv = 1;
                  b++;
               }
             if (priv)
               {
                  while (b)
                    {
                       arg = _csi_arg_get(&b);
                       if (b)
                         {
                            int size;

                            // complete-ish list here:
                            // http://ttssh2.sourceforge.jp/manual/en/about/ctrlseq.html
                            switch (arg)
                              {
                               case 1:
                                 handled = 1;
                                 ty->state.appcursor = mode;
                                 break;
                               case 2:
                                 handled = 1;
                                 ty->state.kbd_lock = mode;
                                 break;
                               case 3: // should we handle this?
                                 handled = 1;
                                 ERR("XXX: 132 column mode %i", mode);
                                 break;
                               case 4:
                                 handled = 1;
                                 ERR("XXX: set insert mode to %i", mode);
                                 break;
                               case 5:
                                 handled = 1;
                                 ty->state.reverse = mode;
                                 break;
                               case 6:
                                 handled = 1;
                                 ERR("XXX: origin mode: cursor is at 0,0/cursor limited to screen/start point for line #'s depends on top margin");
                                 break;
                               case 7:
                                 handled = 1;
                                 DBG("DDD: set wrap mode to %i", mode);
                                 ty->state.wrap = mode;
                                 break;
                               case 8:
                                 handled = 1;
                                 ty->state.no_autorepeat = !mode;
                                 INF("XXX: auto repeat %i", mode);
                                 break;
                               case 9:
                                 handled = 1;
                                 if (mode) ty->mouse_rep = MOUSE_X10;
                                 else ty->mouse_rep = MOUSE_OFF;
                                 break;
                               case 12: // ignore
                                 handled = 1;
//                                 DBG("XXX: set blinking cursor to (stop?) %i or local echo", mode);
                                 break;
                               case 19: // never seen this - what to do?
                                 handled = 1;
//                                 INF("XXX: set print extent to full screen");
                                 break;
                               case 20: // crfl==1 -> cur moves to col 0 on LF, FF or VT, ==0 -> mode is cr+lf
                                 handled = 1;
                                 ty->state.crlf = mode;
                                 break;
                               case 25:
                                 handled = 1;
                                 ty->state.hidecursor = !mode;
                                 break;
                               case 30: // ignore
                                 handled = 1;
//                                 DBG("XXX: set scrollbar mapping %i", mode);
                                 break;
                               case 33: // ignore
                                 handled = 1;
//                                 INF("XXX: Stop cursor blink %i", mode);
                                 break;
                               case 34: // ignore
                                 handled = 1;
//                                 INF("XXX: Underline cursor mode %i", mode);
                                 break;
                               case 35: // ignore
                                 handled = 1;
//                                 DBG("XXX: set shift keys %i", mode);
                                 break;
                               case 38: // ignore
                                 handled = 1;
//                                 INF("XXX: switch to tek window %i", mode);
                                 break;
                               case 59: // ignore
                                 handled = 1;
//                                 INF("XXX: kanji terminal mode %i", mode);
                                 break;
                               case 66:
                                 handled = 1;
                                 ERR("XXX: app keypad mode %i", mode);
                                 break;
                               case 67:
                                 handled = 1;
                                 ty->state.send_bs = mode;
                                 INF("XXX: backspace send bs not del = %i", mode);
                                 break;
                               case 1000:
                                 handled = 1;
                                 if (mode) ty->mouse_rep = MOUSE_NORMAL;
                                 else ty->mouse_rep = MOUSE_OFF;
                                 INF("XXX: set mouse (press+release only) to %i", mode);
                                 break;
                               case 1001:
                                 handled = 1;
                                 ERR("XXX: x11 mouse highlighting %i", mode);
                                 break;
                               case 1002:
                                 handled = 1;
                                 ERR("XXX: set mouse (press+relese+motion while pressed) %i", mode);
                                 break;
                               case 1003:
                                 handled = 1;
                                 ERR("XXX: set mouse (press+relese+all motion) %i", mode);
                                 break;
                               case 1004: // i dont know what focus repporting is?
                                 handled = 1;
                                 ERR("XXX: enable focus reporting %i", mode);
                                 break;
                               case 1005:
                                 handled = 1;
                                 if (mode) ty->mouse_rep = MOUSE_UTF8;
                                 else ty->mouse_rep = MOUSE_OFF;
                                 INF("XXX: set mouse (xterm utf8 style) %i", mode);
                                 break;
                               case 1006:
                                 handled = 1;
                                 if (mode) ty->mouse_rep = MOUSE_SGR;
                                 else ty->mouse_rep = MOUSE_OFF;
                                 INF("XXX: set mouse (xterm sgr style) %i", mode);
                                 break;
                               case 1010: // ignore
                                 handled = 1;
//                                 DBG("XXX: set home on tty output %i", mode);
                                 break;
                               case 1012: // ignore
                                 handled = 1;
//                                 DBG("XXX: set home on tty input %i", mode);
                                 break;
                               case 1015:
                                 handled = 1;
                                 if (mode) ty->mouse_rep = MOUSE_URXVT;
                                 else ty->mouse_rep = MOUSE_OFF;
                                 INF("XXX: set mouse (rxvt-unicdode style) %i", mode);
                                 break;
                               case 1034: // ignore
                                  /* libreadline6 emits it but it shouldn't.
                                     See http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=577012
                                  */
                                  handled = 1;
//                                  DBG("Ignored screen mode %i", arg);
                                  break;
                               case 1047:
                               case 1049:
                               case 47:
                                 handled = 1;
                                 DBG("DDD: switch buf");
                                 if (ty->altbuf)
                                   {
                                      // if we are looking at alt buf now,
                                      // clear main buf before we swap it back
                                      // into the sreen2 save (so save is
                                      // clear)
                                      _clear_all(ty);
//                                      _cursor_copy(&(ty->swap), &(ty->state));
                                      ty->state = ty->swap;
                                   }
                                 else
                                   {
//                                      _cursor_copy(&(ty->state), &(ty->swap));
                                      ty->swap = ty->state;
                                   }
                                 size = ty->w * ty->h;
                                 // swap screen content now
                                 for (i = 0; i < size; i++)
                                   {
                                      Termcell t;

                                      t = ty->screen[i];
                                      ty->screen[i] = ty->screen2[i];
                                      ty->screen2[i] = t;
                                   }
                                 ty->altbuf = !ty->altbuf;
                                 if (ty->cb.cancel_sel.func)
                                   ty->cb.cancel_sel.func(ty->cb.cancel_sel.data);
                                 break;
                               case 1048:
                                 if (mode)
                                   _cursor_copy(&(ty->state), &(ty->save));
                                 else
                                   _cursor_copy(&(ty->save), &(ty->state));
                                 break;
                               case 2004: // ignore
                                 handled = 1;
//                                 INF("XXX: enable bracketed paste mode %i", mode);
                                 break;
                               case 7727: // ignore
                                 handled = 1;
//                                 INF("XXX: enable application escape mode %i", mode);
                                 break;
                               case 7786: // ignore
                                 handled = 1;
//                                 INF("XXX: enable mouse wheel -> cursor key xlation %i", mode);
                                 break;
                               default:
                                 ERR("unhandled screen mode arg %i", arg);
                                 break;
                              }
                         }
                    }
               }
             else
               {
                  while (b)
                    {
                       arg = _csi_arg_get(&b);
                       if (b)
                         {
                            if (arg == 1)
                              {
                                 handled = 1;
                                 ty->state.appcursor = mode;
                              }
                            else if (arg == 4)
                              {
                                 handled = 1;
                                 DBG("DDD: set insert mode to %i", mode);
                                 ty->state.insert = mode;
                              }
//                            else if (arg == 24)
//                              {
//                                 ERR("unhandled #24 arg %i", arg);
//                                  // ???
//                              }
                            else
                              ERR("unhandled screen non-priv mode arg %i, mode %i, ch '%c'", arg, mode, *cc);
                         }
                    }
               }
             if (!handled) ERR("unhandled '%c'", *cc);
          }
        break;
      case 'r':
        arg = _csi_arg_get(&b);
        if (!b)
          {
             INF("no region args reset region");
             ty->state.scroll_y1 = 0;
             ty->state.scroll_y2 = 0;
          }
        else
          {
             int arg2;

             arg2 = _csi_arg_get(&b);
             if (!b)
               {
                  INF("failed to give 2 region args reset region");
                  ty->state.scroll_y1 = 0;
                  ty->state.scroll_y2 = 0;
               }
             else
               {
                  if (arg > arg2)
                    {
                       ERR("scroll region beginning > end [%i %i]", arg, arg2);
                       ty->state.scroll_y1 = 0;
                       ty->state.scroll_y2 = 0;
                    }
                  else
                    {
                       INF("2 region args: %i %i", arg, arg2);
                       if (arg >= ty->h) arg = ty->h - 1;
                       if (arg2 > ty->h) arg2 = ty->h;
                       arg2++;
                       ty->state.scroll_y1 = arg - 1;
                       ty->state.scroll_y2 = arg2 - 1;
                    }
               }
          }
        break;
      case 's': // store cursor pos
        _cursor_copy(&(ty->state), &(ty->save));
        break;
      case 'u': // restore cursor pos
        _cursor_copy(&(ty->save), &(ty->state));
        break;
/*
      case 'R': // report cursor
        break;
      case 'n': // "6n" queires cursor pos, 0n, 3n, 5n too
        break;
      case 's':
        break;
      case 't':
        break;
      case 'p': // define key assignments based on keycode
        break;
      case 'q': // set/clear led's
        break;
      case 'x': // request terminal parameters
        break;
      case 'r': // set top and bottom margins
        break;
      case 'y': // invoke confidence test
        break;
      case 'g': // clear tabulation
        break;
 */
      default:
        ERR("unhandled CSI '%c' (0x%02x)", *cc, *cc);
        break;
     }
   cc++;
   return cc - c;
}

static int
_handle_esc_xterm(Termpty *ty, const Eina_Unicode *c, Eina_Unicode *ce)
{
   Eina_Unicode *cc;
   Eina_Unicode buf[4096], *b;
   char *s;
   int len = 0;
   
   cc = (Eina_Unicode *)c;
   b = buf;
   while ((cc < ce) && (*cc >= ' ') && (*cc < 0x7f))
     {
        *b = *cc;
        b++;
        cc++;
     }
   *b = 0;
   if ((*cc < ' ') || (*cc >= 0x7f)) cc++;
   else return -2;
   switch (buf[0])
     {
      case '0':
        // XXX: title + name - callback
        s = eina_unicode_unicode_to_utf8(&(buf[2]), &len);
        if (ty->prop.title) eina_stringshare_del(ty->prop.title);
        if (ty->prop.icon) eina_stringshare_del(ty->prop.icon);
        if (b)
          {
             ty->prop.title = eina_stringshare_add(s);
             ty->prop.icon = eina_stringshare_add(s);
             free(s);
          }
        else
          {
             ty->prop.title = NULL;
             ty->prop.icon = NULL;
          }
        if (ty->cb.set_title.func) ty->cb.set_title.func(ty->cb.set_title.data);
        if (ty->cb.set_icon.func) ty->cb.set_title.func(ty->cb.set_icon.data);
        break;
      case '1':
        // XXX: icon name - callback
        s = eina_unicode_unicode_to_utf8(&(buf[2]), &len);
        if (ty->prop.icon) eina_stringshare_del(ty->prop.icon);
        if (s)
          {
             ty->prop.icon = eina_stringshare_add(s);
             free(s);
          }
        else
          {
             ty->prop.icon = NULL;
          }
        if (ty->cb.set_icon.func) ty->cb.set_title.func(ty->cb.set_icon.data);
        break;
      case '2':
        // XXX: title - callback
        s = eina_unicode_unicode_to_utf8(&(buf[2]), &len);
        if (ty->prop.title) eina_stringshare_del(ty->prop.title);
        if (s)
          {
             ty->prop.title = eina_stringshare_add(s);
             free(s);
          }
        else
          {
             ty->prop.title = NULL;
          }
        if (ty->cb.set_title.func) ty->cb.set_title.func(ty->cb.set_title.data);
        break;
      case '4':
        // XXX: set palette entry. not supported.
        b = &(buf[2]);
        break;
      default:
        // many others
        ERR("unhandled xterm esc '%c'", buf[0]);
        break;
     }
    return cc - c;
}

static int
_handle_esc_terminology(Termpty *ty, const Eina_Unicode *c, Eina_Unicode *ce)
{
   Eina_Unicode *cc;
   Eina_Unicode buf[4096], *b;
   char *s;
   int slen =  0;

   cc = (Eina_Unicode *)c;
   b = buf;
   while ((cc < ce) && (*cc != 0x0))
     {
        *b = *cc;
        b++;
        cc++;
     }
   *b = 0;
   if (*cc == 0x0) cc++;
   else return -2;
   // commands are stored in the buffer, 0 bytes not allowd (end marker)
   s = eina_unicode_unicode_to_utf8(buf, &slen);
   ty->cur_cmd = s;
   if (ty->cb.command.func) ty->cb.command.func(ty->cb.command.data);
   ty->cur_cmd = NULL;
   if (s) free(s);
   return cc - c;
}

static int
_handle_esc(Termpty *ty, const Eina_Unicode *c, Eina_Unicode *ce)
{
   if ((ce - c) < 2) return 0;
   DBG("ESC: '%c'", c[1]);
   switch (c[1])
     {
      case '[':
        return 2 + _handle_esc_csi(ty, c + 2, ce);
      case ']':
        return 2 + _handle_esc_xterm(ty, c + 2, ce);
      case '}':
        return 2 + _handle_esc_terminology(ty, c + 2, ce);
      case '=': // set alternate keypad mode
        ty->state.alt_kp = 1;
        return 2;
      case '>': // set numeric keypad mode
        ty->state.alt_kp = 0;
        return 2;
      case 'M': // move to prev line
        ty->state.wrapnext = 0;
        ty->state.cy--;
        _text_scroll_rev_test(ty);
        return 2;
      case 'D': // move to next line
        ty->state.wrapnext = 0;
        ty->state.cy++;
        _text_scroll_test(ty);
        return 2;
      case 'E': // add \n\r
        ty->state.wrapnext = 0;
        ty->state.cx = 0;
        ty->state.cy++;
        _text_scroll_test(ty);
        return 2;
      case 'Z': // same a 'ESC [ Pn c'
        _term_txt_write(ty, "\033[?1;2C");
        return 2;
      case 'c': // reset terminal to initial state
        DBG("reset to init mode and clear");
        _reset_state(ty);
        _clear_screen(ty, CLR_ALL);
        if (ty->cb.cancel_sel.func)
          ty->cb.cancel_sel.func(ty->cb.cancel_sel.data);
        return 2;
      case '(': // charset 0
        ty->state.chset[0] = c[2];
        ty->state.multibyte = 0;
        return 3;
      case ')': // charset 1
        ty->state.chset[1] = c[2];
        ty->state.multibyte = 0;
        return 3;
      case '*': // charset 2
        ty->state.chset[2] = c[2];
        ty->state.multibyte = 0;
        return 3;
      case '+': // charset 3
        ty->state.chset[3] = c[2];
        ty->state.multibyte = 0;
        return 3;
      case '$': // charset -2
        ty->state.chset[2] = c[2];
        ty->state.multibyte = 1;
        return 3;
      case '#': // #8 == test mode -> fill screen with "E";
        if (c[2] == '8')
          {
             int i, size;
             Termcell *cells;

             DBG("reset to init mode and clear then fill with E");
             _reset_state(ty);
             ty->save = ty->state;
             ty->swap = ty->state;
             _clear_screen(ty, CLR_ALL);
             if (ty->cb.cancel_sel.func)
               ty->cb.cancel_sel.func(ty->cb.cancel_sel.data);
             cells = ty->screen;
             size = ty->w * ty->h;
             if (cells)
               {
                  for (i = 0; i < size; i++) cells[i].codepoint = 'E';
               }
          }
        return 3;
      case '@': // just consume this plus next char
        return 3;
      case '7': // save cursor pos
        _cursor_copy(&(ty->state), &(ty->save));
        return 2;
      case '8': // restore cursor pos
        _cursor_copy(&(ty->save), &(ty->state));
        return 2;
/*
      case 'G': // query gfx mode
        return 3;
      case 'H': // set tab at current column
        return 2;
      case 'n': // single shift 2
        return 2;
      case 'o': // single shift 3
        return 2;
 */
      default:
        ERR("eek - esc unhandled '%c' (0x%02x)", c[1], c[1]);
        break;
     }
   return 1;
}

static int
_handle_seq(Termpty *ty, Eina_Unicode *c, Eina_Unicode *ce)
{
   Eina_Unicode *cc;
   int len = 0;

/*   
   printf(" B: ");
   int j;
   for (j = 0; c + j < ce && j < 100; j++)
     {
        if ((c[j] < ' ') || (c[j] >= 0x7f))
          printf("\033[35m%02x\033[0m", c[j]);
        else
          printf("%c", c[j]);
     }
   printf("\n");
 */
   if (c[0] < 0x20)
     {
        switch (c[0])
          {
/*
           case 0x00: // NUL
             return 1;
           case 0x01: // SOH (start of heading)
             return 1;
           case 0x02: // STX (start of text)
             return 1;
           case 0x03: // ETX (end of text)
             return 1;
           case 0x04: // EOT (end of transmission)
             return 1;
 */
           case 0x05: // ENQ (enquiry)
             _term_txt_write(ty, "ABC\r\n");
             ty->state.had_cr = 0;
             return 1;
/*
           case 0x06: // ACK (acknowledge)
             return 1;
 */
           case 0x07: // BEL '\a' (bell)
             if (ty->cb.bell.func) ty->cb.bell.func(ty->cb.bell.data);
             ty->state.had_cr = 0;
             return 1;
           case 0x08: // BS  '\b' (backspace)
             DBG("->BS");
             ty->state.wrapnext = 0;
             ty->state.cx--;
             if (ty->state.cx < 0) ty->state.cx = 0;
             ty->state.had_cr = 0;
             return 1;
           case 0x09: // HT  '\t' (horizontal tab)
             DBG("->HT");
             ty->screen[ty->state.cx + (ty->state.cy * ty->w)].att.tab = 1;
             ty->state.wrapnext = 0;
             ty->state.cx += 8;
             ty->state.cx = (ty->state.cx / 8) * 8;
             if (ty->state.cx >= ty->w)
               ty->state.cx = ty->w - 1;
             ty->state.had_cr = 0;
             return 1;
           case 0x0a: // LF  '\n' (new line)
           case 0x0b: // VT  '\v' (vertical tab)
           case 0x0c: // FF  '\f' (form feed)
             DBG("->LF");
             if (ty->state.had_cr)
               ty->screen[ty->state.had_cr_x + (ty->state.had_cr_y * ty->w)].att.newline = 1;
             ty->state.wrapnext = 0;
             if (ty->state.crlf) ty->state.cx = 0;
             ty->state.cy++;
             _text_scroll_test(ty);
             ty->state.had_cr = 0;
             return 1;
           case 0x0d: // CR  '\r' (carriage ret)
             DBG("->CR");
             if (ty->state.cx != 0)
               {
                  ty->state.had_cr_x = ty->state.cx;
                  ty->state.had_cr_y = ty->state.cy;
               }
             ty->state.wrapnext = 0;
             ty->state.cx = 0;
             ty->state.had_cr = 1;
             return 1;

           case 0x0e: // SO  (shift out) // Maps G1 character set into GL.
             ty->state.charset = 1;
             ty->state.charsetch = ty->state.chset[1];
             return 1;
           case 0x0f: // SI  (shift in) // Maps G0 character set into GL.
             ty->state.charset = 0;
             ty->state.charsetch = ty->state.chset[0];
             return 1;
/*
           case 0x10: // DLE (data link escape)
             return 1;
           case 0x11: // DC1 (device control 1)
             return 1;
           case 0x12: // DC2 (device control 2)
             return 1;
           case 0x13: // DC3 (device control 3)
             return 1;
           case 0x14: // DC4 (device control 4)
             return 1;
           case 0x15: // NAK (negative ack.)
             return 1;
           case 0x16: // SYN (synchronous idle)
             return 1;
           case 0x17: // ETB (end of trans. blk)
             return 1;
           case 0x18: // CAN (cancel)
             return 1;
           case 0x19: // EM  (end of medium)
             return 1;
           case 0x1a: // SUB (substitute)
             return 1;
 */
           case 0x1b: // ESC (escape)
             ty->state.had_cr = 0;
             return _handle_esc(ty, c, ce);
/*
           case 0x1c: // FS  (file separator)
             return 1;
           case 0x1d: // GS  (group separator)
             return 1;
           case 0x1e: // RS  (record separator)
             return 1;
           case 0x1f: // US  (unit separator)
             return 1;
 */
           default:
             ERR("unhandled char 0x%02x", c[0]);
             ty->state.had_cr = 0;
             return 1;
          }
     }
   else if (c[0] == 0x7f) // DEL
     {
        ERR("unhandled char 0x%02x [DEL]", c[0]);
        ty->state.had_cr = 0;
        return 1;
     }
   else if (c[0] == 0x9b) // ANSI ESC!!!
     {
        int v;
        
        printf("ANSI CSI!!!!!\n");
        ty->state.had_cr = 0;
        v = _handle_esc_csi(ty, c + 1, ce);
        if (v == -2) return 0;
        return v + 1;
     }

   cc = (int *)c;
   DBG("txt: [");
   while ((cc < ce) && (*cc >= 0x20) && (*cc != 0x7f))
     {
        DBG("%c", *cc);
        cc++;
        len++;
     }
   DBG("]");
   _text_append(ty, c, len);
   ty->state.had_cr = 0;
   return len;
}

static void
_handle_buf(Termpty *ty, const Eina_Unicode *codepoints, int len)
{
   Eina_Unicode *c, *ce, *b;
   int n, bytes;

   c = (int *)codepoints;
   ce = &(c[len]);

   if (ty->buf)
     {
        bytes = (ty->buflen + len + 1) * sizeof(int);
        b = realloc(ty->buf, bytes);
        if (!b)
          {
             ERR("memerr");
          }
        INF("realloc add %i + %i", (int)(ty->buflen * sizeof(int)), (int)(len * sizeof(int)));
        bytes = len * sizeof(Eina_Unicode);
        memcpy(&(b[ty->buflen]), codepoints, bytes);
        ty->buf = b;
        ty->buflen += len;
        ty->buf[ty->buflen] = 0;
        c = ty->buf;
        ce = c + ty->buflen;
        while (c < ce)
          {
             n = _handle_seq(ty, c, ce);
             if (n == 0)
               {
                  Eina_Unicode *tmp = ty->buf;
                  ty->buf = NULL;
                  ty->buflen = 0;
                  bytes = ((char *)ce - (char *)c) + sizeof(Eina_Unicode);
                  INF("malloc til %i", (int)(bytes - sizeof(Eina_Unicode)));
                  ty->buf = malloc(bytes);
                  if (!ty->buf)
                    {
                       ERR("memerr");
                    }
                  bytes = (char *)ce - (char *)c;
                  memcpy(ty->buf, c, bytes);
                  ty->buflen = bytes / sizeof(Eina_Unicode);
                  ty->buf[ty->buflen] = 0;
                  free(tmp);
                  break;
               }
             c += n;
          }
        if (c == ce)
          {
             if (ty->buf)
               {
                  free(ty->buf);
                  ty->buf = NULL;
               }
             ty->buflen = 0;
          }
     }
   else
     {
        while (c < ce)
          {
             n = _handle_seq(ty, c, ce);
             if (n == 0)
               {
                  bytes = ((char *)ce - (char *)c) + sizeof(Eina_Unicode);
                  ty->buf = malloc(bytes);
                  INF("malloc %i", (int)(bytes - sizeof(Eina_Unicode)));
                  if (!ty->buf)
                    {
                       ERR("memerr");
                    }
                  bytes = (char *)ce - (char *)c;
                  memcpy(ty->buf, c, bytes);
                  ty->buflen = bytes / sizeof(Eina_Unicode);
                  ty->buf[ty->buflen] = 0;
                  break;
               }
             c += n;
          }
     }
}

static void
_pty_size(Termpty *ty)
{
   struct winsize sz;

   sz.ws_col = ty->w;
   sz.ws_row = ty->h;
   sz.ws_xpixel = 0;
   sz.ws_ypixel = 0;
   if (ioctl(ty->fd, TIOCSWINSZ, &sz) < 0)
     ERR("Size set ioctl failed: %s", strerror(errno));
}

static Eina_Bool
_cb_exe_exit(void *data, int type __UNUSED__, void *event)
{
   Ecore_Exe_Event_Del *ev = event;
   Termpty *ty = data;

   if (ev->pid != ty->pid) return ECORE_CALLBACK_PASS_ON;
   ty->exit_code = ev->exit_code;
   if (ty->cb.exited.func) ty->cb.exited.func(ty->cb.exited.data);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_cb_fd_read(void *data, Ecore_Fd_Handler *fd_handler __UNUSED__)
{
   Termpty *ty = data;
   char buf[4097];
   Eina_Unicode codepoint[4097];
   int len, i, j, reads;

   // read up to 64 * 4096 bytes
   for (reads = 0; reads < 64; reads++)
     {
        len = read(ty->fd, buf, sizeof(buf) - 1);
        if (len <= 0) break;
        
/*        
        printf(" I: ");
        int jj;
        for (jj = 0; jj < len && jj < 100; jj++)
          {
             if ((buf[jj] < ' ') || (buf[jj] >= 0x7f))
               printf("\033[33m%02x\033[0m", buf[jj]);
             else
               printf("%c", buf[jj]);
          }
        printf("\n");
 */
        buf[len] = 0;
        // convert UTF8 to codepoint integers
        j = 0;
        for (i = 0; i < len;)
          {
             int g = 0;

             if (buf[i])
               {
                  i = evas_string_char_next_get(buf, i, &g);
                  if (i < 0) break;
//                  DBG("(%i) %02x '%c'", j, g, g);
               }
             else
               {
                  g = 0;
                  i++;
               }
             codepoint[j] = g;
             j++;
          }
        codepoint[j] = 0;
//        DBG("---------------- handle buf %i", j);
        _handle_buf(ty, codepoint, j);
     }
   if (ty->cb.change.func) ty->cb.change.func(ty->cb.change.data);
   return EINA_TRUE;
}

static void
_limit_coord(Termpty *ty, Termstate *state)
{
   state->wrapnext = 0;
   if (state->cx >= ty->w) state->cx = ty->w - 1;
   if (state->cy >= ty->h) state->cy = ty->h - 1;
   if (state->had_cr_x >= ty->w) state->had_cr_x = ty->w - 1;
   if (state->had_cr_y >= ty->h) state->had_cr_y = ty->h - 1;
}

Termpty *
termpty_new(const char *cmd, int w, int h, int backscroll)
{
   Termpty *ty;
   const char *pty;

   ty = calloc(1, sizeof(Termpty));
   if (!ty) return NULL;
   ty->w = w;
   ty->h = h;
   ty->backmax = backscroll;

   _reset_state(ty);
   ty->save = ty->state;
   ty->swap = ty->state;

   ty->screen = calloc(1, sizeof(Termcell) * ty->w * ty->h);
   if (!ty->screen) goto err;
   ty->screen2 = calloc(1, sizeof(Termcell) * ty->w * ty->h);
   if (!ty->screen2) goto err;

   ty->fd = posix_openpt(O_RDWR | O_NOCTTY);
   if (ty->fd < 0) goto err;
   if (grantpt(ty->fd) != 0) goto err;
   if (unlockpt(ty->fd) != 0) goto err;
   pty = ptsname(ty->fd);
   ty->slavefd = open(pty, O_RDWR | O_NOCTTY);
   if (ty->slavefd < 0) goto err;
   fcntl(ty->fd, F_SETFL, O_NDELAY);

   ty->hand_exe_exit = ecore_event_handler_add(ECORE_EXE_EVENT_DEL,
                                               _cb_exe_exit, ty);
   ty->pid = fork();
   if (!ty->pid)
     {
        const char *shell = NULL;
        const char *args[4] = {NULL, NULL, NULL, NULL};
        Eina_Bool needs_shell;
        int i;

        needs_shell = ((!cmd) ||
                       (strpbrk(cmd, " |&;<>()$`\\\"'*?#") != NULL));
        DBG("cmd='%s' needs_shell=%u", cmd ? cmd : "", needs_shell);

        if (needs_shell)
          {
             shell = getenv("SHELL");
             if (!shell)
               {
                  uid_t uid = getuid();
                  struct passwd *pw = getpwuid(uid);
                  if (pw) shell = pw->pw_shell;
               }
             if (!shell)
               {
                  WRN("Could not find shell, fallback to /bin/sh");
                  shell = "/bin/sh";
               }
          }

        if (!needs_shell)
          args[0] = cmd;
        else
          {
             args[0] = shell;
             if (cmd)
               {
                  args[1] = "-c";
                  args[2] = cmd;
               }
          }

#define NC(x) (args[x] != NULL ? args[x] : "")
        DBG("exec %s %s %s %s", NC(0), NC(1), NC(2), NC(3));
#undef NC

        for (i = 0; i < 100; i++)
          {
             if (i != ty->slavefd) close(i);
          }
        ty->fd = ty->slavefd;
        setsid();

        dup2(ty->fd, 0);
        dup2(ty->fd, 1);
        dup2(ty->fd, 2);

        if (ioctl(ty->fd, TIOCSCTTY, NULL) < 0) exit(1);

        /* TODO: should we reset signals here? */

        // pretend to be xterm
        putenv("TERM=xterm");
        execvp(args[0], (char *const *)args);
        exit(127); /* same as system() for failed commands */
     }
   ty->hand_fd = ecore_main_fd_handler_add(ty->fd, ECORE_FD_READ,
                                           _cb_fd_read, ty,
                                           NULL, NULL);
   close(ty->slavefd);
   _pty_size(ty);
   return ty;
err:
   if (ty->screen) free(ty->screen);
   if (ty->screen2) free(ty->screen2);
   if (ty->fd >= 0) close(ty->fd);
   if (ty->slavefd >= 0) close(ty->slavefd);
   free(ty);
   return NULL;
}

void
termpty_free(Termpty *ty)
{
   if (ty->hand_exe_exit) ecore_event_handler_del(ty->hand_exe_exit);
   if (ty->hand_fd) ecore_main_fd_handler_del(ty->hand_fd);
   if (ty->prop.title) eina_stringshare_del(ty->prop.title);
   if (ty->prop.icon) eina_stringshare_del(ty->prop.icon);
   if (ty->fd >= 0) close(ty->fd);
   if (ty->slavefd >= 0) close(ty->slavefd);
   if (ty->back)
     {
        int i;

        for (i = 0; i < ty->backmax; i++)
          {
             if (ty->back[i]) free(ty->back[i]);
          }
        free(ty->back);
     }
   if (ty->screen) free(ty->screen);
   if (ty->screen2) free(ty->screen2);
   if (ty->buf) free(ty->buf);
   memset(ty, 0, sizeof(Termpty));
   free(ty);
}

Termcell *
termpty_cellrow_get(Termpty *ty, int y, int *wret)
{
   Termsave *ts;

   if (y >= 0)
     {
        if (y >= ty->h) return NULL;
        *wret = ty->w;
        return &(ty->screen[y * ty->w]);
     }
   if (y < -ty->backmax) return NULL;
   ts = ty->back[(ty->backmax + ty->backpos + y) % ty->backmax];
   if (!ts) return NULL;
   *wret = ts->w;
   return ts->cell;
}

void
termpty_write(Termpty *ty, const char *input, int len)
{
   if (write(ty->fd, input, len) < 0) ERR("write %s", strerror(errno));
}

void
termpty_resize(Termpty *ty, int w, int h)
{
   Termcell *olds, *olds2;
   int y, ww, hh, oldw, oldh;

   if ((ty->w == w) && (ty->h == h)) return;

   olds = ty->screen;
   olds2 = ty->screen2;
   oldw = ty->w;
   oldh = ty->h;

   ty->w = w;
   ty->h = h;
   ty->state.had_cr = 0;
   _limit_coord(ty, &(ty->state));
   _limit_coord(ty, &(ty->swap));
   _limit_coord(ty, &(ty->save));

   ty->screen = calloc(1, sizeof(Termcell) * ty->w * ty->h);
   if (!ty->screen)
     {
        ty->screen2 = NULL;
        ERR("memerr");
     }
   ty->screen2 = calloc(1, sizeof(Termcell) * ty->w * ty->h);
   if (!ty->screen2)
     {
        ERR("memerr");
     }

   ww = ty->w;
   hh = ty->h;
   if (ww > oldw) ww = oldw;
   if (hh > oldh) hh = oldh;

   for (y = 0; y < hh; y++)
     {
        Termcell *c1, *c2;

        c1 = &(olds[y * oldw]);
        c2 = &(ty->screen[y * ty->w]);
        _text_copy(ty, c1, c2, ww);

        c1 = &(olds2[y * oldw]);
        c2 = &(ty->screen2[y * ty->w]);
        _text_copy(ty, c1, c2, ww);
     }

   free(olds);
   free(olds2);

   _pty_size(ty);
}

void
termpty_backscroll_set(Termpty *ty, int size)
{
   int i;

   if (ty->backmax == size) return;

   if (ty->back)
     {
        for (i = 0; i < ty->backmax; i++)
          {
             if (ty->back[i]) free(ty->back[i]);
          }
        free(ty->back);
     }
   if (size > 0)
     ty->back = calloc(1, sizeof(Termsave *) * size);
   else
     ty->back = NULL;
   ty->backscroll_num = 0;
   ty->backpos = 0;
   ty->backmax = size;
}
