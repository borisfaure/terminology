#include "private.h"

#include <Elementary.h>
#include "termpty.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>

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
             cells[i].glyph = val;
             cells[i].att = ty->state.att;
          }
     }
   else
     {
        for (i = 0; i < count; i++)
          {
             cells[i].glyph = val;
             cells[i].att = clear;
          }
     }
}

static void
_text_copy(Termpty *ty, Termcell *cells, Termcell *dest, int count)
{
   memcpy(dest, cells, sizeof(*(cells)) * count);
}

static void
_text_save_top(Termpty *ty)
{
   Termsave *ts;

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
   int y, y1 = 0, y2 = ty->h - 1;

   if (ty->state.scroll_y2 != 0)
     {
        y1 = ty->state.scroll_y1;
        y2 = ty->state.scroll_y2 - 1;
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
   DBG("... scroll!!!!! [%i->%i]\n", y1, y2);
   cells2 = &(ty->screen[y2 * ty->w]);
   for (y = y1; y < y2; y++)
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
   int y, y1 = 0, y2 = ty->h - 1;

   if (ty->state.scroll_y2 != 0)
     {
        y1 = ty->state.scroll_y1;
        y2 = ty->state.scroll_y2 - 1;
     }
   DBG("... scroll rev!!!!! [%i->%i]\n", y1, y2);
   cells = &(ty->screen[y2 * ty->w]);
   for (y = y2; y > y1; y--)
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

static void
_text_append(Termpty *ty, const int *glyphs, int len)
{
   Termcell *cells;
   int i, j;

   cells = &(ty->screen[ty->state.cy * ty->w]);
   for (i = 0; i < len; i++)
     {
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
        cells[ty->state.cx].glyph = glyphs[i];
        cells[ty->state.cx].att = ty->state.att;
        if (ty->state.wrap)
          {
             ty->state.wrapnext = 0;
             if (ty->state.cx >= (ty->w - 1)) ty->state.wrapnext = 1;
             else ty->state.cx++;
          }
        else
          {
             ty->state.wrapnext = 0;
             ty->state.cx++;
             if (ty->state.cx >= ty->w)
               ty->state.cx = ty->w - 1;
          }
     }
}

static void
_term_write(Termpty *ty, const char *txt, int size)
{
   if (write(ty->fd, txt, size) < 0) perror("write");
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
   att->italic = 0;
   att->underline = 0;
   att->blink = 0;
   att->blink2 = 0;
   att->inverse = 0;
   att->invisible = 0;
   att->strike = 0;
   att->fg256 = 0;
   att->bg256 = 0;
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
_csi_arg_get(char **ptr)
{
   char *b = *ptr;
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
_handle_esc_csi(Termpty *ty, const int *c, int *ce)
{
   int *cc, arg, first = 1, i;
   char buf[4096], *b;

   cc = (int *)c;
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
   DBG(" CSI: '%c' args '%s'\n", *cc, buf);
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
                       ty->state.att.italic = 1;
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
                       ty->state.att.italic = 0;
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
                       break;
                     case 38: // xterm 256 fg color ???
                       // now check if next arg is 5
                       arg = _csi_arg_get(&b);
                       if (arg != 5) ERR("Failed xterm 256 color fg esc 5\n");
                       else
                         {
                            // then get next arg - should be color index 0-255
                            arg = _csi_arg_get(&b);
                            if (!b) ERR("Failed xterm 256 color fg esc val\n");
                            else
                              {
                                 ty->state.att.fg256 = 1;
                                 ty->state.att.fg = arg;
                              }
                         }
                       break;
                     case 39: // default fg color
                       ty->state.att.fg256 = 0;
                       ty->state.att.fg = COL_DEF;
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
                       break;
                     case 48: // xterm 256 bg color ???
                       // now check if next arg is 5
                       arg = _csi_arg_get(&b);
                       if (arg != 5) ERR("Failed xterm 256 color bg esc 5\n");
                       else
                         {
                            // then get next arg - should be color index 0-255
                            arg = _csi_arg_get(&b);
                            if (!b) ERR("Failed xterm 256 color bg esc val\n");
                            else
                              {
                                 ty->state.att.bg256 = 1;
                                 ty->state.att.bg = arg;
                              }
                         }
                       break;
                     case 49: // default bg color
                       ty->state.att.bg256 = 0;
                       ty->state.att.bg = COL_DEF;
                       break;
                     default: //  not handled???
                       ERR("  color cmd [%i] not handled\n", arg);
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

             ty->state.wrapnext = 0;
             ty->state.insert = 1;
             for (i = 0; i < arg; i++)
               _text_append(ty, blank, 1);
             ty->state.insert = pi;
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
               ERR("invalid clr scr %i\n", arg);
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
               ERR("invalid clr lin %i\n", arg);
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
                            int i, size;

                            switch (arg)
                              {
                               case 1:
                                 handled = 1;
                                 ty->state.appcursor = mode;
                                 break;
                               case 5:
                                 handled = 1;
                                 break;
                               case 7:
                                 handled = 1;
                                 DBG("DDD: set wrap mode to %i\n", mode);
                                 ty->state.wrap = mode;
                                 break;
                               case 20:
                                 ty->state.crlf = mode;
                                 break;
                               case 12:
                                 handled = 1;
//                                 DBG("XXX: set blinking cursor to (stop?) %i\n", mode);
                                 break;
                               case 25:
                                 handled = 1;
                                 ty->state.hidecursor = !mode;
                                 break;
                               case 1000:
                                 handled = 1;
                                 INF("XXX: set x11 mouse reporting to %i\n", mode);
                                 break;
                               case 1049:
                               case 47:
                               case 1047:
                                 handled = 1;
                                 DBG("DDD: switch buf\n");
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
                               default:
                                 ERR("unhandled screen mode arg %i\n", arg);
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
                                 DBG("DDD: set insert mode to %i\n", mode);
                                 ty->state.insert = mode;
                              }
//                            else if (arg == 24)
//                              {
//                                 ERR("unhandled #24 arg %i\n", arg);
//                                  // ???
//                              }
                            else
                              ERR("unhandled screen non-priv mode arg %i, mode %i, ch '%c'\n", arg, mode, *cc);
                         }
                    }
               }
             if (!handled) ERR("unhandled '%c' : '%s'\n", *cc, buf);
          }
        break;
      case 'r':
        arg = _csi_arg_get(&b);
        if (!b)
          {
             INF("no region args reset region\n");
             ty->state.scroll_y1 = 0;
             ty->state.scroll_y2 = 0;
          }
        else
          {
             int arg2;

             arg2 = _csi_arg_get(&b);
             if (!b)
               {
                  INF("failed to give 2 region args reset region\n");
                  ty->state.scroll_y1 = 0;
                  ty->state.scroll_y2 = 0;
               }
             else
               {
                  if (arg >= arg2)
                    {
                       ERR("scroll region beginning >= end [%i %i]\n", arg, arg2);
                       ty->state.scroll_y1 = 0;
                       ty->state.scroll_y2 = 0;
                    }
                  else
                    {
                       INF("2 region args: %i %i\n", arg, arg2);
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
        ERR("unhandled CSI '%c' (0x%02x), buf='%s'\n", *cc, *cc, buf);
        break;
     }
   cc++;
   return cc - c;
}

static int
_handle_esc_xterm(Termpty *ty, const int *c, int *ce)
{
   int *cc;
   char buf[4096], *b;

   cc = (int *)c;
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
        b = &(buf[2]);
        if (ty->prop.title) eina_stringshare_del(ty->prop.title);
        if (ty->prop.icon) eina_stringshare_del(ty->prop.icon);
        ty->prop.title = eina_stringshare_add(b);
        ty->prop.icon = eina_stringshare_add(b);
        if (ty->cb.set_title.func) ty->cb.set_title.func(ty->cb.set_title.data);
        if (ty->cb.set_icon.func) ty->cb.set_title.func(ty->cb.set_icon.data);
        break;
      case '1':
        // XXX: icon name - callback
        b = &(buf[2]);
        if (ty->prop.icon) eina_stringshare_del(ty->prop.icon);
        ty->prop.icon = eina_stringshare_add(b);
        if (ty->cb.set_icon.func) ty->cb.set_title.func(ty->cb.set_icon.data);
        break;
      case '2':
        // XXX: title - callback
        b = &(buf[2]);
        if (ty->prop.title) eina_stringshare_del(ty->prop.title);
        ty->prop.title = eina_stringshare_add(b);
        if (ty->cb.set_title.func) ty->cb.set_title.func(ty->cb.set_title.data);
        break;
      case '4':
        // XXX: set palette entry. not supported.
        b = &(buf[2]);
        break;
      default:
        // many others
        ERR("unhandled xterm esc '%c' -> '%s'\n", buf[0], buf);
        break;
     }
    return cc - c;
}

static int
_handle_esc(Termpty *ty, const int *c, int *ce)
{
   if ((ce - c) < 2) return 0;
   DBG("ESC: '%c'\n", c[1]);
   switch (c[1])
     {
      case '[':
        return 2 + _handle_esc_csi(ty, c + 2, ce);
      case ']':
        return 2 + _handle_esc_xterm(ty, c + 2, ce);
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
        DBG("reset to init mode and clear\n");
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

             DBG("reset to init mode and clear then fill with E\n");
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
                  for (i = 0; i < size; i++) cells[i].glyph = 'E';
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
        ERR("eek - esc unhandled '%c' (0x%02x)\n", c[1], c[1]);
        break;
     }
   return 1;
}

static int
_handle_seq(Termpty *ty, const int *c, int *ce)
{
   int *cc, len = 0;

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
             INF("BEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEP\n");
             ty->state.had_cr = 0;
             return 1;
           case 0x08: // BS  '\b' (backspace)
             DBG("->BS\n");
             ty->state.wrapnext = 0;
             ty->state.cx--;
             if (ty->state.cx < 0) ty->state.cx = 0;
             ty->state.had_cr = 0;
             return 1;
           case 0x09: // HT  '\t' (horizontal tab)
             DBG("->HT\n");
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
             DBG("->LF\n");
             if (ty->state.had_cr)
               ty->screen[ty->state.had_cr_x + (ty->state.had_cr_y * ty->w)].att.newline = 1;
             ty->state.wrapnext = 0;
             if (ty->state.crlf) ty->state.cx = 0;
             ty->state.cy++;
             _text_scroll_test(ty);
             ty->state.had_cr = 0;
             return 1;
           case 0x0d: // CR  '\r' (carriage ret)
             DBG("->CR\n");
             if (ty->state.cx != 0)
               {
                  ty->state.had_cr_x = ty->state.cx;
                  ty->state.had_cr_y = ty->state.cy;
               }
             ty->state.wrapnext = 0;
             ty->state.cx = 0;
             ty->state.had_cr = 1;
             return 1;
/*
           case 0x0e: // SO  (shift out) // Maps G1 character set into GL.
             return 1;
           case 0x0f: // SI  (shift in) // Maps G0 character set into GL.
             return 1;
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
             ERR("unhandled char 0x%02x\n", c[0]);
             ty->state.had_cr = 0;
             return 1;
          }
     }
   else if (c[0] == 0xf7) // DEL
     {
        ERR("unhandled char 0x%02x [DEL]\n", c[0]);
        ty->state.had_cr = 0;
        return 1;
     }

   cc = (int *)c;
   DBG("txt: [");
   while ((cc < ce) && (*cc >= 0x20) && (*cc != 0xf7))
     {
        DBG("%c", *cc);
        cc++;
        len++;
     }
   DBG("]\n");
   _text_append(ty, c, len);
   ty->state.had_cr = 0;
   return len;
}

static void
_handle_buf(Termpty *ty, const int *glyphs, int len)
{
   int *c, *ce, n, *b, bytes;

   c = (int *)glyphs;
   ce = &(c[len]);

   if (ty->buf)
     {
        bytes = (ty->buflen + len + 1) * sizeof(int);
        b = realloc(ty->buf, bytes);
        if (!b)
          {
             ERR("memerr\n");
          }
        INF("realloc add %i + %i\n", (int)(ty->buflen * sizeof(int)), (int)(len * sizeof(int)));
        bytes = len * sizeof(int);
        memcpy(&(b[ty->buflen]), glyphs, bytes);
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
                  int *tmp = ty->buf;
                  ty->buf = NULL;
                  ty->buflen = 0;
                  bytes = ((char *)ce - (char *)c) + sizeof(int);
                  INF("malloc til %i\n", (int)(bytes - sizeof(int)));
                  ty->buf = malloc(bytes);
                  if (!ty->buf)
                    {
                       ERR("memerr\n");
                    }
                  bytes = (char *)ce - (char *)c;
                  memcpy(ty->buf, c, bytes);
                  ty->buflen = bytes / sizeof(int);
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
                  bytes = ((char *)ce - (char *)c) + sizeof(int);
                  ty->buf = malloc(bytes);
                  INF("malloc %i\n", (int)(bytes - sizeof(int)));
                  if (!ty->buf)
                    {
                       ERR("memerr\n");
                    }
                  bytes = (char *)ce - (char *)c;
                  memcpy(ty->buf, c, bytes);
                  ty->buflen = bytes / sizeof(int);
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
   if (ioctl(ty->fd, TIOCSWINSZ, &sz) < 0) perror("Size set ioctl failed\n");
}

static Eina_Bool
_cb_exe_exit(void *data, int type, void *event)
{
   Ecore_Exe_Event_Del *ev = event;
   Termpty *ty = data;

   if (ev->pid != ty->pid) return ECORE_CALLBACK_PASS_ON;
   // XXX: report via cb
   exit(ev->exit_code);
   return ECORE_CALLBACK_PASS_ON;
}

static Eina_Bool
_cb_fd_read(void *data, Ecore_Fd_Handler *fd_handler)
{
   Termpty *ty = data;
   char buf[4097];
   int glyph[4097];
   int len, i, j, reads;

   // read up to 64 * 4096 bytes
   for (reads = 0; reads < 64; reads++)
     {
        len = read(ty->fd, buf, sizeof(buf) - 1);
        if (len <= 0) break;
        buf[len] = 0;
        // convert UTF8 to glyph integers
        j = 0;
        for (i = 0; i < len;)
          {
             int g = 0;

             if (buf[i])
               {
                  i = evas_string_char_next_get(buf, i, &g);
                  if (i < 0) break;
//                  DBG("(%i) %02x '%c'\n", j, g, g);
               }
             else
               {
                  ERR("ZERO GLYPH!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
                  g = 0;
                  i++;
               }
             glyph[j] = g;
             j++;
          }
        glyph[j] = 0;
//        DBG("---------------- handle buf %i\n", j);
        _handle_buf(ty, glyph, j);
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
        char **args, *shell;
        struct passwd *pw;
        uid_t uid;
        int i;

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

        uid = getuid();
        pw = getpwuid(uid);
        if (!pw) shell = "/bin/sh";
        shell = pw->pw_shell;
        if (!shell) shell = "/bin/sh";
        if (!cmd) cmd = shell;
        args = malloc(2 * sizeof(char *));
        args[0] = (char *)cmd;
        args[1] = NULL;
        // pretend to be xterm
        putenv("TERM=xterm");
        execvp(args[0], args);
        exit(1);
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
   if (write(ty->fd, input, len) < 0) perror("write");
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
   Termsave *tso;
   
   if (ty->backmax == size) return;

   if (ty->back)
     {
        for (i = 0; i < ty->backmax; i++)
          {
             if (ty->back[i]) free(ty->back[i]);
          }
        free(ty->back);
     }
   ty->back = calloc(1, sizeof(Termsave *) * ty->backmax);
   ty->backscroll_num = 0;
   ty->backpos = 0;
   ty->backmax = size;
}
