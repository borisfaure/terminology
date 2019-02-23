#include "private.h"

#include <Elementary.h>

#include "termio.h"
#include "miniview.h"
#include "termpty.h"
#include "termptydbl.h"
#include "termptyops.h"
#include "termiointernals.h"
#include "utf8.h"
#include "tytest.h"

/* {{{ Selection */

void
termio_selection_get(Termio *sd,
                     int c1x, int c1y, int c2x, int c2y,
                     struct ty_sb *sb,
                     Eina_Bool rtrim)
{
   int x, y;

   termpty_backlog_lock();
   for (y = c1y; y <= c2y; y++)
     {
        Termcell *cells;
        ssize_t w;
        int last0, v, start_x, end_x;

        w = 0;
        last0 = -1;
        cells = termpty_cellrow_get(sd->pty, y, &w);
        if (!cells || !w)
          {
             if (ty_sb_add(sb, "\n", 1) < 0) goto err;
             continue;
          }
        if (w > sd->grid.w) w = sd->grid.w;
        if (y == c1y && c1x >= w)
          {
             if (rtrim)
               ty_sb_spaces_rtrim(sb);
             if (ty_sb_add(sb, "\n", 1) < 0) goto err;
             continue;
          }
        start_x = c1x;
        end_x = (c2x >= w) ? w - 1 : c2x;
        if (c1y != c2y)
          {
             if (y == c1y) end_x = w - 1;
             else if (y == c2y) start_x = 0;
             else
               {
                  start_x = 0;
                  end_x = w - 1;
               }
          }
        for (x = start_x; x <= end_x; x++)
          {
             if ((cells[x].codepoint == 0) && (cells[x].att.dblwidth))
               {
                  if (x < end_x) x++;
                  else break;
               }
             if (x >= w) break;
             if (cells[x].att.newline)
               {
                  last0 = -1;
                  if ((y != c2y) || (x != end_x))
                    {
                       if (rtrim)
                         ty_sb_spaces_rtrim(sb);
                       if (ty_sb_add(sb, "\n", 1) < 0) goto err;
                    }
                  break;
               }
             else if (cells[x].codepoint == 0)
               {
                  if (last0 < 0) last0 = x;
               }
             else
               {
                  char txt[8];
                  int txtlen;

                  if (last0 >= 0)
                    {
                       v = x - last0 - 1;
                       last0 = -1;
                       while (v >= 0)
                         {
                            if (ty_sb_add(sb, " ", 1) < 0) goto err;
                            v--;
                         }
                    }
                  txtlen = codepoint_to_utf8(cells[x].codepoint, txt);
                  if (txtlen > 0)
                    if (ty_sb_add(sb, txt, txtlen) < 0) goto err;
                  if ((x == (w - 1)) &&
                      ((x != c2x) || (y != c2y)))
                    {
                       if (!cells[x].att.autowrapped)
                         {
                            if (rtrim)
                              ty_sb_spaces_rtrim(sb);
                            if (ty_sb_add(sb, "\n", 1) < 0) goto err;
                         }
                    }
               }
          }
        if (last0 >= 0)
          {
             if (y == c2y)
               {
                  Eina_Bool have_more = EINA_FALSE;

                  for (x = end_x + 1; x < w; x++)
                    {
                       if ((cells[x].codepoint == 0) &&
                           (cells[x].att.dblwidth))
                         {
                            if (x < (w - 1)) x++;
                            else break;
                         }
                       if (((cells[x].codepoint != 0) &&
                            (cells[x].codepoint != ' ')) ||
                           (cells[x].att.newline))
                         {
                            have_more = EINA_TRUE;
                            break;
                         }
                    }
                  if (!have_more)
                    {
                       if (rtrim)
                         ty_sb_spaces_rtrim(sb);
                       if (ty_sb_add(sb, "\n", 1) < 0) goto err;
                    }
                  else
                    {
                       for (x = last0; x <= end_x; x++)
                         {
                            if ((cells[x].codepoint == 0) &&
                                (cells[x].att.dblwidth))
                              {
                                 if (x < (w - 1)) x++;
                                 else break;
                              }
                            if (x >= w) break;
                            if (ty_sb_add(sb, " ", 1) < 0) goto err;
                         }
                    }
               }
             else
               {
                  if (rtrim)
                    ty_sb_spaces_rtrim(sb);
                  if (ty_sb_add(sb, "\n", 1) < 0) goto err;
               }
          }
     }
   termpty_backlog_unlock();

   if (rtrim)
     ty_sb_spaces_rtrim(sb);

   return;

err:
   ty_sb_free(sb);
}


struct Codepoints_Buf {
   Eina_Unicode *codepoints;
   size_t len;
   size_t size;
};

static int
_codepoint_buf_append(struct Codepoints_Buf *buf,
                      Eina_Unicode u)
{
   if (EINA_UNLIKELY(buf->len == buf->size))
     {
        buf->size *= 2;
        Eina_Unicode *codepoints = realloc(buf->codepoints,
                                           buf->size * sizeof(Eina_Unicode));
        if (EINA_UNLIKELY(!codepoints))
          {
             free(buf->codepoints);
             buf->len = buf->size = 0;
             return -1;
          }
        buf->codepoints = codepoints;
     }
   buf->codepoints[buf->len++] = u;
   return 0;
}

static void
_sel_codepoints_get(const Termio *sd,
                    struct Codepoints_Buf *buf,
                    int c1x, int c1y, int c2x, int c2y)
{
   int x, y;

#define TRY(ACTION) do {               \
     if (EINA_UNLIKELY(ACTION < 0))    \
       {                               \
          goto err;                    \
       }                               \
} while (0)

   termpty_backlog_lock();
   for (y = c1y; y <= c2y; y++)
     {
        Termcell *cells;
        ssize_t w = 0;
        int start_x, end_x;

        cells = termpty_cellrow_get(sd->pty, y, &w);
        if (!cells || !w || (y == c1y && c1x >= w))
          {
             w = 0;
          }

        /* Compute @start_x, @end_x */
        start_x = c1x;
        end_x = c2x;
        if (c1y != c2y)
          {
             if (y == c1y)
               {
                  end_x = sd->grid.w - 1;
               }
             else if (y == c2y)
               {
                  start_x = 0;
               }
             else
               {
                  start_x = 0;
                  end_x = sd->grid.w - 1;
               }
          }
        /* Lookup every cell in that line */
        for (x = start_x; x <= end_x; x++)
          {
             if (x >= w)
               {
                  /* Selection outside of current line of "text" */
                  TRY(_codepoint_buf_append(buf, ' '));
               }
             else if (cells[x].codepoint == 0)
               {
                  TRY(_codepoint_buf_append(buf, ' '));
               }
             else
               {
                  TRY(_codepoint_buf_append(buf, cells[x].codepoint));
               }
          }
     }
err:
   termpty_backlog_unlock();
}

static void
_sel_fill_in_codepoints_array(Termio *sd)
{
   int start_x = 0, start_y = 0, end_x = 0, end_y = 0;
   struct Codepoints_Buf buf = {
        .codepoints = NULL,
        .len = 0,
        .size = 256,
   };

   free(sd->pty->selection.codepoints);
   sd->pty->selection.codepoints = NULL;

   if (!sd->pty->selection.is_active)
     return;

   buf.codepoints = malloc(sizeof(Eina_Unicode) * buf.size);
   if (!buf.codepoints)
     return;

   start_x = sd->pty->selection.start.x;
   start_y = sd->pty->selection.start.y;
   end_x = sd->pty->selection.end.x;
   end_y = sd->pty->selection.end.y;

   if (!sd->pty->selection.is_top_to_bottom)
     {
        INT_SWAP(start_y, end_y);
        INT_SWAP(start_x, end_x);
     }

   if (sd->pty->selection.is_box)
     {
        int i;

        for (i = start_y; i <= end_y; i++)
          {
             _sel_codepoints_get(sd, &buf, start_x, i, end_x, i);
          }
     }
   else
     {
        _sel_codepoints_get(sd, &buf, start_x, start_y, end_x, end_y);
     }
   sd->pty->selection.codepoints = buf.codepoints;
}

const char *
termio_internal_get_selection(Termio *sd, size_t *lenp)
{
   int start_x = 0, start_y = 0, end_x = 0, end_y = 0;
   const char *s = NULL;
   size_t len = 0;

   EINA_SAFETY_ON_NULL_RETURN_VAL(sd, NULL);
   if (sd->pty->selection.is_active)
     {
        start_x = sd->pty->selection.start.x;
        start_y = sd->pty->selection.start.y;
        end_x = sd->pty->selection.end.x;
        end_y = sd->pty->selection.end.y;

        if (!sd->pty->selection.is_top_to_bottom)
          {
             INT_SWAP(start_y, end_y);
             INT_SWAP(start_x, end_x);
          }
     }
   else
     {
        if (sd->link.string)
          {
             len = strlen(sd->link.string);
             s = eina_stringshare_add_length(sd->link.string, len);
          }
        goto end;
     }

   if (sd->pty->selection.is_box)
     {
        int i;
        struct ty_sb sb = {.buf = NULL, .len = 0, .alloc = 0};

        for (i = start_y; i <= end_y; i++)
          {
             struct ty_sb isb = {.buf = NULL, .len = 0, .alloc = 0};
             termio_selection_get(sd, start_x, i, end_x, i,
                                  &isb, EINA_TRUE);

             if (isb.len)
               {
                  if (isb.buf[sb.len - 1] != '\n' && i != end_y)
                    ty_sb_add(&isb, "\n", 1);
                  ty_sb_add(&sb, isb.buf, isb.len);
               }
             ty_sb_free(&isb);
          }
        len = sb.len;
        s = ty_sb_steal_buf(&sb);
        s = eina_stringshare_add_length(s, len);
        ty_sb_free(&sb);
     }
   else
     {
        struct ty_sb sb = {.buf = NULL, .len = 0, .alloc = 0};

        termio_selection_get(sd, start_x, start_y, end_x, end_y, &sb, EINA_TRUE);
        len = sb.len;

        s = eina_stringshare_add_length(sb.buf, len);
        ty_sb_free(&sb);
     }

end:
   *lenp = len;
   return s;
}

static void
_sel_line(Termio *sd, int cy)
{
   int x, y;
   ssize_t w = 0;
   Termcell *cells;

   termpty_backlog_lock();

   termio_sel_set(sd, EINA_TRUE);
   sd->pty->selection.makesel = EINA_FALSE;

   sd->pty->selection.start.x = 0;
   sd->pty->selection.start.y = cy;
   sd->pty->selection.end.x = sd->grid.w - 1;
   sd->pty->selection.end.y = cy;

   /* check lines above */
   y = cy;
   for (;;)
     {
        cells = termpty_cellrow_get(sd->pty, y - 1, &w);
        if (!cells || !cells[w-1].att.autowrapped) break;

        y--;
     }
   sd->pty->selection.start.y = y;
   y = cy;

   /* check lines below */
   for (;;)
     {
        cells = termpty_cellrow_get(sd->pty, y, &w);
        if (!cells || !cells[w-1].att.autowrapped) break;

        sd->pty->selection.end.x = w - 1;
        y++;
     }
   /* Right trim */
   x = sd->pty->selection.end.x;
   while (x > 0 && ((cells[x].codepoint == 0) ||
                    (cells[x].codepoint == ' ') ||
                    (cells[x].att.newline)))
     {
        x--;
     }
   sd->pty->selection.end.x = x;
   sd->pty->selection.end.y = y;

   sd->pty->selection.by_line = EINA_TRUE;
   sd->pty->selection.is_top_to_bottom = EINA_TRUE;

   termpty_backlog_unlock();
}

static void
_sel_line_to(Termio *sd, int cy, Eina_Bool extend)
{
   int start_y, end_y, c_start_y, c_end_y,
       orig_y, orig_start_y, orig_end_y;

   EINA_SAFETY_ON_NULL_RETURN(sd);

   /* Only change the end position */
   orig_y = sd->pty->selection.orig.y;
   start_y = sd->pty->selection.start.y;
   end_y   = sd->pty->selection.end.y;

   if (!sd->pty->selection.is_top_to_bottom)
     INT_SWAP(start_y, end_y);

   _sel_line(sd, cy);
   c_start_y = sd->pty->selection.start.y;
   c_end_y   = sd->pty->selection.end.y;

   _sel_line(sd, orig_y);
   orig_start_y = sd->pty->selection.start.y;
   orig_end_y   = sd->pty->selection.end.y;

   if (sd->pty->selection.is_box)
     {
        if (extend)
          {
             if (start_y <= cy && cy <= end_y )
               {
                  start_y = MIN(c_start_y, orig_start_y);
                  end_y = MAX(c_end_y, orig_end_y);
               }
             else
               {
                  if (c_end_y > end_y)
                    {
                       orig_y = start_y;
                       end_y = c_end_y;
                    }
                  if (c_start_y < start_y)
                    {
                       orig_y = end_y;
                       start_y = c_start_y;
                    }
               }
             goto end;
          }
        else
          {
             start_y = MIN(c_start_y, orig_start_y);
             end_y = MAX(c_end_y, orig_end_y);
             goto end;
          }
     }
   else
     {
        if (c_start_y < start_y)
          {
             /* orig is at bottom */
             if (extend)
               {
                  orig_y = end_y;
               }
             sd->pty->selection.is_top_to_bottom = EINA_FALSE;
             end_y = c_start_y;
             start_y = orig_end_y;
          }
        else if (c_end_y > end_y)
          {
             if (extend)
               {
                  orig_y = start_y;
               }
             sd->pty->selection.is_top_to_bottom = EINA_TRUE;
             start_y = orig_start_y;
             end_y = c_end_y;
          }
        else
          {
             if (c_start_y < orig_start_y)
               {
                  sd->pty->selection.is_top_to_bottom = EINA_FALSE;
                  start_y = orig_end_y;
                  end_y = c_start_y;
               }
             else
               {
                  sd->pty->selection.is_top_to_bottom = EINA_TRUE;
                  start_y = orig_start_y;
                  end_y = c_end_y;
               }
          }
     }
end:
   sd->pty->selection.orig.y = orig_y;
   sd->pty->selection.start.y = start_y;
   sd->pty->selection.end.y = end_y;
   if (sd->pty->selection.is_top_to_bottom)
     {
        sd->pty->selection.start.x = 0;
        sd->pty->selection.end.x = sd->grid.w - 1;
     }
   else
     {
        sd->pty->selection.end.x = 0;
        sd->pty->selection.start.x = sd->grid.w - 1;
     }
}

static Eina_Bool
_codepoint_is_wordsep(const Eina_Unicode g)
{
   /* TODO: use bitmaps to speed things up */
   // http://en.wikipedia.org/wiki/Asterisk
   // http://en.wikipedia.org/wiki/Comma
   // http://en.wikipedia.org/wiki/Interpunct
   // http://en.wikipedia.org/wiki/Bracket
   static const Eina_Unicode wordsep[] =
     {
       0,
       ' ',
       '!',
       '"',
       '#',
       '$',
       '\'',
       '(',
       ')',
       '*',
       ',',
       ';',
       '=',
       '?',
       '[',
       '\\',
       ']',
       '^',
       '`',
       '{',
       '|',
       '}',
       0x00a0,
       0x00ab,
       0x00b7,
       0x00bb,
       0x0294,
       0x02bb,
       0x02bd,
       0x02d0,
       0x0312,
       0x0313,
       0x0314,
       0x0315,
       0x0326,
       0x0387,
       0x055d,
       0x055e,
       0x060c,
       0x061f,
       0x066d,
       0x07fb,
       0x1363,
       0x1367,
       0x14fe,
       0x1680,
       0x1802,
       0x1808,
       0x180e,
       0x2000,
       0x2001,
       0x2002,
       0x2003,
       0x2004,
       0x2005,
       0x2006,
       0x2007,
       0x2008,
       0x2009,
       0x200a,
       0x200b,
       0x2018,
       0x2019,
       0x201a,
       0x201b,
       0x201c,
       0x201d,
       0x201e,
       0x201f,
       0x2022,
       0x2027,
       0x202f,
       0x2039,
       0x203a,
       0x203b,
       0x203d,
       0x2047,
       0x2048,
       0x2049,
       0x204e,
       0x205f,
       0x2217,
       0x225f,
       0x2308,
       0x2309,
       0x2420,
       0x2422,
       0x2423,
       0x2722,
       0x2723,
       0x2724,
       0x2725,
       0x2731,
       0x2732,
       0x2733,
       0x273a,
       0x273b,
       0x273c,
       0x273d,
       0x2743,
       0x2749,
       0x274a,
       0x274b,
       0x2a7b,
       0x2a7c,
       0x2cfa,
       0x2e2e,
       0x2e2e,
       0x3000,
       0x3001,
       0x3008,
       0x3009,
       0x300a,
       0x300b,
       0x300c,
       0x300c,
       0x300d,
       0x300d,
       0x300e,
       0x300f,
       0x3010,
       0x3011,
       0x301d,
       0x301e,
       0x301f,
       0x30fb,
       0xa60d,
       0xa60f,
       0xa6f5,
       0xe0a0,
       0xe0b0,
       0xe0b2,
       0xfe10,
       0xfe41,
       0xfe42,
       0xfe43,
       0xfe44,
       0xfe50,
       0xfe51,
       0xfe56,
       0xfe61,
       0xfe62,
       0xfe63,
       0xfeff,
       0xff02,
       0xff07,
       0xff08,
       0xff09,
       0xff0a,
       0xff0c,
       0xff1b,
       0xff1c,
       0xff1e,
       0xff1f,
       0xff3b,
       0xff3d,
       0xff5b,
       0xff5d,
       0xff62,
       0xff63,
       0xff64,
       0xff65,
       0xe002a
   };
   size_t imax = (sizeof(wordsep) / sizeof(wordsep[0])) - 1,
          imin = 0;
   size_t imaxmax = imax;

   if (g & 0x80000000)
     return EINA_TRUE;

   while (imax >= imin)
     {
        size_t imid = (imin + imax) / 2;

        if (wordsep[imid] == g) return EINA_TRUE;
        else if (wordsep[imid] < g) imin = imid + 1;
        else imax = imid - 1;
        if (imax > imaxmax) break;
     }
   return EINA_FALSE;
}

static Eina_Bool
_to_trim(Eina_Unicode codepoint, Eina_Bool right_trim)
{
   static const Eina_Unicode trim_chars[] =
     {
       ' ',
       ':',
       '<',
       '>',
       '.'
     };
   size_t i = 0, len;
   len = sizeof(trim_chars)/sizeof((trim_chars)[0]);
   if (right_trim)
     len--; /* do not right trim . */

   for (i = 0; i < len; i++)
     if (codepoint == trim_chars[i])
       return EINA_TRUE;
   return EINA_FALSE;
}

static void
_trim_sel_word(Termio *sd)
{
   Termpty *pty = sd->pty;
   Termcell *cells;
   int start = 0, end = 0, y = 0;
   ssize_t w;

   /* 1st step: trim from the start */
   start = pty->selection.start.x;
   for (y = pty->selection.start.y;
        y <= pty->selection.end.y;
        y++)
     {
        cells = termpty_cellrow_get(pty, y, &w);
        if (!cells)
          return;

        while (start < w && _to_trim(cells[start].codepoint, EINA_TRUE))
          start++;

        if (start < w)
          break;

        start = 0;
     }
   /* check validy of the selection */
   if ((y > pty->selection.end.y) ||
       ((y == pty->selection.end.y) &&
        (start > pty->selection.end.x)))
     {
        termio_sel_set(sd, EINA_FALSE);
        return;
     }
   pty->selection.start.y = y;
   pty->selection.start.x = start;

   /* 2nd step: trim from the end */
   end = pty->selection.end.x;
   for (y = pty->selection.end.y;
        y >= pty->selection.start.y;
        y--)
     {
        cells = termpty_cellrow_get(pty, y, &w);
        if (!cells)
          return;

        while (end >= 0 && _to_trim(cells[end].codepoint, EINA_FALSE))
          end--;

        if (end >= 0)
          break;
     }
   if (end < 0)
     {
        return;
     }
   /* check validy of the selection */
   if ((y < pty->selection.start.y) ||
       ((y == pty->selection.start.y) &&
        (end < pty->selection.start.x)))
     {
        termio_sel_set(sd, EINA_FALSE);
        return;
     }
   pty->selection.end.x = end;
   pty->selection.end.y = y;
}

static void
_sel_word(Termio *sd, int cx, int cy)
{
   Termcell *cells;
   int x, y;
   ssize_t w = 0;
   Eina_Bool done = EINA_FALSE;

   termpty_backlog_lock();

   termio_sel_set(sd, EINA_TRUE);
   sd->pty->selection.makesel = EINA_TRUE;
   sd->pty->selection.orig.x = cx;
   sd->pty->selection.orig.y = cy;
   sd->pty->selection.start.x = cx;
   sd->pty->selection.start.y = cy;
   sd->pty->selection.end.x = cx;
   sd->pty->selection.end.y = cy;
   x = cx;
   y = cy;

   if (sd->link.string &&
       (sd->link.x1 <= cx) && (cx <= sd->link.x2) &&
       (sd->link.y1 <= cy) && (cy <= sd->link.y2))
     {
        sd->pty->selection.start.x = sd->link.x1;
        sd->pty->selection.start.y = sd->link.y1;
        sd->pty->selection.end.x = sd->link.x2;
        sd->pty->selection.end.y = sd->link.y2;
        goto end;
     }
   cells = termpty_cellrow_get(sd->pty, y, &w);
   if (!cells)
     {
        goto end;
     }
   if (x >= w)
     x = w - 1;

   do
     {
        for (; x >= 0; x--)
          {
             if ((cells[x].codepoint == 0) && (cells[x].att.dblwidth) &&
                 (x > 0))
               x--;
             if (_codepoint_is_wordsep(cells[x].codepoint))
               {
                  done = EINA_TRUE;
                  break;
               }
             sd->pty->selection.start.x = x;
             sd->pty->selection.start.y = y;
          }
        if (!done)
          {
             Termcell *old_cells = cells;

             cells = termpty_cellrow_get(sd->pty, y - 1, &w);
             if (!cells || !cells[w-1].att.autowrapped)
               {
                  x = 0;
                  cells = old_cells;
                  done = EINA_TRUE;
               }
             else
               {
                  y--;
                  x = w - 1;
               }
          }
     }
   while (!done);

   done = EINA_FALSE;
   if (cy != y)
     {
        y = cy;
        cells = termpty_cellrow_get(sd->pty, y, &w);
        if (!cells)
          {
             goto end;
          }
     }
   x = cx;

   do
     {
        for (; x < w; x++)
          {
             if ((cells[x].codepoint == 0) && (cells[x].att.dblwidth) &&
                 (x < (w - 1)))
               {
                  sd->pty->selection.end.x = x;
                  x++;
               }
             if (_codepoint_is_wordsep(cells[x].codepoint))
               {
                  done = EINA_TRUE;
                  break;
               }
             sd->pty->selection.end.x = x;
             sd->pty->selection.end.y = y;
          }
        if (!done)
          {
             if (!cells[w - 1].att.autowrapped)
               goto end;
             y++;
             x = 0;
             cells = termpty_cellrow_get(sd->pty, y, &w);
             if (!cells)
               {
                  goto end;
               }
          }
     }
   while (!done);

  end:

   sd->pty->selection.by_word = EINA_TRUE;
   sd->pty->selection.is_top_to_bottom = EINA_TRUE;

   _trim_sel_word(sd);

   termpty_backlog_unlock();
}

static void
_sel_word_to(Termio *sd, int cx, int cy, Eina_Bool extend)
{
   int start_x, start_y, end_x, end_y, orig_x, orig_y,
       c_start_x, c_start_y, c_end_x, c_end_y,
       orig_start_x, orig_start_y, orig_end_x, orig_end_y;

   EINA_SAFETY_ON_NULL_RETURN(sd);

   orig_x = sd->pty->selection.orig.x;
   orig_y = sd->pty->selection.orig.y;
   start_x = sd->pty->selection.start.x;
   start_y = sd->pty->selection.start.y;
   end_x   = sd->pty->selection.end.x;
   end_y   = sd->pty->selection.end.y;

   if (!sd->pty->selection.is_top_to_bottom)
     {
        INT_SWAP(start_x, end_x);
        INT_SWAP(start_y, end_y);
     }

   _sel_word(sd, cx, cy);
   c_start_x = sd->pty->selection.start.x;
   c_start_y = sd->pty->selection.start.y;
   c_end_x   = sd->pty->selection.end.x;
   c_end_y   = sd->pty->selection.end.y;

   _sel_word(sd, orig_x, orig_y);
   orig_start_x = sd->pty->selection.start.x;
   orig_start_y = sd->pty->selection.start.y;
   orig_end_x   = sd->pty->selection.end.x;
   orig_end_y   = sd->pty->selection.end.y;

   if (sd->pty->selection.is_box)
     {
        if (extend)
          {
             /* special case: kind of line selection */
             if (c_start_y != c_end_y)
               {
                  start_x = 0;
                  end_x = sd->grid.w - 1;
                  if (start_y <= cy && cy <= end_y )
                    {
                       start_y = MIN(c_start_y, orig_start_y);
                       end_y = MAX(c_end_y, orig_end_y);
                    }
                  else
                    {
                       if (c_end_y > end_y)
                         {
                            orig_y = start_y;
                            end_y = c_end_y;
                         }
                       if (c_start_y < start_y)
                         {
                            orig_y = end_y;
                            start_y = c_start_y;
                         }
                    }
                  goto end;
               }
             if ((start_y <= cy && cy <= end_y ) &&
                 (start_x <= cx && cx <= end_x ))
               {
                  start_x = MIN(c_start_x, orig_start_x);
                  end_x = MAX(c_end_x, orig_end_x);
                  start_y = MIN(c_start_y, orig_start_y);
                  end_y = MAX(c_end_y, orig_end_y);
               }
             else
               {
                  if (c_end_x > end_x)
                    {
                       orig_x = start_x;
                       end_x = c_end_x;
                    }
                  if (c_start_x < start_x)
                    {
                       orig_x = end_x;
                       start_x = c_start_x;
                    }
                  if (c_end_y > end_y)
                    {
                       orig_y = start_y;
                       end_y = c_end_y;
                    }
                  if (c_start_y < start_y)
                    {
                       orig_y = end_y;
                       start_y = c_start_y;
                    }
                  end_x = MAX(c_end_x, end_x);
                  start_y = MIN(c_start_y, start_y);
                  end_y = MAX(c_end_y, end_y);
               }
          }
        else
          {
             /* special case: kind of line selection */
             if (c_start_y != c_end_y || orig_start_y != orig_end_y)
               {
                  start_x = 0;
                  end_x = sd->grid.w - 1;
                  start_y = MIN(c_start_y, orig_start_y);
                  end_y = MAX(c_end_y, orig_end_y);
                  goto end;
               }

             start_x = MIN(c_start_x, orig_start_x);
             end_x = MAX(c_end_x, orig_end_x);
             start_y = MIN(c_start_y, orig_start_y);
             end_y = MAX(c_end_y, orig_end_y);
          }
     }
   else
     {
        if (c_start_y < start_y ||
            (c_start_y == start_y &&
             c_start_x < start_x))
          {
             /* orig is at bottom */
             if (extend)
               {
                  orig_x = end_x;
                  orig_y = end_y;
               }
             sd->pty->selection.is_top_to_bottom = EINA_FALSE;
             end_x = c_start_x;
             end_y = c_start_y;
             start_x = orig_end_x;
             start_y = orig_end_y;
          }
        else if (c_end_y > end_y ||
                 (c_end_y == end_y && c_end_x >= end_x))
          {
             if (extend)
               {
                  orig_x = start_x;
                  orig_y = start_y;
               }
             sd->pty->selection.is_top_to_bottom = EINA_TRUE;
             start_x = orig_start_x;
             start_y = orig_start_y;
             end_x = c_end_x;
             end_y = c_end_y;
          }
        else
          {
             if (c_start_y < orig_start_y ||
                 (c_start_y == orig_start_y && c_start_x <= orig_start_x))
               {
                  sd->pty->selection.is_top_to_bottom = EINA_FALSE;
                  start_x = orig_end_x;
                  start_y = orig_end_y;
                  end_x = c_start_x;
                  end_y = c_start_y;
               }
             else
               {
                  sd->pty->selection.is_top_to_bottom = EINA_TRUE;
                  start_x = orig_start_x;
                  start_y = orig_start_y;
                  end_x = c_end_x;
                  end_y = c_end_y;
               }
          }
     }

end:
   sd->pty->selection.orig.x = orig_x;
   sd->pty->selection.orig.y = orig_y;
   sd->pty->selection.start.x = start_x;
   sd->pty->selection.start.y = start_y;
   sd->pty->selection.end.x = end_x;
   sd->pty->selection.end.y = end_y;
}

static void
_sel_to(Termio *sd, int cx, int cy, Eina_Bool extend)
{
   int start_x, start_y, end_x, end_y, orig_x, orig_y;

   EINA_SAFETY_ON_NULL_RETURN(sd);

   orig_x = sd->pty->selection.orig.x;
   orig_y = sd->pty->selection.orig.y;
   start_x = sd->pty->selection.start.x;
   start_y = sd->pty->selection.start.y;
   end_x   = sd->pty->selection.end.x;
   end_y   = sd->pty->selection.end.y;

   if (sd->pty->selection.is_box)
     {
        if (!sd->pty->selection.is_top_to_bottom)
          INT_SWAP(start_y, end_y);
        if (start_x > end_x)
          INT_SWAP(start_x, end_x);

        if (cy < start_y)
          {
             start_y = cy;
          }
        else if (cy > end_y)
          {
             end_y = cy;
          }
        else
          {
             start_y = orig_y;
             end_y = cy;
          }

        if (cx < start_x)
          {
             start_x = cx;
          }
        else if (cx > end_x)
          {
             end_x = cx;
          }
        else
          {
             start_x = orig_x;
             end_x = cx;
          }
        sd->pty->selection.is_top_to_bottom = (end_y > start_y);
        if (sd->pty->selection.is_top_to_bottom)
          {
             if (start_x > end_x)
               INT_SWAP(start_x, end_x);
          }
        else
          {
             if (start_x < end_x)
               INT_SWAP(start_x, end_x);
          }
     }
   else
     {
        if (!sd->pty->selection.is_top_to_bottom)
          {
             INT_SWAP(start_x, end_x);
             INT_SWAP(start_y, end_y);
          }
        if (cy < start_y ||
            (cy == start_y &&
             cx < start_x))
          {
             /* orig is at bottom */
             if (sd->pty->selection.is_top_to_bottom && extend)
               {
                  orig_x = end_x;
                  orig_y = end_y;
               }
             sd->pty->selection.is_top_to_bottom = EINA_FALSE;
          }
        else if (cy > end_y ||
                 (cy == end_y && cx >= end_x))
          {
             if (!sd->pty->selection.is_top_to_bottom && extend)
               {
                  orig_x = start_x;
                  orig_y = start_y;
               }
             sd->pty->selection.is_top_to_bottom = EINA_TRUE;
          }
        else
          {
             sd->pty->selection.is_top_to_bottom =
                (cy > orig_y) || (cy == orig_y && cx > orig_x);
          }
        start_x = orig_x;
        start_y = orig_y;
        end_x = cx;
        end_y = cy;
     }

   sd->pty->selection.orig.x = orig_x;
   sd->pty->selection.orig.y = orig_y;
   sd->pty->selection.start.x = start_x;
   sd->pty->selection.start.y = start_y;
   sd->pty->selection.end.x = end_x;
   sd->pty->selection.end.y = end_y;
}

static void
_selection_newline_extend_fix(Termio *sd)
{
   int start_x, start_y, end_x, end_y;
   ssize_t w;

   if ((sd->top_left) || (sd->bottom_right) || (sd->pty->selection.is_box))
     return;

   termpty_backlog_lock();

   start_x = sd->pty->selection.start.x;
   start_y = sd->pty->selection.start.y;
   end_x   = sd->pty->selection.end.x;
   end_y   = sd->pty->selection.end.y;
   if (!sd->pty->selection.is_top_to_bottom)
     {
        INT_SWAP(start_y, end_y);
        INT_SWAP(start_x, end_x);
     }

   if ((end_y > start_y) ||
       ((end_y == start_y) &&
        (end_x >= start_x)))
     {
        /* going down/right */
        w = termpty_row_length(sd->pty, start_y);
        if (w < start_x)
          start_x = w;
        w = termpty_row_length(sd->pty, end_y);
        if (w <= end_x)
          end_x = sd->pty->w;
     }
   else
     {
        /* going up/left */
        w = termpty_row_length(sd->pty, end_y);
        if (w < end_x)
          end_x = w;
        w = termpty_row_length(sd->pty, start_y);
        if (w <= start_x)
          start_x = sd->pty->w;
     }

   if (!sd->pty->selection.is_top_to_bottom)
     {
        INT_SWAP(start_y, end_y);
        INT_SWAP(start_x, end_x);
     }
   sd->pty->selection.start.x = start_x;
   sd->pty->selection.start.y = start_y;
   sd->pty->selection.end.x = end_x;
   sd->pty->selection.end.y = end_y;

   termpty_backlog_unlock();
}

/* }}} */

void
termio_selection_dbl_fix(Termio *sd)
{
   int start_x, start_y, end_x, end_y;
   ssize_t w = 0;
   Termcell *cells;
   /* Only change the end position */

   EINA_SAFETY_ON_NULL_RETURN(sd);

   start_x = sd->pty->selection.start.x;
   start_y = sd->pty->selection.start.y;
   end_x   = sd->pty->selection.end.x;
   end_y   = sd->pty->selection.end.y;
   if (!sd->pty->selection.is_top_to_bottom)
     {
        INT_SWAP(start_y, end_y);
        INT_SWAP(start_x, end_x);
     }

   termpty_backlog_lock();
   cells = termpty_cellrow_get(sd->pty, end_y - sd->scroll, &w);
   if (cells)
     {
        // if sel2 after sel1
        if ((end_y > start_y) ||
            ((end_y == start_y) &&
                (end_x >= start_x)))
          {
             if (end_x < (w - 1))
               {
                  if ((cells[end_x].codepoint != 0) &&
                      (cells[end_x].att.dblwidth))
                    end_x++;
               }
          }
        // else sel1 after sel 2
        else
          {
             if (end_x > 0)
               {
                  if ((cells[end_x].codepoint == 0) &&
                      (cells[end_x].att.dblwidth))
                    end_x--;
               }
          }
     }
   cells = termpty_cellrow_get(sd->pty, start_y - sd->scroll, &w);
   if (cells)
     {
        // if sel2 after sel1
        if ((end_y > start_y) ||
            ((end_y == start_y) &&
                (end_x >= start_x)))
          {
             if (start_x > 0)
               {
                  if ((cells[start_x].codepoint == 0) &&
                      (cells[start_x].att.dblwidth))
                    start_x--;
               }
          }
        // else sel1 after sel 2
        else
          {
             if (start_x < (w - 1))
               {
                  if ((cells[start_x].codepoint != 0) &&
                      (cells[start_x].att.dblwidth))
                    start_x++;
               }
          }
     }
   termpty_backlog_unlock();

   if (!sd->pty->selection.is_top_to_bottom)
     {
        INT_SWAP(start_y, end_y);
        INT_SWAP(start_x, end_x);
     }
   sd->pty->selection.start.x = start_x;
   sd->pty->selection.start.y = start_y;
   sd->pty->selection.end.x = end_x;
   sd->pty->selection.end.y = end_y;
}


static void
_handle_mouse_down_single_click(Termio *sd,
                                int cx, int cy,
                                Termio_Modifiers modifiers)
{
   sd->didclick = EINA_FALSE;
   /* SINGLE CLICK */
   if (sd->pty->selection.is_active &&
       (sd->top_left || sd->bottom_right))
     {
        /* stretch selection */
        int start_x, start_y, end_x, end_y;

        start_x = sd->pty->selection.start.x;
        start_y = sd->pty->selection.start.y;
        end_x   = sd->pty->selection.end.x;
        end_y   = sd->pty->selection.end.y;

        if (!sd->pty->selection.is_top_to_bottom)
          {
             INT_SWAP(start_y, end_y);
             INT_SWAP(start_x, end_x);
          }

        cy -= sd->scroll;

        sd->pty->selection.makesel = EINA_TRUE;

        if (sd->pty->selection.is_box)
          {
             if (end_x < start_x)
               INT_SWAP(end_x, start_x);
          }
        if (sd->top_left)
          {
             sd->pty->selection.orig.x = end_x;
             sd->pty->selection.orig.y = end_y;
             sd->pty->selection.is_top_to_bottom = EINA_FALSE;
          }
        else
          {
             /* sd->bottom_right */
             sd->pty->selection.orig.x = start_x;
             sd->pty->selection.orig.y = start_y;
             sd->pty->selection.is_top_to_bottom = EINA_TRUE;
          }

        sd->pty->selection.start.x = sd->pty->selection.orig.x;
        sd->pty->selection.start.y = sd->pty->selection.orig.y;
        sd->pty->selection.end.x = cx;
        sd->pty->selection.end.y = cy;
        termio_selection_dbl_fix(sd);
     }
   else if (!modifiers.shift && modifiers.alt && !sd->pty->selection.is_active
            && (sd->pty->mouse_mode == MOUSE_OFF))
     {
        /* move cursor to position */
        termpty_move_cursor(sd->pty, cx, cy);
     }
   else if (!modifiers.shift && !sd->pty->selection.is_active)
     {
        /* New selection */
        sd->moved = EINA_FALSE;
        termio_sel_set(sd, EINA_FALSE);
        sd->pty->selection.is_box = (modifiers.ctrl || modifiers.alt);
        sd->pty->selection.start.x = cx;
        sd->pty->selection.start.y = cy - sd->scroll;
        sd->pty->selection.orig.x = sd->pty->selection.start.x;
        sd->pty->selection.orig.y = sd->pty->selection.start.y;
        sd->pty->selection.end.x = cx;
        sd->pty->selection.end.y = cy - sd->scroll;
        sd->pty->selection.makesel = EINA_TRUE;
        sd->pty->selection.by_line = EINA_FALSE;
        sd->pty->selection.by_word = EINA_FALSE;
        termio_selection_dbl_fix(sd);
     }
   else if (modifiers.shift && sd->pty->selection.is_active)
     {
        /* let cb_up handle it */
        /* do nothing */
        return;
     }
   else if (modifiers.shift &&
            (time(NULL) - sd->pty->selection.last_click) <= 5)
     {
        sd->pty->selection.is_box = modifiers.ctrl;
        _sel_to(sd, cx, cy - sd->scroll, EINA_FALSE);
        sd->pty->selection.is_active = EINA_TRUE;
        termio_selection_dbl_fix(sd);
     }
   else
     {
        sd->pty->selection.is_box = modifiers.ctrl;
        sd->pty->selection.start.x = sd->pty->selection.end.x = cx;
        sd->pty->selection.orig.x = cx;
        sd->pty->selection.start.y = sd->pty->selection.end.y = cy - sd->scroll;
        sd->pty->selection.orig.y = cy - sd->scroll;
        sd->pty->selection.makesel = EINA_TRUE;
        sd->didclick = !sd->pty->selection.is_active;
        sd->pty->selection.is_active = EINA_FALSE;
        termio_sel_set(sd, EINA_FALSE);
     }
}

static Eina_Bool
_rep_mouse_down(Termio *sd, Evas_Event_Mouse_Down *ev,
                int cx, int cy, Termio_Modifiers modifiers)
{
   char buf[64];
   Eina_Bool ret = EINA_FALSE;
   int btn;

   if (sd->pty->mouse_mode == MOUSE_OFF)
     return EINA_FALSE;
   if (!sd->mouse.button)
     {
        /* Need to remember the first button pressed for terminal handling */
        sd->mouse.button = ev->button;
     }

   btn = ev->button - 1;
   switch (sd->pty->mouse_ext)
     {
      case MOUSE_EXT_NONE:
        if ((cx < (0xff - ' ')) && (cy < (0xff - ' ')))
          {
             if (sd->pty->mouse_mode == MOUSE_X10)
               {
                  if (btn <= 2)
                    {
                       buf[0] = 0x1b;
                       buf[1] = '[';
                       buf[2] = 'M';
                       buf[3] = btn + ' ';
                       buf[4] = cx + 1 + ' ';
                       buf[5] = cy + 1 + ' ';
                       buf[6] = 0;
                       termpty_write(sd->pty, buf, strlen(buf));
                       ret = EINA_TRUE;
                    }
               }
             else
               {
                  int meta = (modifiers.alt) ? 8 : 0;

                  if (btn > 2) btn = 0;
                  buf[0] = 0x1b;
                  buf[1] = '[';
                  buf[2] = 'M';
                  buf[3] = (btn | meta) + ' ';
                  buf[4] = cx + 1 + ' ';
                  buf[5] = cy + 1 + ' ';
                  buf[6] = 0;
                  termpty_write(sd->pty, buf, strlen(buf));
                  ret = EINA_TRUE;
               }
          }
        break;
      case MOUSE_EXT_UTF8: // ESC.[.M.BTN/FLGS.XUTF8.YUTF8
          {
             int meta = (modifiers.alt) ? 8 : 0;
             int v, i;

             if (btn > 2) btn = 0;
             buf[0] = 0x1b;
             buf[1] = '[';
             buf[2] = 'M';
             buf[3] = (btn | meta) + ' ';
             i = 4;
             v = cx + 1 + ' ';
             if (v <= 127) buf[i++] = v;
             else
               { // 14 bits for cx/cy - enough i think
                   buf[i++] = 0xc0 + (v >> 6);
                   buf[i++] = 0x80 + (v & 0x3f);
               }
             v = cy + 1 + ' ';
             if (v <= 127) buf[i++] = v;
             else
               { // 14 bits for cx/cy - enough i think
                   buf[i++] = 0xc0 + (v >> 6);
                   buf[i++] = 0x80 + (v & 0x3f);
               }
             buf[i] = 0;
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      case MOUSE_EXT_SGR: // ESC.[.<.NUM.;.NUM.;.NUM.M
          {
             int meta = (modifiers.alt) ? 8 : 0;

             if (btn > 2) btn = 0;
             snprintf(buf, sizeof(buf), "%c[<%i;%i;%iM", 0x1b,
                      (btn | meta), cx + 1, cy + 1);
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      case MOUSE_EXT_URXVT: // ESC.[.NUM.;.NUM.;.NUM.M
          {
             int meta = (modifiers.alt) ? 8 : 0;

             if (btn > 2) btn = 0;
             snprintf(buf, sizeof(buf), "%c[%i;%i;%iM", 0x1b,
                      (btn | meta) + ' ',
                      cx + 1, cy + 1);
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      default:
        break;
     }
   return ret;
}

void
termio_internal_mouse_down(Termio *sd,
                           Evas_Event_Mouse_Down *ev,
                           Termio_Modifiers modifiers)
{
   int cx, cy;

   termio_cursor_to_xy(sd, ev->canvas.x, ev->canvas.y, &cx, &cy);

   sd->mouse.cx = cx;
   sd->mouse.cy = cy;

   if ((ev->button == 3) && modifiers.ctrl)
     {
        termio_handle_right_click(ev, sd, cx, cy);
        return;
     }
   if (!modifiers.shift && !modifiers.ctrl)
     if (_rep_mouse_down(sd, ev, cx, cy, modifiers))
       {
           if (sd->pty->selection.is_active)
             {
                termio_sel_set(sd, EINA_FALSE);
                termio_smart_update_queue(sd);
             }
          return;
       }
   if (ev->button == 1)
     {
        sd->pty->selection.makesel = EINA_TRUE;
        if (ev->flags & EVAS_BUTTON_TRIPLE_CLICK)
          {
             if (modifiers.shift && sd->pty->selection.is_active)
               _sel_line_to(sd, cy - sd->scroll, EINA_TRUE);
             else
               _sel_line(sd, cy - sd->scroll);
             if (sd->pty->selection.is_active)
               {
                  termio_take_selection(sd->self, ELM_SEL_TYPE_PRIMARY);
               }
             sd->didclick = EINA_TRUE;
             _sel_fill_in_codepoints_array(sd);
          }
        else if (ev->flags & EVAS_BUTTON_DOUBLE_CLICK)
          {
             if (!sd->pty->selection.is_active && sd->didclick)
               {
                  sd->pty->selection.is_active = EINA_TRUE;
               }
             if (modifiers.shift && sd->pty->selection.is_active)
               {
                  _sel_word_to(sd, cx, cy - sd->scroll, EINA_TRUE);
               }
             else
               {
                  _sel_word(sd, cx, cy - sd->scroll);
               }
             if (sd->pty->selection.is_active)
               {
                  if (!termio_take_selection(sd->self, ELM_SEL_TYPE_PRIMARY))
                    {
                       termio_sel_set(sd, EINA_FALSE);
                    }
               }
             sd->didclick = EINA_TRUE;
             _sel_fill_in_codepoints_array(sd);
          }
        else
          {
             _handle_mouse_down_single_click(sd, cx, cy, modifiers);
          }
        termio_smart_update_queue(sd);
     }
   else if (ev->button == 2)
     {
        termio_paste_selection(sd->self, ELM_SEL_TYPE_PRIMARY);
     }
   else if (ev->button == 3)
     {
        termio_handle_right_click(ev, sd, cx, cy);
     }
}

static Eina_Bool
_rep_mouse_up(Termio *sd, Evas_Event_Mouse_Up *ev,
              int cx, int cy, Termio_Modifiers modifiers)
{
   char buf[64];
   Eina_Bool ret = EINA_FALSE;
   int meta;

   if ((sd->pty->mouse_mode == MOUSE_OFF) ||
       (sd->pty->mouse_mode == MOUSE_X10))
     return EINA_FALSE;
   if (sd->mouse.button == ev->button)
     sd->mouse.button = 0;

   meta = (modifiers.alt) ? 8 : 0;

   switch (sd->pty->mouse_ext)
     {
      case MOUSE_EXT_NONE:
        if ((cx < (0xff - ' ')) && (cy < (0xff - ' ')))
          {
             buf[0] = 0x1b;
             buf[1] = '[';
             buf[2] = 'M';
             buf[3] = (3 | meta) + ' ';
             buf[4] = cx + 1 + ' ';
             buf[5] = cy + 1 + ' ';
             buf[6] = 0;
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      case MOUSE_EXT_UTF8: // ESC.[.M.BTN/FLGS.XUTF8.YUTF8
          {
             int v, i;

             buf[0] = 0x1b;
             buf[1] = '[';
             buf[2] = 'M';
             buf[3] = (3 | meta) + ' ';
             i = 4;
             v = cx + 1 + ' ';
             if (v <= 127) buf[i++] = v;
             else
               { // 14 bits for cx/cy - enough i think
                   buf[i++] = 0xc0 + (v >> 6);
                   buf[i++] = 0x80 + (v & 0x3f);
               }
             v = cy + 1 + ' ';
             if (v <= 127) buf[i++] = v;
             else
               { // 14 bits for cx/cy - enough i think
                   buf[i++] = 0xc0 + (v >> 6);
                   buf[i++] = 0x80 + (v & 0x3f);
               }
             buf[i] = 0;
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      case MOUSE_EXT_SGR: // ESC.[.<.NUM.;.NUM.;.NUM.m
          {
             int btn = ev->button - 1;
             if (btn > 2) btn = 0;
             snprintf(buf, sizeof(buf), "%c[<%i;%i;%im", 0x1b,
                      (btn | meta), cx + 1, cy + 1);
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      case MOUSE_EXT_URXVT: // ESC.[.NUM.;.NUM.;.NUM.M
          {
             snprintf(buf, sizeof(buf), "%c[%i;%i;%iM", 0x1b,
                      (3 | meta) + ' ',
                      cx + 1, cy + 1);
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      default:
        break;
     }
   return ret;
}

static Eina_Bool
_rep_mouse_move(Termio *sd, int cx, int cy)
{
   char buf[64];
   Eina_Bool ret = EINA_FALSE;
   int btn;

   if ((sd->pty->mouse_mode == MOUSE_OFF) ||
       (sd->pty->mouse_mode == MOUSE_X10) ||
       (sd->pty->mouse_mode == MOUSE_NORMAL))
     return EINA_FALSE;

   if ((!sd->mouse.button) && (sd->pty->mouse_mode == MOUSE_NORMAL_BTN_MOVE))
     return EINA_FALSE;

   btn = sd->mouse.button - 1;

   switch (sd->pty->mouse_ext)
     {
      case MOUSE_EXT_NONE:
        if ((cx < (0xff - ' ')) && (cy < (0xff - ' ')))
          {
             buf[0] = 0x1b;
             buf[1] = '[';
             buf[2] = 'M';
             buf[3] = btn + 32 + ' ';
             buf[4] = cx + 1 + ' ';
             buf[5] = cy + 1 + ' ';
             buf[6] = 0;
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      case MOUSE_EXT_UTF8: // ESC.[.M.BTN/FLGS.XUTF8.YUTF8
          {
             int v, i;

             buf[0] = 0x1b;
             buf[1] = '[';
             buf[2] = 'M';
             buf[3] = btn + 32 + ' ';
             i = 4;
             v = cx + 1 + ' ';
             if (v <= 127) buf[i++] = v;
             else
               { // 14 bits for cx/cy - enough i think
                   buf[i++] = 0xc0 + (v >> 6);
                   buf[i++] = 0x80 + (v & 0x3f);
               }
             v = cy + 1 + ' ';
             if (v <= 127) buf[i++] = v;
             else
               { // 14 bits for cx/cy - enough i think
                   buf[i++] = 0xc0 + (v >> 6);
                   buf[i++] = 0x80 + (v & 0x3f);
               }
             buf[i] = 0;
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      case MOUSE_EXT_SGR: // ESC.[.<.NUM.;.NUM.;.NUM.M
          {
             snprintf(buf, sizeof(buf), "%c[<%i;%i;%iM", 0x1b,
                      btn + 32, cx + 1, cy + 1);
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      case MOUSE_EXT_URXVT: // ESC.[.NUM.;.NUM.;.NUM.M
          {
             snprintf(buf, sizeof(buf), "%c[%i;%i;%iM", 0x1b,
                      btn + 32  + ' ',
                      cx + 1, cy + 1);
             termpty_write(sd->pty, buf, strlen(buf));
             ret = EINA_TRUE;
          }
        break;
      default:
        break;
     }
   return ret;
}


void
termio_internal_mouse_up(Termio *sd,
                         Evas_Event_Mouse_Up *ev,
                         Termio_Modifiers modifiers)
{
   int cx = 0, cy = 0;

   termio_cursor_to_xy(sd, ev->canvas.x, ev->canvas.y, &cx, &cy);

   sd->mouse.cx = cx;
   sd->mouse.cy = cy;

   if (!modifiers.shift && !modifiers.ctrl && !sd->pty->selection.makesel)
      if (_rep_mouse_up(sd, ev, cx, cy, modifiers))
        {
           if (sd->pty->selection.is_active)
             {
                termio_sel_set(sd, EINA_FALSE);
                termio_smart_update_queue(sd);
             }
           return;
        }
   if (sd->link.down.dnd)
     return;
   if (sd->pty->selection.makesel)
     {
        if (sd->mouse_selection_scroll_timer)
          {
             ecore_timer_del(sd->mouse_selection_scroll_timer);
             sd->mouse_selection_scroll_timer = NULL;
          }
        sd->pty->selection.makesel = EINA_FALSE;

        if (!sd->pty->selection.is_active)
          {
             /* Only change the end position */
             if (((sd->pty->selection.start.x == sd->pty->selection.end.x) &&
                  (sd->pty->selection.start.y == sd->pty->selection.end.y)))
               {
                  termio_sel_set(sd, EINA_FALSE);
                  sd->didclick = EINA_FALSE;
                  sd->pty->selection.last_click = time(NULL);
                  sd->pty->selection.by_line = EINA_FALSE;
                  sd->pty->selection.by_word = EINA_FALSE;
                  _sel_fill_in_codepoints_array(sd);
                  termio_smart_update_queue(sd);
                  return;
               }
          }
        else
          {
             if (sd->pty->selection.by_line)
               {
                  _sel_line_to(sd, cy - sd->scroll, modifiers.shift);
               }
             else if (sd->pty->selection.by_word)
               {
                  _sel_word_to(sd, cx, cy - sd->scroll, modifiers.shift);
               }
             else
               {
                  if (modifiers.shift)
                    {
                       /* extend selection */
                       _sel_to(sd, cx, cy - sd->scroll, EINA_TRUE);
                    }
                  else
                    {
                       sd->didclick = EINA_TRUE;
                       _sel_to(sd, cx, cy - sd->scroll, EINA_FALSE);
                    }
               }
             termio_selection_dbl_fix(sd);
             _selection_newline_extend_fix(sd);
             termio_take_selection(sd->self, ELM_SEL_TYPE_PRIMARY);
             _sel_fill_in_codepoints_array(sd);
             sd->pty->selection.makesel = EINA_FALSE;
             termio_smart_update_queue(sd);
          }
     }
}

static Eina_Bool
_mouse_selection_scroll(void *data)
{
   Termio *sd = data;
   Evas_Coord oy, my;
   int cy;
   float fcy;

   if (!sd->pty->selection.makesel)
     return EINA_FALSE;

   evas_pointer_canvas_xy_get(evas_object_evas_get(sd->self), NULL, &my);
   termio_object_geometry_get(sd, NULL, &oy, NULL, NULL);
   fcy = (my - oy) / (float)sd->font.chh;
   cy = fcy;
   if (fcy < 0.3)
     {
        if (cy == 0)
          cy = -1;
        sd->scroll -= cy;
        sd->pty->selection.end.y = -sd->scroll;
        termio_smart_update_queue(sd);
     }
   else if (fcy >= (sd->grid.h - 0.3))
     {
        if (cy <= sd->grid.h)
          cy = sd->grid.h + 1;
        sd->scroll -= cy - sd->grid.h;
        if (sd->scroll < 0) sd->scroll = 0;
        sd->pty->selection.end.y = sd->scroll + sd->grid.h - 1;
        termio_smart_update_queue(sd);
     }

   return EINA_TRUE;
}

void
termio_cursor_to_xy(Termio *sd, Evas_Coord x, Evas_Coord y,
                    int *cx, int *cy)
{
   Evas_Coord ox, oy;

   termio_object_geometry_get(sd, &ox, &oy, NULL, NULL);
   *cx = (x - ox) / sd->font.chw;
   *cy = (y - oy) / sd->font.chh;
   if (*cx < 0) *cx = 0;
   else if (*cx >= sd->grid.w) *cx = sd->grid.w - 1;
   if (*cy < 0) *cy = 0;
   else if (*cy >= sd->grid.h) *cy = sd->grid.h - 1;
}


void
termio_internal_mouse_move(Termio *sd,
                           Evas_Event_Mouse_Move *ev,
                           Termio_Modifiers modifiers)
{
   int cx, cy;
   float fcy;
   Evas_Coord ox, oy;
   Eina_Bool scroll = EINA_FALSE;

   termio_object_geometry_get(sd, &ox, &oy, NULL, NULL);
   cx = (ev->cur.canvas.x - ox) / sd->font.chw;
   fcy = (ev->cur.canvas.y - oy) / (float)sd->font.chh;

   cy = fcy;
   if (cx < 0)
     cx = 0;
   else if (cx >= sd->grid.w)
     cx = sd->grid.w - 1;
   if (fcy < 0.3)
     {
        cy = 0;
        if (sd->pty->selection.makesel)
             scroll = EINA_TRUE;
     }
   else if (fcy >= (sd->grid.h - 0.3))
     {
        cy = sd->grid.h - 1;
        if (sd->pty->selection.makesel)
             scroll = EINA_TRUE;
     }
   if (scroll == EINA_TRUE)
     {
        if (!sd->mouse_selection_scroll_timer)
          {
             sd->mouse_selection_scroll_timer
                = ecore_timer_add(0.05, _mouse_selection_scroll, sd);
          }
        return;
     }
   else if (sd->mouse_selection_scroll_timer)
     {
        ecore_timer_del(sd->mouse_selection_scroll_timer);
        sd->mouse_selection_scroll_timer = NULL;
     }

   /* Cursor has not changed cells */
   if ((sd->mouse.cx == cx) && (sd->mouse.cy == cy))
     {
        return;
     }

   sd->mouse.cx = cx;
   sd->mouse.cy = cy;
   if (!modifiers.shift && !modifiers.ctrl)
     {
        if (_rep_mouse_move(sd, cx, cy))
          {
             /* Mouse move already been taken care of */
             return;
          }
     }
   if (sd->link.down.dnd)
     {
        sd->pty->selection.makesel = EINA_FALSE;
        termio_sel_set(sd, EINA_FALSE);
        termio_smart_update_queue(sd);
        return;
     }
   if (sd->pty->selection.makesel)
     {
        int start_x, start_y;

        /* Only change the end position */
        start_x = sd->pty->selection.start.x;
        start_y = sd->pty->selection.start.y;

        if (!sd->pty->selection.is_active)
          {
             if ((cx != start_x) ||
                 ((cy - sd->scroll) != start_y))
               termio_sel_set(sd, EINA_TRUE);
          }

        if (sd->pty->selection.by_line)
          {
             _sel_line_to(sd, cy - sd->scroll, EINA_FALSE);
          }
        else if (sd->pty->selection.by_word)
          {
             _sel_word_to(sd, cx, cy - sd->scroll, EINA_FALSE);
          }
        else
          {
             _sel_to(sd, cx, cy - sd->scroll, EINA_FALSE);
          }

        termio_selection_dbl_fix(sd);
        if (!sd->pty->selection.is_box)
          _selection_newline_extend_fix(sd);
        termio_smart_update_queue(sd);
        sd->moved = EINA_TRUE;
     }
   /* TODO: make the following useless */
   if (sd->mouse_move_job)
     ecore_job_del(sd->mouse_move_job);
#if !defined(ENABLE_TESTS)
   sd->mouse_move_job = ecore_job_add(termio_smart_cb_mouse_move_job, sd);
#endif
}

void
termio_internal_mouse_wheel(Termio *sd,
                           Evas_Event_Mouse_Wheel *ev,
                           Termio_Modifiers modifiers)
{
   char buf[64];

   /* do not handle horizontal scrolling */
   if (ev->direction)
     {
        return;
     }

   if (modifiers.ctrl || modifiers.alt || modifiers.shift)
     {
        return;
     }

   if (sd->pty->mouse_mode == MOUSE_OFF)
     {
        if (sd->pty->altbuf)
          {
             /* Emulate cursors */
             buf[0] = 0x1b;
             buf[1] = 'O';
             buf[2] = (ev->z < 0) ? 'A' : 'B';
             buf[3] = 0;
             termpty_write(sd->pty, buf, strlen(buf));
          }
        else
          {
             sd->scroll -= (ev->z * 4);
             if (sd->scroll < 0)
               sd->scroll = 0;
             termio_smart_update_queue(sd);
             miniview_position_offset(term_miniview_get(sd->term),
                                      ev->z * 4, EINA_TRUE);

             termio_smart_cb_mouse_move_job(sd);
          }
     }
   else
     {
       int cx = 0, cy = 0;

       termio_cursor_to_xy(sd, ev->canvas.x, ev->canvas.y, &cx, &cy);

       switch (sd->pty->mouse_ext)
         {
          case MOUSE_EXT_NONE:
            if ((cx < (0xff - ' ')) && (cy < (0xff - ' ')))
              {
                 int btn = (ev->z >= 0) ? 1 + 64 : 64;

                 buf[0] = 0x1b;
                 buf[1] = '[';
                 buf[2] = 'M';
                 buf[3] = btn + ' ';
                 buf[4] = cx + 1 + ' ';
                 buf[5] = cy + 1 + ' ';
                 buf[6] = 0;
                 termpty_write(sd->pty, buf, strlen(buf));
              }
            break;
          case MOUSE_EXT_UTF8: // ESC.[.M.BTN/FLGS.XUTF8.YUTF8
              {
                 int v, i;
                 int btn = (ev->z >= 0) ? 'a' : '`';

                 buf[0] = 0x1b;
                 buf[1] = '[';
                 buf[2] = 'M';
                 buf[3] = btn;
                 i = 4;
                 v = cx + 1 + ' ';
                 if (v <= 127) buf[i++] = v;
                 else
                   { // 14 bits for cx/cy - enough i think
                       buf[i++] = 0xc0 + (v >> 6);
                       buf[i++] = 0x80 + (v & 0x3f);
                   }
                 v = cy + 1 + ' ';
                 if (v <= 127) buf[i++] = v;
                 else
                   { // 14 bits for cx/cy - enough i think
                       buf[i++] = 0xc0 + (v >> 6);
                       buf[i++] = 0x80 + (v & 0x3f);
                   }
                 buf[i] = 0;
                 termpty_write(sd->pty, buf, strlen(buf));
              }
            break;
          case MOUSE_EXT_SGR: // ESC.[.<.NUM.;.NUM.;.NUM.M
              {
                 int btn = (ev->z >= 0) ? 1 + 64 : 64;
                 snprintf(buf, sizeof(buf), "%c[<%i;%i;%iM", 0x1b,
                          btn, cx + 1, cy + 1);
                 termpty_write(sd->pty, buf, strlen(buf));
              }
            break;
          case MOUSE_EXT_URXVT: // ESC.[.NUM.;.NUM.;.NUM.M
              {
                 int btn = (ev->z >= 0) ? 1 + 64 : 64;
                 snprintf(buf, sizeof(buf), "%c[%i;%i;%iM", 0x1b,
                          btn + ' ',
                          cx + 1, cy + 1);
                 termpty_write(sd->pty, buf, strlen(buf));
              }
            break;
          default:
            break;
         }
     }
}


static void
_termio_scroll_selection(Termio *sd, Termpty *ty,
                         int direction, int start_y, int end_y)
{
   if (!ty->selection.is_active)
     return;

   int sel_start_x = ty->selection.start.x;
   int sel_start_y = ty->selection.start.y;
   int sel_end_x = ty->selection.end.x;
   int sel_end_y = ty->selection.end.y;

   int left_margin = ty->termstate.left_margin;
   int right_margin = ty->termstate.right_margin;

   if (!ty->selection.is_top_to_bottom)
     {
        INT_SWAP(sel_start_y, sel_end_y);
        INT_SWAP(sel_start_x, sel_end_x);
     }

   if (start_y <= sel_start_y &&
       sel_end_y <= end_y)
     {
        if (ty->termstate.left_margin)
          {
             if ((ty->selection.is_box) || (sel_start_y == sel_end_y))
               {
                  /* if selection outside scrolling area */
                  if ((sel_end_x <= left_margin) ||
                      (sel_start_x >= right_margin))
                    {
                       return;
                    }
                  /* if selection not within scrolling area */
                  if (!((sel_start_x >= left_margin) &&
                        (sel_end_x <= right_margin)))
                    {
                       termio_sel_set(sd, EINA_FALSE);
                       return;
                    }
               }
             else
               {
                  termio_sel_set(sd, EINA_FALSE);
                  return;
               }
          }

          ty->selection.orig.y += direction;
          ty->selection.start.y += direction;
          ty->selection.end.y += direction;
          sel_start_y += direction;
          sel_end_y += direction;
          if (!(start_y <= sel_start_y &&
                sel_end_y <= end_y))
            {
               termio_sel_set(sd, EINA_FALSE);
            }
     }
   else if (!((start_y > sel_end_y) ||
              (end_y < sel_start_y)))
     {
        if (ty->termstate.left_margin)
          {
             if ((ty->selection.is_box) || (sel_start_y == sel_end_y))
               {
                  /* if selection outside scrolling area */
                  if ((sel_end_x <= left_margin) ||
                      (sel_start_x >= right_margin))
                    {
                       return;
                    }
               }
          }
        termio_sel_set(sd, EINA_FALSE);
     }
   else if (sd->scroll > 0)
     {
        ty->selection.orig.y += direction;
        ty->selection.start.y += direction;
        ty->selection.end.y += direction;
     }
}


void
termio_scroll(Evas_Object *obj, int direction,
              int start_y, int end_y)
{
   Termio *sd = termio_get_from_obj(obj);
   Termpty *ty;

   EINA_SAFETY_ON_NULL_RETURN(sd);

   ty = sd->pty;

   if ((!sd->jump_on_change) && // if NOT scroll to bottom on updates
       (sd->scroll > 0))
     {
        Evas_Object *mv = term_miniview_get(sd->term);
        if (mv)
          {
             miniview_position_offset(mv, direction, EINA_FALSE);
          }
        // adjust scroll position for added scrollback
        sd->scroll -= direction;
     }

   _termio_scroll_selection(sd, ty, direction, start_y, end_y);
}

void
termio_internal_render(Termio *sd,
                       Evas_Coord ox, Evas_Coord oy,
                       int *preedit_xp, int *preedit_yp)
{
   int x, y, ch1 = 0, ch2 = 0, inv = 0, preedit_x = 0, preedit_y = 0;
   const char *preedit_str;
   ssize_t w;
   int sel_start_x = 0, sel_start_y = 0, sel_end_x = 0, sel_end_y = 0;
   Eina_Unicode *cp;
   Termblock *blk;
   Eina_List *l;

   EINA_LIST_FOREACH(sd->pty->block.active, l, blk)
     {
        blk->was_active = blk->active;
        blk->active = EINA_FALSE;
     }

   inv = sd->pty->termstate.reverse;
   termpty_backlog_lock();
   termpty_backscroll_adjust(sd->pty, &sd->scroll);

   /* Make selection bottom to top */
   sel_start_x = sd->pty->selection.start.x;
   sel_start_y = sd->pty->selection.start.y;
   sel_end_x = sd->pty->selection.end.x;
   sel_end_y = sd->pty->selection.end.y;
   if (!sd->pty->selection.is_top_to_bottom)
     {
        INT_SWAP(sel_start_y, sel_end_y);
        INT_SWAP(sel_start_x, sel_end_x);
     }
   cp = sd->pty->selection.codepoints;

   /* Look at every visible line */
   for (y = 0; y < sd->grid.h; y++)
     {
        Termcell *cells;
        Evas_Textgrid_Cell *tc;
        int cur_sel_start_x = -1, cur_sel_end_x = -1;
        int rel_y = y - sd->scroll;

        w = 0;
        cells = termpty_cellrow_get(sd->pty, rel_y, &w);
        if (!cells)
          continue;
        tc = evas_object_textgrid_cellrow_get(sd->grid.obj, y);
        if (!tc)
          continue;

        /* Compute @cur_sel_start_x, @cur_sel_end_x */
        if (sd->pty->selection.codepoints)
          {
             if (sel_start_y <= rel_y && rel_y <= sel_end_y)
               {
                  if (sd->pty->selection.is_box)
                    {
                       cur_sel_start_x = sel_start_x;
                       cur_sel_end_x = sel_end_x;
                    }
                  else
                    {
                       cur_sel_start_x = sel_start_x;
                       cur_sel_end_x = sel_end_x;
                       if (sel_start_y != sel_end_y)
                         {
                            if (rel_y == sel_start_y)
                              {
                                 cur_sel_end_x = sd->grid.w - 1;
                              }
                            else if (y == sel_end_y)
                              {
                                 cur_sel_start_x = 0;
                              }
                            else
                              {
                                 cur_sel_start_x = 0;
                                 cur_sel_end_x = sd->grid.w - 1;
                              }
                         }
                    }
               }
          }

        ch1 = -1;
        /* Look at every cell in that line */
        for (x = 0; x < sd->grid.w; x++)
          {
             Eina_Unicode *u = NULL;

             if (cp && cur_sel_start_x <= x && x <= cur_sel_end_x)
               u = cp++;

             if ((!cells) || (x >= w))
               {
                  if ((tc[x].codepoint != 0) ||
                      (tc[x].bg != COL_INVIS) ||
                      (tc[x].bg_extended))
                    {
                       if (ch1 < 0)
                         ch1 = x;
                       ch2 = x;
                    }
                  tc[x].codepoint = 0;
                  tc[x].bg = (inv) ? COL_INVERSEBG : COL_INVIS;
                  tc[x].bg_extended = 0;
                  tc[x].underline = 0;
                  tc[x].strikethrough = 0;
                  tc[x].bold = 0;
                  tc[x].italic = 0;
                  tc[x].double_width = 0;

                  if (u && *u != ' ')
                    {
                       termio_sel_set(sd, EINA_FALSE);
                       u = cp = NULL;
                    }
               }
             else
               {
                  int bid, bx = 0, by = 0;

                  bid = termpty_block_id_get(&(cells[x]), &bx, &by);
                  if (bid >= 0)
                    {
                       if (ch1 < 0)
                         ch1 = x;
                       ch2 = x;
                       tc[x].codepoint = 0;
                       tc[x].fg_extended = 0;
                       tc[x].bg_extended = 0;
                       tc[x].underline = 0;
                       tc[x].strikethrough = 0;
                       tc[x].bold = 0;
                       tc[x].italic = 0;
                       tc[x].double_width = 0;
                       tc[x].fg = COL_INVIS;
                       tc[x].bg = COL_INVIS;
                       blk = termpty_block_get(sd->pty, bid);
                       if (blk)
                         {
                            termio_block_activate(sd->self, blk);
                            blk->x = (x - bx);
                            blk->y = (y - by);
                            evas_object_move(blk->obj,
                                             ox + (blk->x * sd->font.chw),
                                             oy + (blk->y * sd->font.chh));
                            evas_object_resize(blk->obj,
                                               blk->w * sd->font.chw,
                                               blk->h * sd->font.chh);
                         }
                       if (u && *u != ' ')
                         {
                            termio_sel_set(sd, EINA_FALSE);
                            u = cp = NULL;
                         }
                    }
                  else if (cells[x].att.invisible)
                    {
                       if ((tc[x].codepoint != 0) ||
                           (tc[x].bg != COL_INVIS) ||
                           (tc[x].bg_extended))
                         {
                            if (ch1 < 0)
                              ch1 = x;
                            ch2 = x;
                         }
                       tc[x].codepoint = 0;
                       tc[x].bg = (inv) ? COL_INVERSEBG : COL_INVIS;
                       tc[x].bg_extended = 0;
                       tc[x].underline = 0;
                       tc[x].strikethrough = 0;
                       tc[x].bold = 0;
                       tc[x].italic = 0;
                       tc[x].double_width = cells[x].att.dblwidth;
                       if ((tc[x].double_width) && (tc[x].codepoint == 0) &&
                           (ch2 == x - 1))
                         ch2 = x;
                       if (u && *u != ' ')
                         {
                            termio_sel_set(sd, EINA_FALSE);
                            u = cp = NULL;
                         }
                    }
                  else
                    {
                       int fg, bg, fgext, bgext, bold, italic;
                       Eina_Unicode codepoint;

                       // colors
                       fg = cells[x].att.fg;
                       bg = cells[x].att.bg;
                       fgext = cells[x].att.fg256;
                       bgext = cells[x].att.bg256;
                       codepoint = cells[x].codepoint;
                       if (sd->config->font.bolditalic)
                         {
                            bold = cells[x].att.bold;
                            italic = cells[x].att.italic;
                         }
                       else
                         {
                            bold = 0;
                            italic = 0;
                         }

                       if ((fg == COL_DEF) && (cells[x].att.inverse ^ inv))
                         fg = COL_INVERSEBG;
                       if (bg == COL_DEF)
                         {
                            if (cells[x].att.inverse ^ inv)
                              bg = COL_INVERSE;
                            else if (!bgext)
                              bg = COL_INVIS;
                         }
                       if ((cells[x].att.fgintense) && (!fgext))
                         fg += 48;
                       if ((cells[x].att.bgintense) && (!bgext))
                         bg += 48;
                       if ((cells[x].att.bold) && (!fgext))
                         fg += 12;
                       if ((cells[x].att.faint) && (!fgext))
                         fg += 24;
                       if (cells[x].att.inverse ^ inv)
                         {
                            int t;
                            t = fgext; fgext = bgext; bgext = t;
                            t = fg; fg = bg; bg = t;
                         }
                       if ((tc[x].codepoint != codepoint) ||
                           (tc[x].bold != bold) ||
                           (tc[x].italic != italic) ||
                           (tc[x].fg != fg) ||
                           (tc[x].bg != bg) ||
                           (tc[x].fg_extended != fgext) ||
                           (tc[x].bg_extended != bgext) ||
                           (tc[x].underline != cells[x].att.underline) ||
                           (tc[x].strikethrough != cells[x].att.strike))
                         {
                            if (ch1 < 0)
                              ch1 = x;
                            ch2 = x;
                         }
                       tc[x].fg_extended = fgext;
                       tc[x].bg_extended = bgext;
                       tc[x].underline = cells[x].att.underline;
                       tc[x].strikethrough = cells[x].att.strike;
                       if (sd->config->font.bolditalic)
                         {
                            tc[x].bold = cells[x].att.bold;
                            tc[x].italic = cells[x].att.italic;
                         }
                       else
                         {
                            tc[x].bold = 0;
                            tc[x].italic = 0;
                         }
                       tc[x].double_width = cells[x].att.dblwidth;
                       tc[x].fg = fg;
                       tc[x].bg = bg;
                       tc[x].codepoint = codepoint;
                       if ((tc[x].double_width) && (tc[x].codepoint == 0) &&
                           (ch2 == x - 1))
                         ch2 = x;
                       // cells[x].att.blink
                       // cells[x].att.blink2
                       if (u && (*u != codepoint) && (*u != ' ') &&
                           (codepoint != 0))
                         {
                            termio_sel_set(sd, EINA_FALSE);
                            u = cp = NULL;
                         }
                    }
               }
          }
        evas_object_textgrid_cellrow_set(sd->grid.obj, y, tc);
        /* only bothering to keep 1 change span per row - not worth doing
         * more really */
        if (ch1 >= 0)
          evas_object_textgrid_update_add(sd->grid.obj, ch1, y,
                                          ch2 - ch1 + 1, 1);
     }

   preedit_str = term_preedit_str_get(sd->term);
   if (preedit_str && preedit_str[0])
     {
        Eina_Unicode *uni, g;
        int len = 0, i, jump, xx, backx;
        Eina_Bool dbl;
        Evas_Textgrid_Cell *tc;
        x = sd->cursor.x, y = sd->cursor.y;

        uni = eina_unicode_utf8_to_unicode(preedit_str, &len);
        if (uni)
          {
             for (i = 0; i < len; i++)
               {
                  jump = 1;
                  g = uni[i];
                  dbl = _termpty_is_dblwidth_get(sd->pty, g);
                  if (dbl) jump = 2;
                  backx = 0;
                  if ((x + jump) > sd->grid.w)
                    {
                       if (y < (sd->grid.h - 1))
                         {
                            x = jump;
                            backx = jump;
                            y++;
                         }
                    }
                  else
                    {
                       x += jump;
                       backx = jump;
                    }
                  tc = evas_object_textgrid_cellrow_get(sd->grid.obj, y);
                  xx = x - backx;
                  tc[xx].bold = 1;
                  tc[xx].bg = COL_BLACK;
                  tc[xx].fg = COL_WHITE;
                  tc[xx].fg_extended = 0;
                  tc[xx].bg_extended = 0;
                  tc[xx].underline = 1;
                  tc[xx].strikethrough = 0;
                  tc[xx].double_width = dbl;
                  tc[xx].codepoint = g;
                  if (dbl)
                    {
                       xx = x - backx + 1;
                       tc[xx].bold = 1;
                       tc[xx].bg = COL_BLACK;
                       tc[xx].fg = COL_WHITE;
                       tc[xx].fg_extended = 0;
                       tc[xx].bg_extended = 0;
                       tc[xx].underline = 1;
                       tc[xx].strikethrough = 0;
                       tc[xx].double_width = 0;
                       tc[xx].codepoint = 0;
                    }
                  evas_object_textgrid_cellrow_set(sd->grid.obj, y, tc);
                  if (x >= sd->grid.w)
                    {
                       if (y < (sd->grid.h - 1))
                         {
                            x = 0;
                            y++;
                         }
                    }
               }
             evas_object_textgrid_update_add(sd->grid.obj, 0, sd->cursor.y,
                                             sd->grid.w, y - sd->cursor.y + 1);
          }
        preedit_x = x - sd->cursor.x;
        preedit_y = y - sd->cursor.y;
     }
   termpty_backlog_unlock();
   *preedit_xp = preedit_x;
   *preedit_yp = preedit_y;
}
