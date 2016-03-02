#include "private.h"
#include <Elementary.h>
#include <stdint.h>
#include "termio.h"
#include "termpty.h"
#include "termptydbl.h"
#include "termptyesc.h"
#include "termptyops.h"
#include "termptyext.h"
#if defined(SUPPORT_80_132_COLUMNS)
#include "termio.h"
#endif

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
#define DEL 127

/* XXX: all handle_ functions return the number of bytes successfully read, 0
 * if not enough bytes could be read
 */

static const char *ASCII_CHARS_TABLE[] =
{
   "NUL", // '\0'
   "SOH", // '\001'
   "STX", // '\002'
   "ETX", // '\003'
   "EOT", // '\004'
   "ENQ", // '\005'
   "ACK", // '\006'
   "BEL", // '\007'
   "BS",  // '\010'
   "HT",  // '\011'
   "LF",  // '\012'
   "VT",  // '\013'
   "FF",  // '\014'
   "CR" , // '\015'
   "SO",  // '\016'
   "SI",  // '\017'
   "DLE", // '\020'
   "DC1", // '\021'
   "DC2", // '\022'
   "DC3", // '\023'
   "DC4", // '\024'
   "NAK", // '\025'
   "SYN", // '\026'
   "ETB", // '\027'
   "CAN", // '\030'
   "EM",  // '\031'
   "SUB", // '\032'
   "ESC", // '\033'
   "FS",  // '\034'
   "GS",  // '\035'
   "RS",  // '\036'
   "US"   // '\037'
};

static const char *
_safechar(unsigned int c)
{
   static char _str[9];

   // This should avoid 'BEL' and 'ESC' in particular, which would
   // have side effects in the parent terminal (esp. ESC).
   if (c < (sizeof(ASCII_CHARS_TABLE) / sizeof(ASCII_CHARS_TABLE[0])))
     return ASCII_CHARS_TABLE[c];

   if (c == DEL)
     return "DEL";

   // The rest should be safe (?)
   snprintf(_str, 9, "%c", c);
   _str[8] = '\0';
   return _str;
}

static int
_csi_arg_get(Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int sum = 0;

   while ((*b) && (*b < '0' || *b > '9')) b++;
   if (!*b)
     {
        *ptr = NULL;
        return 0;
     }
   while ((*b >= '0') && (*b <= '9'))
     {
        if (sum > INT32_MAX/10 )
          {
             *ptr = NULL;
             return 0;
          }
        sum *= 10;
        sum += *b - '0';
        b++;
     }
   *ptr = b;
   return sum;
}

static void
_handle_cursor_control(Termpty *ty, const Eina_Unicode *cc)
{
   switch (*cc)
     {
      case 0x07: // BEL '\a' (bell)
         ty->termstate.had_cr = 0;
         if (ty->cb.bell.func) ty->cb.bell.func(ty->cb.bell.data);
         return;
      case 0x08: // BS  '\b' (backspace)
         DBG("->BS");
         ty->termstate.had_cr = 0;
         ty->termstate.wrapnext = 0;
         ty->cursor_state.cx--;
         TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
         return;
      case 0x09: // HT  '\t' (horizontal tab)
         DBG("->HT");
         ty->termstate.had_cr = 0;
         TERMPTY_SCREEN(ty, ty->cursor_state.cx, ty->cursor_state.cy).att.tab = 1;
         ty->termstate.wrapnext = 0;
         ty->cursor_state.cx += 8;
         ty->cursor_state.cx = (ty->cursor_state.cx / 8) * 8;
         TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
         return;
      case 0x0a: // LF  '\n' (new line)
      case 0x0b: // VT  '\v' (vertical tab)
      case 0x0c: // FF  '\f' (form feed)
         DBG("->LF");
         if (ty->termstate.had_cr)
           {
              TERMPTY_SCREEN(ty, ty->termstate.had_cr_x,
                                 ty->termstate.had_cr_y).att.newline = 1;
           }
         ty->termstate.had_cr = 0;
         ty->termstate.wrapnext = 0;
         if (ty->termstate.crlf) ty->cursor_state.cx = 0;
         ty->cursor_state.cy++;
         termpty_text_scroll_test(ty, EINA_TRUE);
         return;
      case 0x0d: // CR  '\r' (carriage ret)
         DBG("->CR");
         if (ty->cursor_state.cx != 0)
           {
              ty->termstate.had_cr_x = ty->cursor_state.cx;
              ty->termstate.had_cr_y = ty->cursor_state.cy;
              ty->termstate.wrapnext = 0;
           }
         ty->cursor_state.cx = 0;
//         ty->termstate.had_cr = 1;
         return;
      default:
         return;
     }
}

static void
_switch_to_alternative_screen(Termpty *ty, int mode)
{
   DBG("switch to alternative screen, mode:%d", mode);
   if (ty->altbuf)
     {
        // if we are looking at alt buf now,
        // clear main buf before we swap it back
        // into the screen2 save (so save is
        // clear)
        termpty_clear_all(ty);
     }
   // swap screen content now
   if (mode != ty->altbuf)
     termpty_screen_swap(ty);
}

static void
_handle_esc_csi_reset_mode(Termpty *ty, Eina_Unicode cc, Eina_Unicode *b)
{
   int mode = 0, priv = 0, arg;

   if (cc == 'h') mode = 1;
   if (*b == '?')
     {
        priv = 1;
        b++;
     }
   if (priv) /* DEC Private Mode Reset (DECRST) */
     {
        while (b)
          {
             arg = _csi_arg_get(&b);
             if (b)
               {
                  // complete-ish list here:
                  // http://ttssh2.sourceforge.jp/manual/en/about/ctrlseq.html
                  switch (arg)
                    {
                     case 1:
                        ty->termstate.appcursor = mode;
                        break;
                     case 2:
                        ty->termstate.kbd_lock = mode;
                        break;
                     case 3: // 132 column mode… should we handle this?
#if defined(SUPPORT_80_132_COLUMNS)
                        if (ty->termstate.att.is_80_132_mode_allowed)
                          {
                             /* ONLY FOR TESTING PURPOSE FTM */
                             Evas_Object *wn;
                             int w, h;

                             wn = termio_win_get(ty->obj);
                             elm_win_size_step_get(wn, &w, &h);
                             evas_object_resize(wn,
                                                4 +
                                                (mode ? 132 : 80) * w,
                                                4 + ty->h * h);
                             termpty_resize(ty, mode ? 132 : 80,
                                            ty->h);
                             termpty_reset_state(ty);
                             termpty_clear_screen(ty,
                                                   TERMPTY_CLR_ALL);
                          }
#endif
                        break;
                     case 4:
                        WRN("TODO: scrolling mode (DECSCLM): %i", mode);
                        break;
                     case 5:
                        ty->termstate.reverse = mode;
                        break;
                     case 6:
                        if (mode)
                          {
                             ty->termstate.margin_top = ty->cursor_state.cy;
                             ty->cursor_state.cx = 0;
                          }
                        else
                          {
                             ty->cursor_state.cx = 0;
                             ty->termstate.margin_top = 0;
                          }
                        DBG("origin mode (%d): cursor is at 0,0"
                            " cursor limited to screen/start point"
                            " for line #'s depends on top margin",
                            mode);
                        break;
                     case 7:
                        DBG("set wrap mode to %i", mode);
                        ty->termstate.wrap = mode;
                        break;
                     case 8:
                        ty->termstate.no_autorepeat = !mode;
                        DBG("auto repeat %i", mode);
                        break;
                     case 9:
                        DBG("set mouse (X10) %i", mode);
                        if (mode) ty->mouse_mode = MOUSE_X10;
                        else ty->mouse_mode = MOUSE_OFF;
                        break;
                     case 12: // ignore
                        WRN("TODO: set blinking cursor to (stop?) %i or local echo (ignored)", mode);
                        break;
                     case 19: // never seen this - what to do?
                        WRN("TODO: set print extent to full screen");
                        break;
                     case 20: // crfl==1 -> cur moves to col 0 on LF, FF or VT, ==0 -> mode is cr+lf
                        ty->termstate.crlf = mode;
                        break;
                     case 25:
                        ty->termstate.hide_cursor = !mode;
                        DBG("hide cursor: %d", !mode);
                        break;
                     case 30: // ignore
                        WRN("TODO: set scrollbar mapping %i", mode);
                        break;
                     case 33: // ignore
                        WRN("TODO: Stop cursor blink %i", mode);
                        break;
                     case 34: // ignore
                        WRN("TODO: Underline cursor mode %i", mode);
                        break;
                     case 35: // ignore
                        WRN("TODO: set shift keys %i", mode);
                        break;
                     case 38: // ignore
                        WRN("TODO: switch to tek window %i", mode);
                        break;
                     case 40:
                        DBG("Allow 80 -> 132 Mode %i", mode);
#if defined(SUPPORT_80_132_COLUMNS)
                        ty->termstate.att.is_80_132_mode_allowed = mode;
#endif
                        break;
                     case 45: // ignore
                        WRN("TODO: Reverse-wraparound Mode");
                        break;
                     case 59: // ignore
                        WRN("TODO: kanji terminal mode %i", mode);
                        break;
                     case 66:
                        WRN("TODO: app keypad mode %i", mode);
                        break;
                     case 67:
                        ty->termstate.send_bs = mode;
                        DBG("backspace send bs not del = %i", mode);
                        break;
                     case 1000:
                        if (mode) ty->mouse_mode = MOUSE_NORMAL;
                        else ty->mouse_mode = MOUSE_OFF;
                        DBG("set mouse (press+release only) to %i", mode);
                        break;
                     case 1001:
                        WRN("TODO: x11 mouse highlighting %i", mode);
                        break;
                     case 1002:
                        if (mode) ty->mouse_mode = MOUSE_NORMAL_BTN_MOVE;
                        else ty->mouse_mode = MOUSE_OFF;
                        DBG("set mouse (press+release+motion while pressed) %i", mode);
                        break;
                     case 1003:
                        if (mode) ty->mouse_mode = MOUSE_NORMAL_ALL_MOVE;
                        else ty->mouse_mode = MOUSE_OFF;
                        DBG("set mouse (press+release+all motion) %i", mode);
                        break;
                     case 1004: // i dont know what focus repporting is?
                        WRN("TODO: enable focus reporting %i", mode);
                        break;
                     case 1005:
                        if (mode) ty->mouse_ext = MOUSE_EXT_UTF8;
                        else ty->mouse_ext = MOUSE_EXT_NONE;
                        DBG("set mouse (xterm utf8 style) %i", mode);
                        break;
                     case 1006:
                        if (mode) ty->mouse_ext = MOUSE_EXT_SGR;
                        else ty->mouse_ext = MOUSE_EXT_NONE;
                        DBG("set mouse (xterm sgr style) %i", mode);
                        break;
                     case 1010: // ignore
                        WRN("TODO: set home on tty output %i", mode);
                        break;
                     case 1012: // ignore
                        WRN("TODO: set home on tty input %i", mode);
                        break;
                     case 1015:
                        if (mode) ty->mouse_ext = MOUSE_EXT_URXVT;
                        else ty->mouse_ext = MOUSE_EXT_NONE;
                        DBG("set mouse (rxvt-unicode style) %i", mode);
                        break;
                     case 1034: // ignore
                        /* libreadline6 emits it but it shouldn't.
                           See http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=577012
                           */
                        DBG("Ignored screen mode %i", arg);
                        break;
                     case 1047:
                     case 47:
                        _switch_to_alternative_screen(ty, mode);
                        break;
                     case 1048:
                     case 1049:
                        termpty_cursor_copy(ty, mode);
                        if (arg == 1049)
                          _switch_to_alternative_screen(ty, mode);
                        break;
                     case 2004:
                        ty->bracketed_paste = mode;
                        break;
                     case 7727: // ignore
                        WRN("TODO: enable application escape mode %i", mode);
                        break;
                     case 7786: // ignore
                        WRN("TODO: enable mouse wheel -> cursor key xlation %i", mode);
                        break;
                     default:
                        WRN("Unhandled DEC Private Reset Mode arg %i", arg);
                        break;
                    }
               }
          }
     }
   else /* Reset Mode (RM) */
     {
        while (b)
          {
             arg = _csi_arg_get(&b);
             if (b)
               {
                  switch (arg)
                    {
                     case 1:
                        ty->termstate.appcursor = mode;
                        break;
                     case 4:
                        DBG("set insert mode to %i", mode);
                        ty->termstate.insert = mode;
                        break;
                     case 34:
                        WRN("TODO: hebrew keyboard mapping: %i", mode);
                        break;
                     case 36:
                        WRN("TODO: hebrew encoding mode: %i", mode);
                        break;
                     default:
                        WRN("Unhandled ANSI Reset Mode arg %i", arg);
                    }
               }
          }
     }
}

static void
_handle_esc_csi_color_set(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int first = 1;

   if (b && (*b == '>'))
     { // key resources used by xterm
        WRN("TODO: set/reset key resources used by xterm");
        return;
     }
   DBG("color set");
   while (b)
     {
        int arg = _csi_arg_get(&b);
        if ((first) && (!b))
          termpty_reset_att(&(ty->termstate.att));
        else if (b)
          {
             first = 0;
             switch (arg)
               {
                case 0: // reset to normal
                   termpty_reset_att(&(ty->termstate.att));
                   break;
                case 1: // bold/bright
                   ty->termstate.att.bold = 1;
                   break;
                case 2: // faint
                   ty->termstate.att.faint = 1;
                   break;
                case 3: // italic
                   ty->termstate.att.italic = 1;
                   break;
                case 4: // underline
                   ty->termstate.att.underline = 1;
                   break;
                case 5: // blink
                   ty->termstate.att.blink = 1;
                   break;
                case 6: // blink rapid
                   ty->termstate.att.blink2 = 1;
                   break;
                case 7: // reverse
                   ty->termstate.att.inverse = 1;
                   break;
                case 8: // invisible
                   ty->termstate.att.invisible = 1;
                   break;
                case 9: // strikethrough
                   ty->termstate.att.strike = 1;
                   break;
                case 20: // fraktur!
                   ty->termstate.att.fraktur = 1;
                   break;
                case 21: // no bold/bright
                   ty->termstate.att.bold = 0;
                   break;
                case 22: // no bold/bright, no faint
                   ty->termstate.att.bold = 0;
                   ty->termstate.att.faint = 0;
                   break;
                case 23: // no italic, not fraktur
                   ty->termstate.att.italic = 0;
                   ty->termstate.att.fraktur = 0;
                   break;
                case 24: // no underline
                   ty->termstate.att.underline = 0;
                   break;
                case 25: // no blink
                   ty->termstate.att.blink = 0;
                   ty->termstate.att.blink2 = 0;
                   break;
                case 27: // no reverse
                   ty->termstate.att.inverse = 0;
                   break;
                case 28: // no invisible
                   ty->termstate.att.invisible = 0;
                   break;
                case 29: // no strikethrough
                   ty->termstate.att.strike = 0;
                   break;
                case 30: // fg
                case 31:
                case 32:
                case 33:
                case 34:
                case 35:
                case 36:
                case 37:
                   ty->termstate.att.fg256 = 0;
                   ty->termstate.att.fg = (arg - 30) + COL_BLACK;
                   ty->termstate.att.fgintense = 0;
                   break;
                case 38: // xterm 256 fg color ???
                   // now check if next arg is 5
                   arg = _csi_arg_get(&b);
                   if (arg != 5) ERR("Failed xterm 256 color fg esc 5 (got %d)", arg);
                   else
                     {
                        // then get next arg - should be color index 0-255
                        arg = _csi_arg_get(&b);
                        if (!b) ERR("Failed xterm 256 color fg esc val");
                        else
                          {
                             ty->termstate.att.fg256 = 1;
                             ty->termstate.att.fg = arg;
                          }
                     }
                   ty->termstate.att.fgintense = 0;
                   break;
                case 39: // default fg color
                   ty->termstate.att.fg256 = 0;
                   ty->termstate.att.fg = COL_DEF;
                   ty->termstate.att.fgintense = 0;
                   break;
                case 40: // bg
                case 41:
                case 42:
                case 43:
                case 44:
                case 45:
                case 46:
                case 47:
                   ty->termstate.att.bg256 = 0;
                   ty->termstate.att.bg = (arg - 40) + COL_BLACK;
                   ty->termstate.att.bgintense = 0;
                   break;
                case 48: // xterm 256 bg color ???
                   // now check if next arg is 5
                   arg = _csi_arg_get(&b);
                   if (arg != 5) ERR("Failed xterm 256 color bg esc 5 (got %d)", arg);
                   else
                     {
                        // then get next arg - should be color index 0-255
                        arg = _csi_arg_get(&b);
                        if (!b) ERR("Failed xterm 256 color bg esc val");
                        else
                          {
                             ty->termstate.att.bg256 = 1;
                             ty->termstate.att.bg = arg;
                          }
                     }
                   ty->termstate.att.bgintense = 0;
                   break;
                case 49: // default bg color
                   ty->termstate.att.bg256 = 0;
                   ty->termstate.att.bg = COL_DEF;
                   ty->termstate.att.bgintense = 0;
                   break;
                case 90: // fg
                case 91:
                case 92:
                case 93:
                case 94:
                case 95:
                case 96:
                case 97:
                   ty->termstate.att.fg256 = 0;
                   ty->termstate.att.fg = (arg - 90) + COL_BLACK;
                   ty->termstate.att.fgintense = 1;
                   break;
                case 98: // xterm 256 fg color ???
                   // now check if next arg is 5
                   arg = _csi_arg_get(&b);
                   if (arg != 5) ERR("Failed xterm 256 color fg esc 5 (got %d)", arg);
                   else
                     {
                        // then get next arg - should be color index 0-255
                        arg = _csi_arg_get(&b);
                        if (!b) ERR("Failed xterm 256 color fg esc val");
                        else
                          {
                             ty->termstate.att.fg256 = 1;
                             ty->termstate.att.fg = arg;
                          }
                     }
                   ty->termstate.att.fgintense = 1;
                   break;
                case 99: // default fg color
                   ty->termstate.att.fg256 = 0;
                   ty->termstate.att.fg = COL_DEF;
                   ty->termstate.att.fgintense = 1;
                   break;
                case 100: // bg
                case 101:
                case 102:
                case 103:
                case 104:
                case 105:
                case 106:
                case 107:
                   ty->termstate.att.bg256 = 0;
                   ty->termstate.att.bg = (arg - 100) + COL_BLACK;
                   ty->termstate.att.bgintense = 1;
                   break;
                case 108: // xterm 256 bg color ???
                   // now check if next arg is 5
                   arg = _csi_arg_get(&b);
                   if (arg != 5) ERR("Failed xterm 256 color bg esc 5 (got %d)", arg);
                   else
                     {
                        // then get next arg - should be color index 0-255
                        arg = _csi_arg_get(&b);
                        if (!b) ERR("Failed xterm 256 color bg esc val");
                        else
                          {
                             ty->termstate.att.bg256 = 1;
                             ty->termstate.att.bg = arg;
                          }
                     }
                   ty->termstate.att.bgintense = 1;
                   break;
                case 109: // default bg color
                   ty->termstate.att.bg256 = 0;
                   ty->termstate.att.bg = COL_DEF;
                   ty->termstate.att.bgintense = 1;
                   break;
                default: //  not handled???
                   WRN("Unhandled color cmd [%i]", arg);
                   break;
               }
          }
     }
}

static void
_handle_esc_csi_dsr(Termpty *ty, Eina_Unicode *b)
{
   int arg, len;
   char bf[32];

   if (*b == '>')
     {
        WRN("TODO: disable key resources used by xterm");
        return;
     }
   if (*b == '?')
     {
        b++;
        arg = _csi_arg_get(&b);
        switch (arg)
          {
           case 6:
              len = snprintf(bf, sizeof(bf), "\033[?%d;%d;1R",
                             ty->cursor_state.cy + 1, ty->cursor_state.cx + 1);
              termpty_write(ty, bf, len);
              break;
           default:
              WRN("unhandled DSR (dec specific) %d", arg);
              break;
          }
     }
   else
     {
        arg = _csi_arg_get(&b);
        switch (arg)
          {
           case 6:
              len = snprintf(bf, sizeof(bf), "\033[%d;%dR",
                             ty->cursor_state.cy + 1, ty->cursor_state.cx + 1);
              termpty_write(ty, bf, len);
              break;
           default:
              WRN("unhandled DSR %d", arg);
              break;
          }
     }
}

static int
_handle_esc_csi(Termpty *ty, const Eina_Unicode *c, Eina_Unicode *ce)
{
   int arg, i;
   const Eina_Unicode *cc, *be;
   Eina_Unicode buf[4096], *b;

   cc = (Eina_Unicode *)c;
   b = buf;
   be = buf + sizeof(buf) / sizeof(buf[0]);
   while ((cc < ce) && (*cc <= '?') && (b < be))
     {
        _handle_cursor_control(ty, cc);
        *b = *cc;
        b++;
        cc++;
     }
   if (b == be)
     {
        ERR("csi parsing overflowed, skipping the whole buffer (binary data?)");
        return cc - c;
     }
   if (cc == ce) return 0;
   *b = 0;
   b = buf;
   DBG(" CSI: '%s' args '%s'", _safechar(*cc), (char *) buf);
   switch (*cc)
     {
      case 'm': // color set
        _handle_esc_csi_color_set(ty, &b);
        break;
      case '@': // insert N blank chars
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        DBG("insert %d blank chars", arg);
          {
             int pi = ty->termstate.insert;
             Eina_Unicode blank[1] = { ' ' };
             int cx = ty->cursor_state.cx;

             ty->termstate.wrapnext = 0;
             ty->termstate.insert = 1;
             for (i = 0; i < arg; i++)
               termpty_text_append(ty, blank, 1);
             ty->termstate.insert = pi;
             ty->cursor_state.cx = cx;
             TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
          }
        break;
      case 'A': // cursor up N
      case 'e': // cursor up N
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        DBG("cursor up %d", arg);
        ty->termstate.wrapnext = 0;
        ty->cursor_state.cy = MAX(0, ty->cursor_state.cy - arg);
        TERMPTY_RESTRICT_FIELD(ty->cursor_state.cy, 0, ty->h);
        break;
      case 'B': // cursor down N
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        DBG("cursor down %d", arg);
        ty->termstate.wrapnext = 0;
        ty->cursor_state.cy = MIN(ty->h - 1, ty->cursor_state.cy + arg);
        TERMPTY_RESTRICT_FIELD(ty->cursor_state.cy, 0, ty->h);
        break;
      case 'D': // cursor left N
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        DBG("cursor left %d", arg);
        ty->termstate.wrapnext = 0;
        for (i = 0; i < arg; i++)
          ty->cursor_state.cx--;
        TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
        break;
      case 'C': // cursor right N
      case 'a': // cursor right N
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        DBG("cursor right %d", arg);
        ty->termstate.wrapnext = 0;
        for (i = 0; i < arg; i++)
          ty->cursor_state.cx++;
        TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
        break;
      case 'H': // cursor pos set
      case 'f': // cursor pos set
        DBG("cursor pos set");
        ty->termstate.wrapnext = 0;
        if (!*b)
          {
             ty->cursor_state.cx = 0;
             ty->cursor_state.cy = 0;
          }
        else
          {
             arg = _csi_arg_get(&b);
             if (arg < 1) arg = 1;
             arg--;
             if (arg >= ty->h) arg = ty->h - 1;
             if (b)
               {
                  ty->cursor_state.cy = arg;
                  TERMPTY_RESTRICT_FIELD(ty->cursor_state.cy, 0, ty->h);
                  arg = _csi_arg_get(&b);
                  if (arg < 1) arg = 1;
                  arg--;
               }
             else arg = 0;

             if (arg >= ty->w) arg = ty->w - 1;
             if (b)
               {
                  ty->cursor_state.cx = arg;
                  TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
               }
          }
        ty->cursor_state.cy += ty->termstate.margin_top;
        TERMPTY_RESTRICT_FIELD(ty->cursor_state.cy, 0, ty->h);
       break;
      case 'G': // to column N
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        DBG("to column %d", arg);
        ty->termstate.wrapnext = 0;
        ty->cursor_state.cx = arg - 1;
        TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
        break;
      case 'd': // to row N
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        DBG("to row %d", arg);
        ty->termstate.wrapnext = 0;
        ty->cursor_state.cy = arg - 1;
        TERMPTY_RESTRICT_FIELD(ty->cursor_state.cy, 0, ty->h);
        break;
      case 'E': // down relative N rows, and to col 0
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        DBG("down relative %d rows, and to col 0", arg);
        ty->termstate.wrapnext = 0;
        ty->cursor_state.cy += arg;
        TERMPTY_RESTRICT_FIELD(ty->cursor_state.cy, 0, ty->h);
        ty->cursor_state.cx = 0;
        break;
      case 'F': // up relative N rows, and to col 0
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        DBG("up relative %d rows, and to col 0", arg);
        ty->termstate.wrapnext = 0;
        ty->cursor_state.cy -= arg;
        TERMPTY_RESTRICT_FIELD(ty->cursor_state.cy, 0, ty->h);
        ty->cursor_state.cx = 0;
        break;
      case 'X': // erase N chars
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        DBG("erase %d chars", arg);
        termpty_clear_line(ty, TERMPTY_CLR_END, arg);
        break;
      case 'S': // scroll up N lines
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        DBG("scroll up %d lines", arg);
        for (i = 0; i < arg; i++)
          termpty_text_scroll(ty, EINA_TRUE);
        break;
      case 'T': // scroll down N lines
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        DBG("scroll down %d lines", arg);
        for (i = 0; i < arg; i++)
          termpty_text_scroll_rev(ty, EINA_TRUE);
        break;
      case 'M': // delete N lines - cy
      case 'L': // insert N lines - cy
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        DBG("%s %d lines", (*cc == 'M') ? "delete" : "insert", arg);
          {
             int sy1, sy2;

             sy1 = ty->termstate.scroll_y1;
             sy2 = ty->termstate.scroll_y2;
             if (ty->termstate.scroll_y2 == 0)
               {
                  ty->termstate.scroll_y1 = ty->cursor_state.cy;
                  ty->termstate.scroll_y2 = ty->h;
               }
             else
               {
                  ty->termstate.scroll_y1 = ty->cursor_state.cy;
                  if (ty->termstate.scroll_y2 <= ty->termstate.scroll_y1)
                    ty->termstate.scroll_y2 = ty->termstate.scroll_y1 + 1;
               }
             for (i = 0; i < arg; i++)
               {
                  if (*cc == 'M')
                    termpty_text_scroll(ty, EINA_TRUE);
                  else
                    termpty_text_scroll_rev(ty, EINA_TRUE);
               }
             ty->termstate.scroll_y1 = sy1;
             ty->termstate.scroll_y2 = sy2;
          }
        break;
      case 'P': // erase and scrollback N chars
        arg = _csi_arg_get(&b);
        if (arg < 1) arg = 1;
        DBG("erase and scrollback %d chars", arg);
          {
             Termcell *cells;
             int x, lim;

             cells = &(TERMPTY_SCREEN(ty, 0, ty->cursor_state.cy));
             lim = ty->w - arg;
             for (x = ty->cursor_state.cx; x < (ty->w); x++)
               {
                  if (x < lim)
                    termpty_cell_copy(ty, &(cells[x + arg]), &(cells[x]), 1);
                  else
                    {
                       cells[x].codepoint = ' ';
                       cells[x].att.underline = 0;
                       cells[x].att.blink = 0;
                       cells[x].att.blink2 = 0;
                       cells[x].att.inverse = 0;
                       cells[x].att.strike = 0;
                       cells[x].att.dblwidth = 0;
                    }
               }
          }
        break;
      case 'c': // query device attributes
        DBG("query device attributes");
          {
             char bf[32];
             if (b && *b == '>')
               {
                  // Primary device attributes
                  //  0 → VT100
                  //  1 → VT220
                  //  2 → VT240
                  // 18 → VT330
                  // 19 → VT340
                  // 24 → VT320
                  // 41 → VT420
                  // 61 → VT510
                  // 64 → VT520
                  // 65 → VT525
                  snprintf(bf, sizeof(bf), "\033[>41;285;%ic", 0);
               }
             else
               {
                  // Secondary device attributes
                  snprintf(bf, sizeof(bf), "\033[?64;1;2;6;9;15;18;21;22c");
               }
             termpty_write(ty, bf, strlen(bf));
          }
        break;
      case 'J': // "2j" erases the screen, 1j erase from screen start to curs, 0j erase cursor to end of screen
        DBG("2j erases the screen, 1j erase from screen start to curs, 0j erase cursor to end of screen");
        arg = _csi_arg_get(&b);
        if (b)
          {
             if ((arg >= TERMPTY_CLR_END) && (arg <= TERMPTY_CLR_ALL))
               termpty_clear_screen(ty, arg);
             else
               WRN("invalid clr scr %i", arg);
          }
        else
          termpty_clear_screen(ty, TERMPTY_CLR_END);
        break;
      case 'K': // 0K erase to end of line, 1K erase from screen start to cursor, 2K erase all of line
        arg = _csi_arg_get(&b);
        DBG("0K erase to end of line, 1K erase from screen start to cursor, 2K erase all of line: %d", arg);
        if (b)
          {
             if ((arg >= TERMPTY_CLR_END) && (arg <= TERMPTY_CLR_ALL))
               termpty_clear_line(ty, arg, ty->w);
             else
               WRN("invalid clr lin %i", arg);
          }
        else termpty_clear_line(ty, TERMPTY_CLR_END, ty->w);
        break;
      case 'h':
      case 'l':
        _handle_esc_csi_reset_mode(ty, *cc, b);
        break;
      case 'r':
        arg = _csi_arg_get(&b);
        if (!b)
          {
             WRN("no region args reset region");
             ty->termstate.scroll_y1 = 0;
             ty->termstate.scroll_y2 = 0;
          }
        else
          {
             int arg2;

             arg2 = _csi_arg_get(&b);
             if (!b)
               {
                  WRN("failed to give 2 regions args reset region");
                  ty->termstate.scroll_y1 = 0;
                  ty->termstate.scroll_y2 = 0;
               }
             else
               {
                  if (arg > arg2)
                    {
                       DBG("scroll region beginning > end [%i %i]", arg, arg2);
                       ty->termstate.scroll_y1 = 0;
                       ty->termstate.scroll_y2 = 0;
                    }
                  else
                    {
                       DBG("2 regions args: %i %i", arg, arg2);
                       if (arg >= ty->h) arg = ty->h - 1;
                       if (arg == 0) arg = 1;
                       if (arg2 > ty->h) arg2 = ty->h;
                       ty->termstate.scroll_y1 = arg - 1;
                       ty->termstate.scroll_y2 = arg2;
                       if ((arg == 1) && (arg2 == ty->h))
                          ty->termstate.scroll_y2 = 0;
                    }
               }
          }
        break;
      case 's': // store cursor pos
        termpty_cursor_copy(ty, EINA_TRUE);
        break;
      case 't': // store cursor pos
        arg = _csi_arg_get(&b);
        WRN("TODO: window operation %d not supported", arg);
        break;
      case 'u': // restore cursor pos
        termpty_cursor_copy(ty, EINA_FALSE);
        break;
      case 'p': // define key assignments based on keycode
        if (b && *b == '!')
          {
             DBG("soft reset (DECSTR)");
             termpty_reset_state(ty);
          }
        else
          {
             goto unhandled;
          }
        break;
      case 'n':
        _handle_esc_csi_dsr(ty, b);
        break;
/*
      case 'R': // report cursor
        break;
      case 's':
        break;
      case 't':
        break;
      case 'q': // set/clear led's
        break;
      case 'x': // request terminal parameters
        break;
      case 'r': // set top and bottom margins
        break;
      case 'y': // invoke confidence test
        break;
 */
      case 'g': // clear tabulation
        arg = _csi_arg_get(&b);
        DBG("Tabulation Clear (TBC): %d", arg);
        break;
       case 'Z': // Cursor Back Tab
       {
          int idx, size, cx = ty->cursor_state.cx, cy = ty->cursor_state.cy;

          arg = _csi_arg_get(&b);
          if (arg < 1) arg = 1;

          size = ty->w * cy + cx + 1;
          for (idx = size - 1; idx >= 0; idx--)
            {
               if (TERMPTY_SCREEN(ty, cx, cy).att.tab) arg--;
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
               ty->cursor_state.cx = cx;
               TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
               ty->cursor_state.cy = cy;
               TERMPTY_RESTRICT_FIELD(ty->cursor_state.cy, 0, ty->h);
            }
       }
       break;
      default:
       goto unhandled;
     }
   cc++;
   return cc - c;
unhandled:
     {
        Eina_Strbuf *bf = eina_strbuf_new();

        for (i = 0; c + i <= cc && i < 100; i++)
          {
             if ((c[i] < ' ') || (c[i] >= 0x7f))
               eina_strbuf_append_printf(bf, "\033[35m%08x\033[0m",
                                         (unsigned int) c[i]);
             else
               eina_strbuf_append_char(bf, c[i]);
          }
        WRN("unhandled CSI '%s': %s", _safechar(*cc), eina_strbuf_string_get(bf));
        eina_strbuf_free(bf);
     }
   cc++;
   return cc - c;
}

static int
_xterm_arg_get(Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int sum = 0;

   while (*b >= '0' && *b <= '9')
     {
        sum *= 10;
        sum += *b - '0';
        b++;
     }
   if (*b != ';')
     sum = -1;
   else
     b++;
   *ptr = b;
   return sum;
}

static int
_eina_unicode_to_hex(Eina_Unicode u)
{
   if (u >= '0' && u <= '9')
     return u - '0';
   if (u >= 'a' && u <= 'f')
     return 10 + u - 'a';
   if (u >= 'A' && u <= 'F')
     return 10 + u - 'A';
   return -1;
}

static int
_xterm_parse_color(Eina_Unicode **ptr, unsigned char *r, unsigned char *g,
                   unsigned char *b, int len)
{
   Eina_Unicode *p = *ptr;
   int i;

   if (*p != '#')
     {
        WRN("unsupported xterm color");
        return -1;
     }
   p++;
   len--;
   if (len == 7)
     {
        i = _eina_unicode_to_hex(p[0]);
        if (i < 0) goto err;
        *r = i;
        i = _eina_unicode_to_hex(p[1]);
        if (i < 0) goto err;
        *r = *r * 16 + i;

        i = _eina_unicode_to_hex(p[2]);
        if (i < 0) goto err;
        *g = i;
        i = _eina_unicode_to_hex(p[3]);
        if (i < 0) goto err;
        *g = *g * 16 + i;

        i = _eina_unicode_to_hex(p[4]);
        if (i < 0) goto err;
        *b = i;
        i = _eina_unicode_to_hex(p[5]);
        if (i < 0) goto err;
        *b = *b * 16 + i;
     }
   else if (len == 4)
     {
        i = _eina_unicode_to_hex(p[0]);
        if (i < 0) goto err;
        *r = i;
        i = _eina_unicode_to_hex(p[1]);
        if (i < 0) goto err;
        *g = i;
        i = _eina_unicode_to_hex(p[2]);
        if (i < 0) goto err;
        *b = i;
     }
   else
     {
        goto err;
     }

   return 0;

err:
   WRN("invalid xterm color");
   return -1;
}


static void
_handle_xterm_777_command(Termpty *ty EINA_UNUSED,
                          char *s
#if ((ELM_VERSION_MAJOR == 1) && (ELM_VERSION_MINOR < 8))
              EINA_UNUSED
#endif
                          , int len EINA_UNUSED)
{
#if (ELM_VERSION_MAJOR > 1) || (ELM_VERSION_MINOR >= 8)
   char *cmd_end = NULL,
        *title = NULL,
        *title_end = NULL,
        *message = NULL;

   if (strncmp(s, "notify;", strlen("notify;")))
     {
        WRN("unrecognized xterm 777 command %s", s);
        return;
     }

   if (!elm_need_sys_notify())
     {
        WRN("no elementary system notification support");
        return;
     }
   cmd_end = s + strlen("notify");
   if (*cmd_end != ';')
     return;
   *cmd_end = '\0';
   title = cmd_end + 1;
   title_end = strchr(title, ';');
   if (!title_end)
     {
        *cmd_end = ';';
        return;
     }
   *title_end = '\0';
   message = title_end + 1;

   elm_sys_notify_send(0, "dialog-information", title, message,
                       ELM_SYS_NOTIFY_URGENCY_NORMAL, -1,
                       NULL, NULL);
   *cmd_end = ';';
   *title_end = ';';
#endif
}

static int
_handle_esc_xterm(Termpty *ty, const Eina_Unicode *c, Eina_Unicode *ce)
{
   const Eina_Unicode *cc, *be;
   Eina_Unicode buf[4096], *p;
   char *s;
   int len = 0;
   int arg;

   cc = c;
   p = buf;
   be = buf + sizeof(buf) / sizeof(buf[0]);
   while ((cc < ce) && (*cc != ST) && (*cc != BEL) && (p < be))
     {
        if ((cc < ce - 1) && (*cc == ESC) && (*(cc + 1) == '\\'))
          {
             cc++;
             break;
          }
        *p = *cc;
        p++;
        cc++;
     }
   if (p == be)
     {
        ERR("xterm parsing overflowed, skipping the whole buffer (binary data?)");
        return cc - c;
     }
   *p = 0;
   p = buf;
   if ((*cc == ST) || (*cc == BEL) || (*cc == '\\')) cc++;
   else return 0;

#define TERMPTY_WRITE_STR(_S) \
   termpty_write(ty, _S, strlen(_S))

   arg = _xterm_arg_get(&p);
   switch (arg)
     {
      case -1:
         goto err;
      case 0:
        // XXX: title + name - callback
        if (!*p)
          goto err;
        if (*p == '?')
          {
             /* returns empty string. See CVE-2003-0063 */
             TERMPTY_WRITE_STR("\033]0;Terminology\007");
          }
        else
          {
             s = eina_unicode_unicode_to_utf8(p, &len);
             if (ty->prop.title) eina_stringshare_del(ty->prop.title);
             if (ty->prop.icon) eina_stringshare_del(ty->prop.icon);
             if (s)
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
             if (ty->cb.set_title.func && !ty->prop.user_title)
               ty->cb.set_title.func(ty->cb.set_title.data);
             if (ty->cb.set_icon.func) ty->cb.set_icon.func(ty->cb.set_icon.data);
          }
        break;
      case 1:
        // XXX: icon name - callback
        if (!*p)
          goto err;
        if (*p == '?')
          {
             /* returns empty string. See CVE-2003-0063 */
             TERMPTY_WRITE_STR("\033]1;Terminology\007");
          }
        else
          {
             s = eina_unicode_unicode_to_utf8(p, &len);
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
             if (ty->cb.set_icon.func) ty->cb.set_icon.func(ty->cb.set_icon.data);
          }
        break;
      case 2:
        // XXX: title - callback
        if (!*p)
          goto err;
        if (*p == '?')
          {
             /* returns empty string. See CVE-2003-0063 */
             TERMPTY_WRITE_STR("\033]2;Terminology\007");
          }
        else
          {
             s = eina_unicode_unicode_to_utf8(p, &len);
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
          }
        break;
      case 4:
        if (!*p)
          goto err;
        // XXX: set palette entry. not supported.
        WRN("set palette, not supported");
        if ((cc - c) < 3) return 0;
        break;
      case 10:
        if (!*p)
          goto err;
        if (*p == '?')
          {
             char bf[6];
             Config *config = termio_config_get(ty->obj);
             TERMPTY_WRITE_STR("\033]10;#");
             snprintf(bf, sizeof(bf), "%.2X%.2X%.2X",
                      config->colors[0].r,
                      config->colors[0].g,
                      config->colors[0].b);
             termpty_write(ty, bf, 6);
             TERMPTY_WRITE_STR("\007");
          }
        else
          {
             unsigned char r, g, b;
             len = cc - c - (p - buf);
             if (_xterm_parse_color(&p, &r, &g, &b, len) < 0)
               goto err;
#ifndef ENABLE_FUZZING
             evas_object_textgrid_palette_set(
                termio_textgrid_get(ty->obj),
                EVAS_TEXTGRID_PALETTE_STANDARD, 0,
                r, g, b, 0xff);
#endif
          }
        break;
      case 777:
        DBG("xterm notification support");
        s = eina_unicode_unicode_to_utf8(p, &len);
        if (s)
          {
             _handle_xterm_777_command(ty, s, len);
             free(s);
          }
        break;
      default:
        // many others
        WRN("unhandled xterm esc %d", arg);
        break;
     }

#undef TERMPTY_WRITE_STR

    return cc - c;
err:
    WRN("invalid xterm sequence");
    return cc - c;
}

static int
_handle_esc_terminology(Termpty *ty, const Eina_Unicode *c, const Eina_Unicode *ce)
{
   Eina_Unicode *cc;
   Eina_Unicode *buf, bufsmall[1024], *b;
   char *cmd;
   int blen = 0;
   Config *config;

   config = termio_config_get(ty->obj);

   cc = (Eina_Unicode *)c;
   while ((cc < ce) && (*cc != 0x0))
     {
        blen++;
        cc++;
     }
   buf = bufsmall;
   if (blen > (int)((sizeof(bufsmall) / sizeof(Eina_Unicode)) - 40))
     buf = malloc((blen * sizeof(Eina_Unicode)) + 40);
   cc = (Eina_Unicode *)c;
   b = buf;
   while ((cc < ce) && (*cc != 0x0))
     {
        *b = *cc;
        b++;
        cc++;
     }
   if ((cc < ce) && (*cc == 0x0)) cc++;
   else
     {
        if (buf != bufsmall) free(buf);
        return 0;
     }
   *b = 0;

   // commands are stored in the buffer, 0 bytes not allowed (end marker)
   cmd = eina_unicode_unicode_to_utf8(buf, NULL);
   ty->cur_cmd = cmd;
   if ((!config->ty_escapes) || (!_termpty_ext_handle(ty, cmd, buf)))
     {
        if (ty->cb.command.func) ty->cb.command.func(ty->cb.command.data);
     }
   ty->cur_cmd = NULL;
   free(cmd);

   if (buf != bufsmall) free(buf);
   return cc - c;
}

static int
_handle_esc_dcs(Termpty *ty EINA_UNUSED, const Eina_Unicode *c, const Eina_Unicode *ce)
{
   const Eina_Unicode *cc, *be;
   Eina_Unicode buf[4096], *b;
   int len = 0;

   cc = c;
   b = buf;
   be = buf + sizeof(buf) / sizeof(buf[0]);
   while ((cc < ce) && (*cc != ST) && (b < be))
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
   if (b == be)
     {
        ERR("dcs parsing overflowed, skipping the whole buffer (binary data?)");
        len = cc - c;
        goto end;
     }
   *b = 0;
   if ((*cc == ST) || (*cc == '\\')) cc++;
   else return 0;
   len = cc - c;
   switch (buf[0])
     {
      case '+':
         if (len < 4)
           goto end;
         switch (buf[1])
           {
            case 'q':
              WRN("unhandled dsc request to get termcap/terminfo");
              /* TODO */
              goto end;
               break;
            case 'p':
              WRN("unhandled dsc request to set termcap/terminfo");
              /* TODO */
              goto end;
               break;
            default:
              ERR("invalid dsc request about termcap/terminfo");
              goto end;
           }
         break;
      case '$':
         /* Request status string */
         if (len > 1 && buf[1] != 'q')
           {
              WRN("invalid/unhandled dsc esc '$%s' (expected '$q')", _safechar(buf[1]));
              goto end;
           }
         if (len < 4)
           goto end;
         switch (buf[2])
           {
            case '"':
               if (buf[3] == 'p') /* DECSCL */
                 {
                    char bf[32];
                    snprintf(bf, sizeof(bf), "\033P1$r64;1\"p\033\\");
                    termpty_write(ty, bf, strlen(bf));
                 }
               else if (buf[3] == 'q') /* DECSCA */
                 {
                    WRN("unhandled DECSCA '$qq'");
                    goto end;
                 }
               else
                 {
                    WRN("invalid/unhandled dsc esc '$q\"%s'", _safechar(buf[3]));
                    goto end;
                 }
               break;
            case 'm': /* SGR */
               /* TODO: */
            case 'r': /* DECSTBM */
               /* TODO: */
            default:
               WRN("unhandled dsc request status string '$q%s'", _safechar(buf[2]));
               goto end;
           }
         /* TODO */
         break;
      default:
        // many others
        WRN("Unhandled DCS escape '%s'", _safechar(buf[0]));
        break;
     }
end:
   return len;
}

static int
_handle_esc(Termpty *ty, const Eina_Unicode *c, Eina_Unicode *ce)
{
   int len = ce - c;

   if (len < 1) return 0;
   DBG("ESC: '%s'", _safechar(c[0]));
   switch (c[0])
     {
      case '[':
        len = _handle_esc_csi(ty, c + 1, ce);
        if (len == 0) return 0;
        return 1 + len;
      case ']':
        len = _handle_esc_xterm(ty, c + 1, ce);
        if (len == 0) return 0;
        return 1 + len;
      case '}':
        len = _handle_esc_terminology(ty, c + 1, ce);
        if (len == 0) return 0;
        return 1 + len;
      case 'P':
        len =  _handle_esc_dcs(ty, c + 1, ce);
        if (len == 0) return 0;
        return 1 + len;
      case '=': // set alternate keypad mode
        ty->termstate.alt_kp = 1;
        return 1;
      case '>': // set numeric keypad mode
        ty->termstate.alt_kp = 0;
        return 1;
      case 'M': // move to prev line
        ty->termstate.wrapnext = 0;
        ty->cursor_state.cy--;
        termpty_text_scroll_rev_test(ty, EINA_TRUE);
        return 1;
      case 'D': // move to next line
        ty->termstate.wrapnext = 0;
        ty->cursor_state.cy++;
        termpty_text_scroll_test(ty, EINA_FALSE);
        return 1;
      case 'E': // add \n\r
        ty->termstate.wrapnext = 0;
        ty->cursor_state.cx = 0;
        ty->cursor_state.cy++;
        termpty_text_scroll_test(ty, EINA_FALSE);
        return 1;
      case 'Z': // same a 'ESC [ Pn c'
        _term_txt_write(ty, "\033[?1;2C");
        return 1;
      case 'c': // reset terminal to initial state
        DBG("reset to init mode and clear");
        termpty_reset_state(ty);
        termpty_clear_screen(ty, TERMPTY_CLR_ALL);
        if (ty->cb.cancel_sel.func)
          ty->cb.cancel_sel.func(ty->cb.cancel_sel.data);
        return 1;
      case '(': // charset 0
        if (len < 2) return 0;
        ty->termstate.chset[0] = c[1];
        ty->termstate.multibyte = 0;
        ty->termstate.charsetch = c[1];
        return 2;
      case ')': // charset 1
        if (len < 2) return 0;
        ty->termstate.chset[1] = c[1];
        ty->termstate.multibyte = 0;
        return 2;
      case '*': // charset 2
        if (len < 2) return 0;
        ty->termstate.chset[2] = c[1];
        ty->termstate.multibyte = 0;
        return 2;
      case '+': // charset 3
        if (len < 2) return 0;
        ty->termstate.chset[3] = c[1];
        ty->termstate.multibyte = 0;
        return 2;
      case '$': // charset -2
        if (len < 2) return 0;
        ty->termstate.chset[2] = c[1];
        ty->termstate.multibyte = 1;
        return 2;
      case '#': // #8 == test mode -> fill screen with "E";
        if (len < 2) return 0;
        if (c[1] == '8')
          {
             int size;
             Termcell *cells;

             DBG("reset to init mode and clear then fill with E");
             termpty_reset_state(ty);
             termpty_clear_screen(ty, TERMPTY_CLR_ALL);
             if (ty->cb.cancel_sel.func)
               ty->cb.cancel_sel.func(ty->cb.cancel_sel.data);
             cells = ty->screen;
             size = ty->w * ty->h;
             if (cells)
               {
                  Termatt att;

                  memset((&att), 0, sizeof(att));
                  termpty_cell_codepoint_att_fill(ty, 'E', att, cells, size);
               }
          }
        return 2;
      case '@': // just consume this plus next char
        if (len < 2) return 0;
        return 2;
      case '7': // save cursor pos
        termpty_cursor_copy(ty, EINA_TRUE);
        return 1;
      case '8': // restore cursor pos
        termpty_cursor_copy(ty, EINA_FALSE);
        return 1;
      case 'H': // set tab at current column
        DBG("Character Tabulation Set (HTS)");
        return 1;
/*
      case 'G': // query gfx mode
        return 2;
      case 'n': // single shift 2
        return 1;
      case 'o': // single shift 3
        return 1;
 */
      default:
        WRN("Unhandled escape '%s' (0x%02x)", _safechar(c[0]), (unsigned int) c[0]);
        return 1;
     }
   return 0;
}

/* XXX: ce is excluded */
int
termpty_handle_seq(Termpty *ty, Eina_Unicode *c, Eina_Unicode *ce)
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
          printf("%s", _safechar(c[j]));
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
/*
           case 0x05: // ENQ (enquiry)
             _term_txt_write(ty, "ABC\r\n");
             ty->termstate.had_cr = 0;
             return 1;
 */
/*
           case 0x06: // ACK (acknowledge)
             return 1;
 */
           case 0x07: // BEL '\a' (bell)
           case 0x08: // BS  '\b' (backspace)
           case 0x09: // HT  '\t' (horizontal tab)
           case 0x0a: // LF  '\n' (new line)
           case 0x0b: // VT  '\v' (vertical tab)
           case 0x0c: // FF  '\f' (form feed)
           case 0x0d: // CR  '\r' (carriage ret)
             _handle_cursor_control(ty, c);
             return 1;

           case 0x0e: // SO  (shift out) // Maps G1 character set into GL.
             ty->termstate.had_cr = 0;
             ty->termstate.charset = 1;
             ty->termstate.charsetch = ty->termstate.chset[1];
             return 1;
           case 0x0f: // SI  (shift in) // Maps G0 character set into GL.
             ty->termstate.charset = 0;
             ty->termstate.had_cr = 0;
             ty->termstate.charsetch = ty->termstate.chset[0];
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
             ty->termstate.had_cr = 0;
             len = _handle_esc(ty, c + 1, ce);
             if (len == 0) return 0;
             return 1 + len;
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
             ty->termstate.had_cr = 0;
             //ERR("unhandled char 0x%02x", c[0]);
             return 1;
          }
     }
   else if (c[0] == 0x7f) // DEL
     {
        ty->termstate.had_cr = 0;
        WRN("Unhandled char 0x%02x [DEL]", (unsigned int) c[0]);
        return 1;
     }
   else if (c[0] == 0x9b) // ANSI ESC!!!
     {
        ty->termstate.had_cr = 0;
        DBG("ANSI CSI!!!!!");
        len = _handle_esc_csi(ty, c + 1, ce);
        if (len == 0) return 0;
        return 1 + len;
     }
   else if ((ty->block.expecting) && (ty->block.on))
     {
        Termexp *ex;
        Eina_List *l;
        
        ty->termstate.had_cr = 0;
        EINA_LIST_FOREACH(ty->block.expecting, l, ex)
          {
             if (c[0] == ex->ch)
               {
                  Eina_Unicode cp;
             
                  cp = (1 << 31) | ((ex->id & 0x1fff) << 18) |
                    ((ex->x & 0x1ff) << 9) | (ex->y & 0x1ff);
                  ex->x++;
                  if (ex->x >= ex->w)
                    {
                       ex->x = 0;
                       ex->y++;
                    }
                  ex->left--;
                  termpty_text_append(ty, &cp, 1);
                  if (ex->left <= 0)
                    {
                       ty->block.expecting = 
                         eina_list_remove_list(ty->block.expecting, l);
                       free(ex);
                    }
                  else
                    ty->block.expecting =
                    eina_list_promote_list(ty->block.expecting, l);
                  return 1;
               }
          }
        termpty_text_append(ty, c, 1);
        return 1;
     }
   else
     {
        ty->termstate.had_cr = 0;
     }
   cc = (Eina_Unicode *)c;
   DBG("txt: [");
   while ((cc < ce) && (*cc >= 0x20) && (*cc != 0x7f))
     {
        DBG("%s", _safechar(*cc));
        cc++;
        len++;
     }
   DBG("]");
   termpty_text_append(ty, c, len);
   ty->termstate.had_cr = 0;
   return len;
}
