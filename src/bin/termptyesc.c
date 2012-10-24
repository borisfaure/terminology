#include "private.h"
#include <Elementary.h>
#include "termpty.h"
#include "termptydbl.h"
#include "termptyesc.h"
#include "termptyops.h"
#include "termptyext.h"

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

#define ST 0x9c // String Terminator
#define BEL 0x07 // Bell
#define ESC 033 // Escape

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
               _termpty_reset_att(&(ty->state.att));
             else if (b)
               {
                  first = 0;
                  switch (arg)
                    {
                     case 0: // reset to normal
                       _termpty_reset_att(&(ty->state.att));
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
               _termpty_text_append(ty, blank, 1);
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
             _termpty_text_scroll_rev_test(ty);
          }
        break;
      case 'B': // cursor down N
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        ty->state.wrapnext = 0;
        for (i = 0; i < arg; i++)
          {
             ty->state.cy++;
             _termpty_text_scroll_test(ty);
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
        _termpty_clear_line(ty, TERMPTY_CLR_END, arg);
        break;
      case 'S': // scroll up N lines
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        for (i = 0; i < arg; i++) _termpty_text_scroll(ty);
        break;
      case 'T': // scroll down N lines
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        for (i = 0; i < arg; i++) _termpty_text_scroll_rev(ty);
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
                  if (*cc == 'M') _termpty_text_scroll(ty);
                  else _termpty_text_scroll_rev(ty);
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
//              0 → VT100
//              1 → VT220
//              2 → VT240
//             18 → VT330
//             19 → VT340
//             24 → VT320
//             41 → VT420
//             61 → VT510
//             64 → VT520
//             65 → VT525
             snprintf(bf, sizeof(bf), "\033[>0;271;%ic", 0);
             termpty_write(ty, bf, strlen(bf));
          }
        break;
      case 'J': // "2j" erases the screen, 1j erase from screen start to curs, 0j erase cursor to end of screen
        arg = _csi_arg_get(&b);
        if (b)
          {
             if ((arg >= TERMPTY_CLR_END) && (arg <= TERMPTY_CLR_ALL))
               _termpty_clear_screen(ty, arg);
             else
               ERR("invalid clr scr %i", arg);
          }
        else _termpty_clear_screen(ty, TERMPTY_CLR_END);
        break;
      case 'K': // 0K erase to end of line, 1K erase from screen start to cursor, 2K erase all of line
        arg = _csi_arg_get(&b);
        if (b)
          {
             if ((arg >= TERMPTY_CLR_END) && (arg <= TERMPTY_CLR_ALL))
               _termpty_clear_line(ty, arg, ty->w);
             else
               ERR("invalid clr lin %i", arg);
          }
        else _termpty_clear_line(ty, TERMPTY_CLR_END, ty->w);
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
                                 INF("XXX: set mouse (X10) %i", mode);
                                 if (mode) ty->mouse_mode = MOUSE_X10;
                                 else ty->mouse_mode = MOUSE_OFF;
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
                                 if (mode) ty->mouse_mode = MOUSE_NORMAL;
                                 else ty->mouse_mode = MOUSE_OFF;
                                 INF("XXX: set mouse (press+release only) to %i", mode);
                                 break;
                               case 1001:
                                 handled = 1;
                                 ERR("XXX: x11 mouse highlighting %i", mode);
                                 break;
                               case 1002:
                                 handled = 1;
                                 if (mode) ty->mouse_mode = MOUSE_NORMAL_BTN_MOVE;
                                 else ty->mouse_mode = MOUSE_OFF;
                                 INF("XXX: set mouse (press+release+motion while pressed) %i", mode);
                                 break;
                               case 1003:
                                 handled = 1;
                                 if (mode) ty->mouse_mode = MOUSE_NORMAL_ALL_MOVE;
                                 else ty->mouse_mode = MOUSE_OFF;
                                 ERR("XXX: set mouse (press+release+all motion) %i", mode);
                                 break;
                               case 1004: // i dont know what focus repporting is?
                                 handled = 1;
                                 ERR("XXX: enable focus reporting %i", mode);
                                 break;
                               case 1005:
                                 handled = 1;
                                 if (mode) ty->mouse_ext = MOUSE_EXT_UTF8;
                                 else ty->mouse_ext = MOUSE_EXT_NONE;
                                 INF("XXX: set mouse (xterm utf8 style) %i", mode);
                                 break;
                               case 1006:
                                 handled = 1;
                                 if (mode) ty->mouse_ext = MOUSE_EXT_SGR;
                                 else ty->mouse_ext = MOUSE_EXT_NONE;
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
                                 if (mode) ty->mouse_ext = MOUSE_EXT_URXVT;
                                 else ty->mouse_ext = MOUSE_EXT_NONE;
                                 INF("XXX: set mouse (rxvt-unicode style) %i", mode);
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
                                      _termpty_clear_all(ty);
//                                      _termpty_cursor_copy(&(ty->swap), &(ty->state));
                                      ty->state = ty->swap;
                                   }
                                 else
                                   {
//                                      _termpty_cursor_copy(&(ty->state), &(ty->swap));
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
                                   _termpty_cursor_copy(&(ty->state), &(ty->save));
                                 else
                                   _termpty_cursor_copy(&(ty->save), &(ty->state));
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
        _termpty_cursor_copy(&(ty->state), &(ty->save));
        break;
      case 'u': // restore cursor pos
        _termpty_cursor_copy(&(ty->save), &(ty->state));
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
       case 'Z': // Cursor Back Tab
       {
          int idx, size, cx = ty->state.cx, cy = ty->state.cy;

          arg = _csi_arg_get(&b);
          if (arg < 1) arg = 1;

          size = ty->w * cy + cx + 1;
          for (idx = size - 1; idx >= 0; idx--)
            {
               if (ty->screen[cx + (cy * ty->w)].att.tab) arg--;
               cx--;
               if (cx < 0)
                 {
                    cx = ty->w - 1;
                    cy--;
                 }
               if (!arg) break;
            }
          if (!arg)
            {
               ty->state.cx = cx;
               ty->state.cy = cy;
            }
       }
       break;
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
   const Eina_Unicode *cc;
   Eina_Unicode buf[4096], *b;
   char *s;
   int len = 0;
   
   cc = c;
   b = buf;
   while ((cc < ce) && (*cc != ST) && (*cc != BEL))
     {
        if ((cc < ce - 1) && (*cc == ESC) && (*(cc + 1) == '\\'))
          {
             cc++;
             break;
          }
        *b = *cc;
        b++;
        cc++;
     }
   *b = 0;
   if ((*cc == ST) || (*cc == BEL) || (*cc == '\\')) cc++;
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
   if (!_termpty_ext_handle(ty, s, buf))
     {
        if (ty->cb.command.func) ty->cb.command.func(ty->cb.command.data);
     }
   ty->cur_cmd = NULL;
   if (s) free(s);
   return cc - c;
}

static int
_handle_esc_dcs(Termpty *ty __UNUSED__, const Eina_Unicode *c, Eina_Unicode *ce)
{
   const Eina_Unicode *cc;
   Eina_Unicode buf[4096], *b;
 
   cc = c;
   b = buf;
   while ((cc < ce) && (*cc != ST))
     {
        if ((cc < ce - 1) && (*cc == ESC) && (*(cc + 1) == '\\'))
          {
             cc++;
             break;
          }
        *b = *cc;
        b++;
        cc++;
     }
   *b = 0;
   if ((*cc == ST) || (*cc == '\\')) cc++;
   else return -2;
   switch (buf[0])
     {
      case '+':
         /* TODO: Set request termcap/terminfo */
         break;
      default:
        // many others
        ERR("unhandled dcs esc '%c'", buf[0]);
        break;
     }
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
      case 'P':
        return 2 + _handle_esc_dcs(ty, c + 2, ce);
      case '=': // set alternate keypad mode
        ty->state.alt_kp = 1;
        return 2;
      case '>': // set numeric keypad mode
        ty->state.alt_kp = 0;
        return 2;
      case 'M': // move to prev line
        ty->state.wrapnext = 0;
        ty->state.cy--;
        _termpty_text_scroll_rev_test(ty);
        return 2;
      case 'D': // move to next line
        ty->state.wrapnext = 0;
        ty->state.cy++;
        _termpty_text_scroll_test(ty);
        return 2;
      case 'E': // add \n\r
        ty->state.wrapnext = 0;
        ty->state.cx = 0;
        ty->state.cy++;
        _termpty_text_scroll_test(ty);
        return 2;
      case 'Z': // same a 'ESC [ Pn c'
        _term_txt_write(ty, "\033[?1;2C");
        return 2;
      case 'c': // reset terminal to initial state
        DBG("reset to init mode and clear");
        _termpty_reset_state(ty);
        _termpty_clear_screen(ty, TERMPTY_CLR_ALL);
        if (ty->cb.cancel_sel.func)
          ty->cb.cancel_sel.func(ty->cb.cancel_sel.data);
        return 2;
      case '(': // charset 0
        ty->state.chset[0] = c[2];
        ty->state.multibyte = 0;
        ty->state.charsetch = c[2];
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
             _termpty_reset_state(ty);
             ty->save = ty->state;
             ty->swap = ty->state;
             _termpty_clear_screen(ty, TERMPTY_CLR_ALL);
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
        _termpty_cursor_copy(&(ty->state), &(ty->save));
        return 2;
      case '8': // restore cursor pos
        _termpty_cursor_copy(&(ty->save), &(ty->state));
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

int
_termpty_handle_seq(Termpty *ty, Eina_Unicode *c, Eina_Unicode *ce)
{
   Eina_Unicode *cc;
   int len = 0;

/*   
   printf(" B: ");
   int j;
   for (j = 0; c + j < ce && j < 100; j++)
     {
        if ((c[j] < ' ') || (c[j] >= 0x7f))
          printf("\033[35m%08x\033[0m", c[j]);
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
             _termpty_text_scroll_test(ty);
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
   _termpty_text_append(ty, c, len);
   ty->state.had_cr = 0;
   return len;
}
