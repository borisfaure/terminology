#include "private.h"
#include <Elementary.h>
#include "termpty.h"
#include "termptydbl.h"
#include "termptyops.h"
#include "termptygfx.h"

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
_text_save_top(Termpty *ty)
{
   Termsave *ts;

   if (ty->backmax <= 0) return;
   ts = malloc(sizeof(Termsave) + ((ty->w - 1) * sizeof(Termcell)));
   ts->w = ty->w;
   _termpty_text_copy(ty, ty->screen, ts->cell, ty->w);
   if (!ty->back) ty->back = calloc(1, sizeof(Termsave *) * ty->backmax);
   if (ty->back[ty->backpos]) free(ty->back[ty->backpos]);
   ty->back[ty->backpos] = ts;
   ty->backpos++;
   if (ty->backpos >= ty->backmax) ty->backpos = 0;
   ty->backscroll_num++;
   if (ty->backscroll_num >= ty->backmax) ty->backscroll_num = ty->backmax - 1;
}

void
_termpty_text_copy(Termpty *ty __UNUSED__, Termcell *cells, Termcell *dest, int count)
{
   memcpy(dest, cells, sizeof(*(cells)) * count);
}

void
_termpty_text_scroll(Termpty *ty)
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
        _termpty_text_copy(ty, cells2, cells, ty->w);
     }
   _text_clear(ty, cells2, ty->w, ' ', EINA_TRUE);
}

void
_termpty_text_scroll_rev(Termpty *ty)
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
        _termpty_text_copy(ty, cells, cells2, ty->w);
     }
   _text_clear(ty, cells, ty->w, ' ', EINA_TRUE);
}

void
_termpty_text_scroll_test(Termpty *ty)
{
   int e = ty->h;

   if (ty->state.scroll_y2 != 0) e = ty->state.scroll_y2;
   if (ty->state.cy >= e)
     {
        _termpty_text_scroll(ty);
        ty->state.cy = e - 1;
     }
}

void
_termpty_text_scroll_rev_test(Termpty *ty)
{
   int b = 0;

   if (ty->state.scroll_y2 != 0) b = ty->state.scroll_y1;
   if (ty->state.cy < b)
     {
        _termpty_text_scroll_rev(ty);
        ty->state.cy = b;
     }
}

void
_termpty_text_append(Termpty *ty, const Eina_Unicode *codepoints, int len)
{
   Termcell *cells;
   int i, j;

   cells = &(ty->screen[ty->state.cy * ty->w]);
   for (i = 0; i < len; i++)
     {
        Eina_Unicode g;

        if (ty->state.wrapnext)
          {
             cells[ty->state.cx].att.autowrapped = 1;
             ty->state.wrapnext = 0;
             ty->state.cx = 0;
             ty->state.cy++;
             _termpty_text_scroll_test(ty);
             cells = &(ty->screen[ty->state.cy * ty->w]);
          }
        if (ty->state.insert)
          {
             for (j = ty->w - 1; j > ty->state.cx; j--)
               cells[j] = cells[j - 1];
          }

        g = _termpty_charset_trans(codepoints[i], ty->state.charsetch);
        
        cells[ty->state.cx].codepoint = g;
        cells[ty->state.cx].att = ty->state.att;
#if defined(SUPPORT_DBLWIDTH)
        cells[ty->state.cx].att.dblwidth = _termpty_is_dblwidth_get(ty, g);
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
             if (ty->state.cx >= (ty->w - 1)) return;
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

void
_termpty_clear_line(Termpty *ty, Termpty_Clear mode, int limit)
{
   Termcell *cells;
   int n = 0;

   cells = &(ty->screen[ty->state.cy * ty->w]);
   switch (mode)
     {
      case TERMPTY_CLR_END:
        n = ty->w - ty->state.cx;
        cells = &(cells[ty->state.cx]);
        break;
      case TERMPTY_CLR_BEGIN:
        n = ty->state.cx + 1;
        break;
      case TERMPTY_CLR_ALL:
        n = ty->w;
        break;
      default:
        return;
     }
   if (n > limit) n = limit;
   _text_clear(ty, cells, n, 0, EINA_TRUE);
}

void
_termpty_clear_screen(Termpty *ty, Termpty_Clear mode)
{
   Termcell *cells;

   cells = ty->screen;
   switch (mode)
     {
      case TERMPTY_CLR_END:
        _termpty_clear_line(ty, mode, ty->w);
        if (ty->state.cy < (ty->h - 1))
          {
             cells = &(ty->screen[(ty->state.cy + 1) * ty->w]);
             _text_clear(ty, cells, ty->w * (ty->h - ty->state.cy - 1), 0, EINA_TRUE);
          }
        break;
      case TERMPTY_CLR_BEGIN:
        if (ty->state.cy > 0)
          _text_clear(ty, cells, ty->w * ty->state.cy, 0, EINA_TRUE);
        _termpty_clear_line(ty, mode, ty->w);
        break;
      case TERMPTY_CLR_ALL:
        _text_clear(ty, cells, ty->w * ty->h, 0, EINA_TRUE);
        ty->state.scroll_y2 = 0;
        break;
      default:
        break;
     }
   if (ty->cb.cancel_sel.func)
     ty->cb.cancel_sel.func(ty->cb.cancel_sel.data);
}

void
_termpty_clear_all(Termpty *ty)
{
   if (!ty->screen) return;
   memset(ty->screen, 0, sizeof(*(ty->screen)) * ty->w * ty->h);
}

void
_termpty_reset_att(Termatt *att)
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

void
_termpty_reset_state(Termpty *ty)
{
   ty->state.cx = 0;
   ty->state.cy = 0;
   ty->state.scroll_y1 = 0;
   ty->state.scroll_y2 = 0;
   ty->state.had_cr_x = 0;
   ty->state.had_cr_y = 0;
   _termpty_reset_att(&(ty->state.att));
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

void
_termpty_cursor_copy(Termstate *state, Termstate *dest)
{
   dest->cx = state->cx;
   dest->cy = state->cy;
}
