#include "private.h"
#include <Elementary.h>
#include "termio.h"
#include "termpty.h"
#include "termptydbl.h"
#include "termptyops.h"
#include "termptygfx.h"
#include "termptysave.h"
#include "miniview.h"

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
   Termcell src;

   memset(&src, 0, sizeof(src));
   src.codepoint = val;
   if (inherit_att) src.att = ty->state.att;
   termpty_cell_fill(ty, &src, cells, count);
}

void
termpty_text_save_top(Termpty *ty, Termcell *cells, ssize_t w_max)
{
   Termsave *ts;
   ssize_t w;

   if (ty->backmax <= 0) return;

   termpty_save_freeze();
   w = termpty_line_length(cells, w_max);
   ts = termpty_save_new(w);
   termpty_cell_copy(ty, cells, ts->cell, w);
   if (!ty->back) ty->back = calloc(1, sizeof(Termsave *) * ty->backmax);
   if (ty->back[ty->backpos])
     {
        termpty_save_free(ty->back[ty->backpos]);
        ty->back[ty->backpos] = NULL;
     }
   ty->back[ty->backpos] = ts;
   ty->backpos++;
   if (ty->backpos >= ty->backmax) ty->backpos = 0;
   ty->backscroll_num++;
   if (ty->backscroll_num >= ty->backmax) ty->backscroll_num = ty->backmax;
   termpty_save_thaw();
}

void
_termpty_text_scroll(Termpty *ty, Eina_Bool clear)
{
   Termcell *cells = NULL, *cells2;
   int y, start_y = 0, end_y = ty->h - 1;

   if (ty->state.scroll_y2 != 0)
     {
        start_y = ty->state.scroll_y1;
        end_y = ty->state.scroll_y2 - 1;
     }
   else
     if (!ty->altbuf)
       termpty_text_save_top(ty, &(TERMPTY_SCREEN(ty, 0, 0)), ty->w);

   termio_scroll(ty->obj, -1, start_y, end_y);
   DBG("... scroll!!!!! [%i->%i]", start_y, end_y);

   if (start_y == 0 && end_y == ty->h - 1)
     {
       // screen is a circular buffer now
       cells = &(ty->screen[ty->circular_offset * ty->w]);
       if (clear)
          _text_clear(ty, cells, ty->w, 0, EINA_TRUE);

       ty->circular_offset++;
       if (ty->circular_offset >= ty->h)
         ty->circular_offset = 0;
     }
   else
     {
       cells = &(TERMPTY_SCREEN(ty, 0, end_y));
       for (y = start_y; y < end_y; y++)
         {
            cells = &(TERMPTY_SCREEN(ty, 0, (y + 1)));
            cells2 = &(TERMPTY_SCREEN(ty, 0, y));
            termpty_cell_copy(ty, cells, cells2, ty->w);
         }
       if (clear)
          _text_clear(ty, cells, ty->w, 0, EINA_TRUE);
     }
}

void
_termpty_text_scroll_rev(Termpty *ty, Eina_Bool clear)
{
   Termcell *cells, *cells2 = NULL;
   int y, start_y = 0, end_y = ty->h - 1;

   if (ty->state.scroll_y2 != 0)
     {
        start_y = ty->state.scroll_y1;
        end_y = ty->state.scroll_y2 - 1;
     }
   DBG("... scroll rev!!!!! [%i->%i]", start_y, end_y);
   termio_scroll(ty->obj, 1, start_y, end_y);

   if (start_y == 0 && end_y == ty->h - 1)
     {
       // screen is a circular buffer now
       ty->circular_offset--;
       if (ty->circular_offset < 0)
         ty->circular_offset = ty->h - 1;

       cells = &(ty->screen[ty->circular_offset * ty->w]);
       if (clear)
          _text_clear(ty, cells, ty->w, 0, EINA_TRUE);
     }
   else
     {
       cells = &(TERMPTY_SCREEN(ty, 0, end_y));
       for (y = end_y; y > start_y; y--)
         {
            cells = &(TERMPTY_SCREEN(ty, 0, (y - 1)));
            cells2 = &(TERMPTY_SCREEN(ty, 0, y));
            termpty_cell_copy(ty, cells, cells2, ty->w);
         }
       if (clear)
          _text_clear(ty, cells, ty->w, 0, EINA_TRUE);
     }
}

void
_termpty_text_scroll_test(Termpty *ty, Eina_Bool clear)
{
   int e = ty->h;

   if (ty->state.scroll_y2 != 0) e = ty->state.scroll_y2;
   if (ty->state.cy >= e)
     {
        _termpty_text_scroll(ty, clear);
        ty->state.cy = e - 1;
     }
}

void
_termpty_text_scroll_rev_test(Termpty *ty, Eina_Bool clear)
{
   int b = 0;

   if (ty->state.scroll_y1 != 0) b = ty->state.scroll_y1;
   if (ty->state.cy < b)
     {
        _termpty_text_scroll_rev(ty, clear);
        ty->state.cy = b;
     }
}

void
_termpty_text_append(Termpty *ty, const Eina_Unicode *codepoints, int len)
{
   Termcell *cells;
   int i, j;

   termio_content_change(ty->obj, ty->state.cx, ty->state.cy, len);

   cells = &(TERMPTY_SCREEN(ty, 0, ty->state.cy));
   for (i = 0; i < len; i++)
     {
        Eina_Unicode g;

        if (ty->state.wrapnext)
          {
             cells[ty->w - 1].att.autowrapped = 1;
             ty->state.wrapnext = 0;
             ty->state.cx = 0;
             ty->state.cy++;
             _termpty_text_scroll_test(ty, EINA_TRUE);
             cells = &(TERMPTY_SCREEN(ty, 0, ty->state.cy));
          }
        if (ty->state.insert)
          {
             for (j = ty->w - 1; j > ty->state.cx; j--)
               termpty_cell_copy(ty, &(cells[j - 1]), &(cells[j]), 1);
          }

        g = _termpty_charset_trans(codepoints[i], &ty->state);

        termpty_cell_codepoint_att_fill(ty, g, ty->state.att,
                                        &(cells[ty->state.cx]), 1);
#if defined(SUPPORT_DBLWIDTH)
        cells[ty->state.cx].att.dblwidth = _termpty_is_dblwidth_get(ty, g);
        if (EINA_UNLIKELY((cells[ty->state.cx].att.dblwidth) && (ty->state.cx < (ty->w - 1))))
          {
             TERMPTY_FMTCLR(cells[ty->state.cx].att);
             termpty_cell_codepoint_att_fill(ty, 0, cells[ty->state.cx].att,
                                             &(cells[ty->state.cx + 1]), 1);
          }
#endif        
        if (ty->state.wrap)
          {
             unsigned char offset = 1;

             ty->state.wrapnext = 0;
#if defined(SUPPORT_DBLWIDTH)
	     if (EINA_UNLIKELY(cells[ty->state.cx].att.dblwidth))
               offset = 2;
#endif
             if (EINA_UNLIKELY(ty->state.cx >= (ty->w - offset))) ty->state.wrapnext = 1;
             else ty->state.cx += offset;
          }
        else
          {
             unsigned char offset = 1;

             ty->state.wrapnext = 0;
#if defined(SUPPORT_DBLWIDTH)
             if (EINA_UNLIKELY(cells[ty->state.cx].att.dblwidth))
               offset = 2;
#endif
             ty->state.cx += offset;
             if (ty->state.cx > (ty->w - offset))
               {
                  ty->state.cx = ty->w - offset;
                  return;
               }
          }
     }
}

void
_termpty_clear_line(Termpty *ty, Termpty_Clear mode, int limit)
{
   Termcell *cells;
   int n = 0;
   Evas_Coord x = 0, y = ty->state.cy;

   switch (mode)
     {
      case TERMPTY_CLR_END:
        n = ty->w - ty->state.cx;
        x = ty->state.cx;
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
   cells = &(TERMPTY_SCREEN(ty, x, y));
   if (n > limit) n = limit;
   termio_content_change(ty->obj, x, y, n);
   _text_clear(ty, cells, n, 0, EINA_TRUE);
}

void
_termpty_clear_screen(Termpty *ty, Termpty_Clear mode)
{
   Termcell *cells;

   switch (mode)
     {
      case TERMPTY_CLR_END:
        _termpty_clear_line(ty, mode, ty->w);
        if (ty->state.cy < (ty->h - 1))
          {
             int l = ty->h - (ty->state.cy + 1);

             termio_content_change(ty->obj, 0, ty->state.cy, l * ty->w);

             while (l)
               {
                  cells = &(TERMPTY_SCREEN(ty, 0, (ty->state.cy + l)));
                  _text_clear(ty, cells, ty->w, 0, EINA_TRUE);
                  l--;
               }
          }
        break;
      case TERMPTY_CLR_BEGIN:
        if (ty->state.cy > 0)
          {
             // First clear from circular > height, then from 0 to circular
             int y = ty->state.cy + ty->circular_offset;

             termio_content_change(ty->obj, 0, 0, ty->state.cy * ty->w);

             cells = &(TERMPTY_SCREEN(ty, 0, 0));

             if (y < ty->h)
               {
                  _text_clear(ty, cells, ty->w * ty->state.cy, 0, EINA_TRUE);
               }
             else
               {
                  int yt = y % ty->w;
                  int yb = ty->h - ty->circular_offset;

                  _text_clear(ty, cells, ty->w * yb, 0, EINA_TRUE);
                  _text_clear(ty, ty->screen, ty->w * yt, 0, EINA_TRUE);
               }
          }
        _termpty_clear_line(ty, mode, ty->w);
        break;
      case TERMPTY_CLR_ALL:
        ty->circular_offset = 0;
        _text_clear(ty, ty->screen, ty->w * ty->h, 0, EINA_TRUE);
        ty->state.scroll_y2 = 0;
        if (ty->cb.cancel_sel.func)
          ty->cb.cancel_sel.func(ty->cb.cancel_sel.data);
        break;
      default:
        break;
     }
}

void
_termpty_clear_all(Termpty *ty)
{
   if (!ty->screen) return;
   termpty_cell_fill(ty, NULL, ty->screen, ty->w * ty->h);
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
   att->fraktur = 0;
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
   ty->mouse_mode = MOUSE_OFF;
   ty->mouse_ext = MOUSE_EXT_NONE;
   ty->bracketed_paste = 0;
}

void
_termpty_cursor_copy(Termstate *state, Termstate *dest)
{
   dest->cx = state->cx;
   dest->cy = state->cy;
   dest->wrapnext = state->wrapnext;
}
