#include "private.h"
#include <Elementary.h>
#include "termio.h"
#include "termpty.h"
#include "termptydbl.h"
#include "termptyops.h"
#include "termptygfx.h"
#include "termptysave.h"
#include "miniview.h"
#include <assert.h>

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
termpty_cells_clear(Termpty *ty, Termcell *cells, int count)
{
   Termcell src;

   memset(&src, 0, sizeof(src));
   src.codepoint = 0;
   src.att = ty->termstate.att;
   termpty_cell_fill(ty, &src, cells, count);
}

void
termpty_text_scroll(Termpty *ty, Eina_Bool clear)
{
   Termcell *cells = NULL, *cells2;
   int y, start_y = 0, end_y = ty->h - 1;

   if (ty->termstate.scroll_y2 != 0)
     {
        start_y = ty->termstate.scroll_y1;
        end_y = ty->termstate.scroll_y2 - 1;
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
          termpty_cells_clear(ty, cells, ty->w);

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
          termpty_cells_clear(ty, cells, ty->w);
     }
}

void
termpty_text_scroll_rev(Termpty *ty, Eina_Bool clear)
{
   Termcell *cells, *cells2 = NULL;
   int y, start_y = 0, end_y = ty->h - 1;

   if (ty->termstate.scroll_y2 != 0)
     {
        start_y = ty->termstate.scroll_y1;
        end_y = ty->termstate.scroll_y2 - 1;
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
          termpty_cells_clear(ty, cells, ty->w);
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
          termpty_cells_clear(ty, cells, ty->w);
     }
}

void
termpty_text_scroll_test(Termpty *ty, Eina_Bool clear)
{
   int e = ty->h;

   if (ty->termstate.scroll_y2 != 0) e = ty->termstate.scroll_y2;
   if (ty->cursor_state.cy >= e)
     {
        termpty_text_scroll(ty, clear);
        ty->cursor_state.cy = e - 1;
        TERMPTY_RESTRICT_FIELD(ty->cursor_state.cy, 0, ty->h);
     }
}

void
termpty_text_scroll_rev_test(Termpty *ty, Eina_Bool clear)
{
   int b = 0;

   if (ty->termstate.scroll_y1 != 0) b = ty->termstate.scroll_y1;
   if (ty->cursor_state.cy < b)
     {
        termpty_text_scroll_rev(ty, clear);
        ty->cursor_state.cy = b;
        TERMPTY_RESTRICT_FIELD(ty->cursor_state.cy, 0, ty->h);
     }
}

void
termpty_text_append(Termpty *ty, const Eina_Unicode *codepoints, int len)
{
   Termcell *cells;
   int i, j;

   termio_content_change(ty->obj, ty->cursor_state.cx, ty->cursor_state.cy, len);

   cells = &(TERMPTY_SCREEN(ty, 0, ty->cursor_state.cy));
   for (i = 0; i < len; i++)
     {
        Eina_Unicode g;

        if (ty->termstate.wrapnext)
          {
             cells[ty->w - 1].att.autowrapped = 1;
             ty->termstate.wrapnext = 0;
             ty->cursor_state.cx = 0;
             ty->cursor_state.cy++;
             termpty_text_scroll_test(ty, EINA_TRUE);
             cells = &(TERMPTY_SCREEN(ty, 0, ty->cursor_state.cy));
          }
        if (ty->termstate.insert)
          {
             for (j = ty->w - 1; j > ty->cursor_state.cx; j--)
               termpty_cell_copy(ty, &(cells[j - 1]), &(cells[j]), 1);
          }

        g = _termpty_charset_trans(codepoints[i], ty);
        /* Skip 0-width space */
        if (EINA_UNLIKELY(g == 0x200b))
          {
             continue;
          }
        if (EINA_UNLIKELY(g >= 0x300 && g <=0x36f))
          {
             /* combining chars */
             if (EINA_UNLIKELY(g == 0x336))
               {
                  ty->termstate.combining_strike = 1;
               }
             continue;
          }

        termpty_cell_codepoint_att_fill(ty, g, ty->termstate.att,
                                        &(cells[ty->cursor_state.cx]), 1);
        if (EINA_UNLIKELY(ty->termstate.combining_strike))
          {
             ty->termstate.combining_strike = 0;
             cells[ty->cursor_state.cx].att.strike = 1;
          }
        cells[ty->cursor_state.cx].att.dblwidth = _termpty_is_dblwidth_get(ty, g);
        if (EINA_UNLIKELY((cells[ty->cursor_state.cx].att.dblwidth) && (ty->cursor_state.cx < (ty->w - 1))))
          {
             TERMPTY_FMTCLR(cells[ty->cursor_state.cx].att);
             termpty_cell_codepoint_att_fill(ty, 0, cells[ty->cursor_state.cx].att,
                                             &(cells[ty->cursor_state.cx + 1]), 1);
          }
        if (ty->termstate.wrap)
          {
             unsigned char offset = 1;

             ty->termstate.wrapnext = 0;
             if (EINA_UNLIKELY(cells[ty->cursor_state.cx].att.dblwidth))
               offset = 2;
             if (EINA_UNLIKELY(ty->cursor_state.cx >= (ty->w - offset)))
               ty->termstate.wrapnext = 1;
             else
               {
                  ty->cursor_state.cx += offset;
                  TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
               }
          }
        else
          {
             unsigned char offset = 1;

             ty->termstate.wrapnext = 0;
             if (EINA_UNLIKELY(cells[ty->cursor_state.cx].att.dblwidth))
               offset = 2;
             ty->cursor_state.cx += offset;
             if (ty->cursor_state.cx > (ty->w - offset))
               {
                  ty->cursor_state.cx = ty->w - offset;
                  TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
                  return;
               }
             TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
          }
     }
}

void
termpty_clear_line(Termpty *ty, Termpty_Clear mode, int limit)
{
   Termcell *cells;
   int n = 0;
   Evas_Coord x = 0, y = ty->cursor_state.cy;

   assert (y >= 0 && y < ty->h);

   switch (mode)
     {
      case TERMPTY_CLR_END:
        n = ty->w - ty->cursor_state.cx;
        x = ty->cursor_state.cx;
        break;
      case TERMPTY_CLR_BEGIN:
        n = ty->cursor_state.cx + 1;
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
   termpty_cells_clear(ty, cells, n);
}

void
termpty_clear_screen(Termpty *ty, Termpty_Clear mode)
{
   Termcell *cells;

   switch (mode)
     {
      case TERMPTY_CLR_END:
        termpty_clear_line(ty, mode, ty->w);
        if (ty->cursor_state.cy < (ty->h - 1))
          {
             int l = ty->h - (ty->cursor_state.cy + 1);

             termio_content_change(ty->obj, 0, ty->cursor_state.cy, l * ty->w);

             while (l)
               {
                  cells = &(TERMPTY_SCREEN(ty, 0, (ty->cursor_state.cy + l)));
                  termpty_cells_clear(ty, cells, ty->w);
                  l--;
               }
          }
        break;
      case TERMPTY_CLR_BEGIN:
        if (ty->cursor_state.cy > 0)
          {
             // First clear from circular > height, then from 0 to circular
             int y = ty->cursor_state.cy + ty->circular_offset;

             termio_content_change(ty->obj, 0, 0, ty->cursor_state.cy * ty->w);

             cells = &(TERMPTY_SCREEN(ty, 0, 0));

             if (y < ty->h)
               {
                  termpty_cells_clear(ty, cells, ty->w * ty->cursor_state.cy);
               }
             else
               {
                  int yt = y % ty->h;
                  int yb = ty->h - ty->circular_offset;

                  termpty_cells_clear(ty, cells, ty->w * yb);
                  termpty_cells_clear(ty, ty->screen, ty->w * yt);
               }
          }
        termpty_clear_line(ty, mode, ty->w);
        break;
      case TERMPTY_CLR_ALL:
        ty->circular_offset = 0;
        termpty_cells_clear(ty, ty->screen, ty->w * ty->h);
        ty->termstate.scroll_y2 = 0;
        if (ty->cb.cancel_sel.func)
          ty->cb.cancel_sel.func(ty->cb.cancel_sel.data);
        break;
      default:
        break;
     }
}

void
termpty_clear_all(Termpty *ty)
{
   if (!ty->screen) return;
   termpty_cell_fill(ty, NULL, ty->screen, ty->w * ty->h);
}

void
termpty_reset_att(Termatt *att)
{
   att->fg = COL_DEF;
   att->bg = COL_DEF;
   att->bold = 0;
   att->faint = 0;
   att->italic = 0;
   att->dblwidth = 0;
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
termpty_reset_state(Termpty *ty)
{
   int backsize;

   ty->cursor_state.cx = 0;
   ty->cursor_state.cy = 0;
   ty->termstate.scroll_y1 = 0;
   ty->termstate.scroll_y2 = 0;
   ty->termstate.had_cr_x = 0;
   ty->termstate.had_cr_y = 0;
   termpty_reset_att(&(ty->termstate.att));
   ty->termstate.charset = 0;
   ty->termstate.charsetch = 'B';
   ty->termstate.chset[0] = 'B';
   ty->termstate.chset[1] = 'B';
   ty->termstate.chset[2] = 'B';
   ty->termstate.chset[3] = 'B';
   ty->termstate.multibyte = 0;
   ty->termstate.alt_kp = 0;
   ty->termstate.insert = 0;
   ty->termstate.appcursor = 0;
   ty->termstate.wrap = 1;
   ty->termstate.wrapnext = 0;
   ty->termstate.crlf = 0;
   ty->termstate.had_cr = 0;
   ty->termstate.send_bs = 0;
   ty->termstate.reverse = 0;
   ty->termstate.no_autorepeat = 0;
   ty->termstate.cjk_ambiguous_wide = 0;
   ty->termstate.hide_cursor = 0;
   ty->mouse_mode = MOUSE_OFF;
   ty->mouse_ext = MOUSE_EXT_NONE;
   ty->bracketed_paste = 0;

   ty->backlog_beacon.screen_y = 0;
   ty->backlog_beacon.backlog_y = 0;

   termpty_backlog_lock();
   if (ty->back)
     {
        size_t i;
        for (i = 0; i < ty->backsize; i++)
          termpty_save_free(&ty->back[i]);
        free(ty->back);
        ty->back = NULL;
     }
   ty->backpos = 0;
   backsize = ty->backsize;
   ty->backsize = 0;
   termpty_backlog_size_set(ty, backsize);
   termpty_backlog_unlock();
}

void
termpty_cursor_copy(Termpty *ty, Eina_Bool save)
{
   if (save)
     {
        ty->cursor_save[ty->altbuf].cx = ty->cursor_state.cx;
        ty->cursor_save[ty->altbuf].cy = ty->cursor_state.cy;
     }
   else
     {
        ty->cursor_state.cx = ty->cursor_save[ty->altbuf].cx;
        ty->cursor_state.cy = ty->cursor_save[ty->altbuf].cy;
     }
}
