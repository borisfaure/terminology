#include "private.h"
#include <Elementary.h>
#include <stdint.h>
#include "col.h"
#include "termio.h"
#include "termpty.h"
#include "termptydbl.h"
#include "termptyesc.h"
#include "termptyops.h"
#include "termptyext.h"
#if defined(ENABLE_TESTS)
#include "tytest.h"
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
#define UTF8CC 0xc2

/* XXX: all handle_ functions return the number of bytes successfully read, 0
 * if not enough bytes could be read
 */

static const char *const ASCII_CHARS_TABLE[] =
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

enum csi_arg_error {
     CSI_ARG_NO_VALUE = 1,
     CSI_ARG_ERROR = 2
};

static int
_csi_arg_get(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int sum = 0;

   if ((b == NULL) || (*b == '\0'))
     {
        *ptr = NULL;
        return -CSI_ARG_NO_VALUE;
     }

   /* Skip potential '?', '>'.... */
   while ((*b) && ( (*b) != ';' && ((*b) < '0' || (*b) > '9')))
     b++;

   if (*b == ';')
     {
        b++;
        *ptr = b;
        return -CSI_ARG_NO_VALUE;
     }

   if (*b == '\0')
     {
        *ptr = NULL;
        return -CSI_ARG_NO_VALUE;
     }

   while ((*b >= '0') && (*b <= '9'))
     {
        if (sum > INT32_MAX/10 )
          {
             ERR("Invalid sequence: argument is too large");
             ty->decoding_error = EINA_TRUE;
             goto error;
          }
        sum *= 10;
        sum += *b - '0';
        b++;
     }

   if ((*b == ';') || (*b == ':'))
     {
        if (b[1])
          b++;
        *ptr = b;
     }
   else if (*b == '\0')
     {
        *ptr = NULL;
     }
   else
     {
        *ptr = b;
     }
   return sum;

error:
   ty->decoding_error = EINA_TRUE;
   *ptr = NULL;
   return -CSI_ARG_ERROR;
}

static void
_tab_forward(Termpty *ty, int n)
{
   int cx = ty->cursor_state.cx;

   for (; n > 0; n--)
     {
        do
          {
             cx++;
          }
        while ((cx < ty->w) && (!TAB_TEST(ty, cx)));
     }

   ty->cursor_state.cx = cx;
   TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
}

static void
_cursor_to_start_of_line(Termpty *ty)
{
   ty->cursor_state.cx = ty->termstate.left_margin;
}

static void
_handle_cursor_control(Termpty *ty, const Eina_Unicode *cc)
{
   switch (*cc)
     {
      case 0x07: // BEL '\a' (bell)
         if (ty->cb.bell.func) ty->cb.bell.func(ty->cb.bell.data);
         return;
      case 0x08: // BS  '\b' (backspace)
         DBG("->BS");
         ty->termstate.wrapnext = 0;
         ty->cursor_state.cx--;
         TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
         return;
      case 0x09: // HT  '\t' (horizontal tab)
         DBG("->HT");
         _tab_forward(ty, 1);
         return;
      case 0x0a: // LF  '\n' (new line)
      case 0x0b: // VT  '\v' (vertical tab)
      case 0x0c: // FF  '\f' (form feed)
         DBG("->LF");
         ty->termstate.wrapnext = 0;
         if (ty->termstate.crlf)
           _cursor_to_start_of_line(ty);
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
         _cursor_to_start_of_line(ty);
         return;
      default:
         return;
     }
}

static void
_switch_to_alternative_screen(Termpty *ty, int mode)
{
   // swap screen content now
   if (mode != ty->altbuf)
     termpty_screen_swap(ty);
}

static void
_move_cursor_to_origin(Termpty *ty)
{
  if (ty->termstate.restrict_cursor)
    {
      ty->cursor_state.cx = ty->termstate.left_margin;
      TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
      ty->cursor_state.cy = ty->termstate.top_margin;
      TERMPTY_RESTRICT_FIELD(ty->cursor_state.cy, 0, ty->h);
    }
  else
    {
      ty->cursor_state.cx = 0;
      ty->cursor_state.cy = 0;
    }
}


static void
_handle_esc_csi_reset_mode(Termpty *ty, Eina_Unicode cc, Eina_Unicode *b,
                           const Eina_Unicode * const end)
{
   int mode = 0, priv = 0, arg;

   if (cc == 'h')
     mode = 1;
   if (*b == '?')
     {
        priv = 1;
        b++;
     }
   if (priv) /* DEC Private Mode Reset (DECRST) */
     {
        while (b && b <= end)
          {
             arg = _csi_arg_get(ty, &b);
             // complete-ish list here:
             // http://ttssh2.sourceforge.jp/manual/en/about/ctrlseq.html
             switch (arg)
               {
                case -CSI_ARG_ERROR:
                   return;
              /* TODO: -CSI_ARG_NO_VALUE */
                case 1:
                   ty->termstate.appcursor = mode;
                   break;
                case 2:
                   DBG("DECANM - ANSI MODE - VT52");
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
                   DBG("scrolling mode (DECSCLM): %i (always fast mode)", mode);
                   break;
                case 5:
                   ty->termstate.reverse = mode;
                   break;
                case 6:
                   /* DECOM */
                   if (mode)
                     {
                        /* set, within margins */
                        ty->termstate.restrict_cursor = 1;
                     }
                   else
                     {
                        ty->termstate.restrict_cursor = 0;
                     }
                   _move_cursor_to_origin(ty);
                   DBG("DECOM: mode (%d): cursor is at 0,0"
                       " cursor limited to screen/start point"
                       " for line #'s depends on top margin",
                       mode);
                   break;
                case 7:
                   DBG("DECAWM: set/reset wrap mode to %i", mode);
                   ty->termstate.wrap = mode;
                   break;
                case 8:
                   ty->termstate.no_autorepeat = !mode;
                   DBG("DECARM - auto repeat %i", mode);
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
                   ty->decoding_error = EINA_TRUE;
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
                   ty->decoding_error = EINA_TRUE;
                   break;
                case 33: // ignore
                   WRN("TODO: Stop cursor blink %i", mode);
                   break;
                case 34: // ignore
                   WRN("TODO: Underline cursor mode %i", mode);
                   ty->decoding_error = EINA_TRUE;
                   break;
                case 35: // ignore
                   WRN("TODO: set shift keys %i", mode);
                   ty->decoding_error = EINA_TRUE;
                   break;
                case 38: // ignore
                   WRN("TODO: switch to tek window %i", mode);
                   ty->decoding_error = EINA_TRUE;
                   break;
                case 40:
                   DBG("Allow 80 -> 132 Mode %i", mode);
#if defined(SUPPORT_80_132_COLUMNS)
                   ty->termstate.att.is_80_132_mode_allowed = mode;
#endif
                   break;
                case 45: // ignore
                   WRN("TODO: Reverse-wraparound Mode");
                   ty->decoding_error = EINA_TRUE;
                   break;
                case 47:
                   _switch_to_alternative_screen(ty, mode);
                   break;
                case 59: // ignore
                   WRN("TODO: kanji terminal mode %i", mode);
                   ty->decoding_error = EINA_TRUE;
                   break;
                case 66:
                   WRN("TODO: app keypad mode %i", mode);
                   ty->decoding_error = EINA_TRUE;
                   break;
                case 67:
                   ty->termstate.send_bs = mode;
                   DBG("backspace send bs not del = %i", mode);
                   break;
                case 69:
                   ty->termstate.lr_margins = mode;
                   if (!mode)
                     {
                        ty->termstate.left_margin = 0;
                        ty->termstate.right_margin = 0;
                     }
                   break;
                case 98:
                   DBG("DECARSM Set/Reset Auto Resize Mode: %d", mode);
                   break;
                case 100:
                   DBG("DECAAM Set/Reset Auto Answerback Mode: %d", mode);
                   break;
                case 101:
                   DBG("DECCANSM Set/Reset Conceal Answerback Message Mode: %d", mode);
                   break;
                case 109:
                   DBG("DECCAPSLK Set/Reset Caps Lock Mode: %d", mode);
                   break;
                case 1000:
                   if (mode) ty->mouse_mode = MOUSE_NORMAL;
                   else ty->mouse_mode = MOUSE_OFF;
                   DBG("set mouse (press+release only) to %i", mode);
                   break;
                case 1001:
                   WRN("TODO: x11 mouse highlighting %i", mode);
                   ty->decoding_error = EINA_TRUE;
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
                   ty->decoding_error = EINA_TRUE;
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
                   ty->decoding_error = EINA_TRUE;
                   break;
                case 1012: // ignore
                   WRN("TODO: set home on tty input %i", mode);
                   ty->decoding_error = EINA_TRUE;
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
                   ty->decoding_error = EINA_TRUE;
                   break;
                case 1047:
                   if (!mode && ty->altbuf)
                     /* clear screen before switching back to normal */
                     termpty_clear_screen(ty, TERMPTY_CLR_ALL);
                   _switch_to_alternative_screen(ty, mode);
                   break;
                case 1048:
                   termpty_cursor_copy(ty, mode);
                   break;
                case 1049:
                   if (mode)
                     {
                        // switch to altbuf
                        termpty_cursor_copy(ty, mode);
                        _switch_to_alternative_screen(ty, mode);
                        if (ty->altbuf)
                          /* clear screen before switching back to normal */
                          termpty_clear_screen(ty, TERMPTY_CLR_ALL);
                     }
                   else
                     {
                        if (ty->altbuf)
                          /* clear screen before switching back to normal */
                          termpty_clear_screen(ty, TERMPTY_CLR_ALL);
                        _switch_to_alternative_screen(ty, mode);
                        termpty_cursor_copy(ty, mode);
                     }
                   break;
                case 2004:
                   ty->bracketed_paste = mode;
                   break;
                case 7727: // ignore
                   WRN("TODO: enable application escape mode %i", mode);
                   ty->decoding_error = EINA_TRUE;
                   break;
                case 7786: // ignore
                   WRN("TODO: enable mouse wheel -> cursor key xlation %i", mode);
                   ty->decoding_error = EINA_TRUE;
                   break;
                default:
                   WRN("Unhandled DEC Private Reset Mode arg %i", arg);
                   ty->decoding_error = EINA_TRUE;
                   break;
               }
          }
     }
   else /* Reset Mode (RM) */
     {
        while (b && b <= end)
          {
             arg = _csi_arg_get(ty, &b);

             switch (arg)
               {
                case -CSI_ARG_ERROR:
                   return;
              /* TODO: -CSI_ARG_NO_VALUE */
                case 1:
                   ty->termstate.appcursor = mode;
                   break;
                case 3:
                   WRN("CRM - Show Control Character Mode: %i", mode);
                   ty->decoding_error = EINA_TRUE;
                   break;
                case 4:
                   DBG("set insert mode to %i", mode);
                   ty->termstate.insert = mode;
                   break;
                case 34:
                   WRN("TODO: hebrew keyboard mapping: %i", mode);
                   ty->decoding_error = EINA_TRUE;
                   break;
                case 36:
                   WRN("TODO: hebrew encoding mode: %i", mode);
                   ty->decoding_error = EINA_TRUE;
                   break;
                default:
                   WRN("Unhandled ANSI Reset Mode arg %i", arg);
                   ty->decoding_error = EINA_TRUE;
               }
          }
     }
}

static int
_csi_truecolor_arg_get(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int sum = 0;
   char separator;

   if ((b == NULL) || (*b == '\0'))
     {
        *ptr = NULL;
        return -CSI_ARG_NO_VALUE;
     }

   /* by construction, shall be the same separator as the following ones */
   separator = *(b-1);
   if ((separator != ';') && (separator != ':'))
     {
        *ptr = NULL;
        return -CSI_ARG_NO_VALUE;
     }

   if (*b == separator)
     {
        b++;
        *ptr = b;
        return -CSI_ARG_NO_VALUE;
     }

   if (*b == '\0')
     {
        *ptr = NULL;
        return -CSI_ARG_NO_VALUE;
     }

   while ((*b >= '0') && (*b <= '9'))
     {
        if (sum > INT32_MAX/10 )
          {
             *ptr = NULL;
             ERR("Invalid sequence: argument is too large");
             ty->decoding_error = EINA_TRUE;
             return -CSI_ARG_ERROR;
          }
        sum *= 10;
        sum += *b - '0';
        b++;
     }
   if (*b == separator)
     {
        b++;
        *ptr = b;
     }
   else if (*b == '\0')
     {
        *ptr = NULL;
     }
   else
     {
        *ptr = b;
     }
   return sum;
}


static int
_approximate_truecolor_rgb(Termpty *ty, int r0, int g0, int b0)
{
   int chosen_color = COL_DEF;
/* TODO: use the function in tests */
#if defined(ENABLE_FUZZING)
   (void) ty;
   (void) r0;
   (void) g0;
   (void) b0;
#else
   int c;
   int distance_min = INT_MAX;
   Evas_Object *textgrid;

   DBG("approximating r:%d g:%d b:%d", r0, g0, b0);

   textgrid = termio_textgrid_get(ty->obj);
   for (c = 0; c < 256; c++)
     {
        int r1 = 0, g1 = 0, b1 = 0, a1 = 0;
        int delta_red_sq, delta_green_sq, delta_blue_sq, middle_red;
        int distance;

        evas_object_textgrid_palette_get(textgrid,
                                         EVAS_TEXTGRID_PALETTE_EXTENDED,
                                         c, &r1, &g1, &b1, &a1);
        /* Compute the color distance
         * XXX: this is inacurate but should give good enough results.
         * See * https://en.wikipedia.org/wiki/Color_difference
         */
        middle_red = (r0 + r1) / 2;
        delta_red_sq = (r0 - r1) * (r0 - r1);
        delta_green_sq = (g0 - g1) * (g0 - g1);
        delta_blue_sq = (b0 - b1) * (b0 - b1);

        distance = 2 * delta_red_sq
           + 4 * delta_green_sq
           + 3 * delta_blue_sq
           + ((middle_red) * (delta_red_sq - delta_blue_sq) / 256);
        if (distance < distance_min)
          {
             distance_min = distance;
             chosen_color = c;
          }
     }
#endif
   return chosen_color;
}

static int
_handle_esc_csi_truecolor_rgb(Termpty *ty, Eina_Unicode **ptr)
{
   int r, g, b;
   Eina_Unicode *u = *ptr;
   char separator;

   if ((u == NULL) || (*u == '\0'))
     {
        return COL_DEF;
     }
   separator = *(u-1);

   r = _csi_truecolor_arg_get(ty, ptr);
   g = _csi_truecolor_arg_get(ty, ptr);
   b = _csi_truecolor_arg_get(ty, ptr);
   if ((r == -CSI_ARG_ERROR) ||
       (g == -CSI_ARG_ERROR) ||
       (b == -CSI_ARG_ERROR))
     return COL_DEF;

   if (separator == ':' && *ptr)
     {
        if (**ptr != ';')
          {
             /* then the first parameter was a color-space-id (ignored) */
             r = g;
             g = b;
             b = _csi_truecolor_arg_get(ty, ptr);
             /* Skip other parameters */
             while ((*ptr) && (**ptr != ';'))
               {
                  int arg = _csi_truecolor_arg_get(ty, ptr);
                  if (arg == -CSI_ARG_ERROR)
                    break;
               }
          }
        if ((*ptr) && (**ptr == ';'))
          {
             *ptr = (*ptr) + 1;
          }
     }

   if (r == -CSI_ARG_NO_VALUE)
     r = 0;
   if (g == -CSI_ARG_NO_VALUE)
     g = 0;
   if (b == -CSI_ARG_NO_VALUE)
     b = 0;

   return _approximate_truecolor_rgb(ty, r, g, b);
}

static int
_handle_esc_csi_truecolor_cmy(Termpty *ty, Eina_Unicode **ptr)
{
   int r, g, b, c, m, y;
   Eina_Unicode *u = *ptr;
   char separator;

   if ((u == NULL) || (*u == '\0'))
     {
        return COL_DEF;
     }
   separator = *(u-1);

   /* Considering CMY stored as percents */
   c = _csi_truecolor_arg_get(ty, ptr);
   m = _csi_truecolor_arg_get(ty, ptr);
   y = _csi_truecolor_arg_get(ty, ptr);

   if ((c == -CSI_ARG_ERROR) ||
       (m == -CSI_ARG_ERROR) ||
       (y == -CSI_ARG_ERROR))
     return COL_DEF;

   if (separator == ':' && *ptr)
     {
        if (**ptr != ';')
          {
             /* then the first parameter was a color-space-id (ignored) */
             c = m;
             m = y;
             y = _csi_truecolor_arg_get(ty, ptr);
             /* Skip other parameters */
             while ((*ptr) && (**ptr != ';'))
               {
                  int arg = _csi_truecolor_arg_get(ty, ptr);
                  if (arg == -CSI_ARG_ERROR)
                    break;
               }
          }
        if ((*ptr) && (**ptr == ';'))
          {
             *ptr = (*ptr) + 1;
          }
     }

   if (c == -CSI_ARG_NO_VALUE)
     c = 0;
   if (m == -CSI_ARG_NO_VALUE)
     m = 0;
   if (y == -CSI_ARG_NO_VALUE)
     y = 0;

   r = 255 - ((c * 255) / 100);
   g = 255 - ((m * 255) / 100);
   b = 255 - ((y * 255) / 100);

   return _approximate_truecolor_rgb(ty, r, g, b);
}

static int
_handle_esc_csi_truecolor_cmyk(Termpty *ty, Eina_Unicode **ptr)
{
   int r, g, b, c, m, y, k;
   Eina_Unicode *u = *ptr;
   char separator;

   if ((u == NULL) || (*u == '\0'))
     {
        return COL_DEF;
     }
   separator = *(u-1);

   /* Considering CMYK stored as percents */
   c = _csi_truecolor_arg_get(ty, ptr);
   m = _csi_truecolor_arg_get(ty, ptr);
   y = _csi_truecolor_arg_get(ty, ptr);
   k = _csi_truecolor_arg_get(ty, ptr);

   if ((c == -CSI_ARG_ERROR) ||
       (m == -CSI_ARG_ERROR) ||
       (y == -CSI_ARG_ERROR) ||
       (k == -CSI_ARG_ERROR))
     return COL_DEF;

   if (separator == ':' && *ptr)
     {
        if (**ptr != ';')
          {
             /* then the first parameter was a color-space-id (ignored) */
             c = m;
             m = y;
             y = k;
             k = _csi_truecolor_arg_get(ty, ptr);
             /* Skip other parameters */
             while ((*ptr) && (**ptr != ';'))
               {
                  int arg = _csi_truecolor_arg_get(ty, ptr);
                  if (arg == -CSI_ARG_ERROR)
                    break;
               }
          }
        if ((*ptr) && (**ptr == ';'))
          {
             *ptr = (*ptr) + 1;
          }
     }

   if (c == -CSI_ARG_NO_VALUE)
     c = 0;
   if (m == -CSI_ARG_NO_VALUE)
     m = 0;
   if (y == -CSI_ARG_NO_VALUE)
     y = 0;
   if (k == -CSI_ARG_NO_VALUE)
     k = 0;

   r = (255 * (100 - c) * (100 - k)) / 100 / 100;
   g = (255 * (100 - m) * (100 - k)) / 100 / 100;
   b = (255 * (100 - y) * (100 - k)) / 100 / 100;

   return _approximate_truecolor_rgb(ty, r, g, b);
}

static void
_handle_esc_csi_color_set(Termpty *ty, Eina_Unicode **ptr,
                          const Eina_Unicode * const end)
{
   Eina_Unicode *b = *ptr;

   if (b && (*b == '>'))
     { // key resources used by xterm
        WRN("TODO: set/reset key resources used by xterm");
        ty->decoding_error = EINA_TRUE;
        return;
     }
   DBG("color set");
   while (b && b <= end)
     {
        int arg = _csi_arg_get(ty, &b);
        switch (arg)
          {
           case -CSI_ARG_ERROR:
              return;
           case -CSI_ARG_NO_VALUE:
              EINA_FALLTHROUGH;
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
              arg = _csi_arg_get(ty, &b);
              switch (arg)
                {
                 case -CSI_ARG_ERROR:
                    return;
                   /* TODO: -CSI_ARG_NO_VALUE */
                 case 1:
                    ty->termstate.att.fg256 = 0;
                    ty->termstate.att.fg = COL_INVIS;
                    break;
                 case 2:
                    ty->termstate.att.fg256 = 1;
                    ty->termstate.att.fg =
                       _handle_esc_csi_truecolor_rgb(ty, &b);
                    DBG("truecolor RGB fg: approximation got color %d",
                        ty->termstate.att.fg);
                    break;
                 case 3:
                    ty->termstate.att.fg256 = 1;
                    ty->termstate.att.fg =
                       _handle_esc_csi_truecolor_cmy(ty, &b);
                    DBG("truecolor CMY fg: approximation got color %d",
                        ty->termstate.att.fg);
                    break;
                 case 4:
                    ty->termstate.att.fg256 = 1;
                    ty->termstate.att.fg =
                       _handle_esc_csi_truecolor_cmyk(ty, &b);
                    DBG("truecolor CMYK fg: approximation got color %d",
                        ty->termstate.att.fg);
                    break;
                 case 5:
                    // then get next arg - should be color index 0-255
                    arg = _csi_arg_get(ty, &b);
                    if (arg <= -CSI_ARG_ERROR || arg > 255)
                      {
                         ERR("Invalid fg color %d", arg);
                         ty->decoding_error = EINA_TRUE;
                      }
                    else
                      {
                         if (arg == -CSI_ARG_NO_VALUE)
                           arg = 0;
                         ty->termstate.att.fg256 = 1;
                         ty->termstate.att.fg = arg;
                      }
                    break;
                 default:
                    ERR("Failed xterm 256 color fg (got %d)", arg);
                    ty->decoding_error = EINA_TRUE;
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
              arg = _csi_arg_get(ty, &b);
              switch (arg)
                {
                 case -CSI_ARG_ERROR:
                    return;
                   /* TODO: -CSI_ARG_NO_VALUE */
                 case 1:
                    ty->termstate.att.bg256 = 0;
                    ty->termstate.att.bg = COL_INVIS;
                    break;
                 case 2:
                    ty->termstate.att.bg256 = 1;
                    ty->termstate.att.bg =
                       _handle_esc_csi_truecolor_rgb(ty, &b);
                    DBG("truecolor RGB bg: approximation got color %d",
                        ty->termstate.att.bg);
                    break;
                 case 3:
                    ty->termstate.att.bg256 = 1;
                    ty->termstate.att.bg =
                       _handle_esc_csi_truecolor_cmy(ty, &b);
                    DBG("truecolor CMY bg: approximation got color %d",
                        ty->termstate.att.bg);
                    break;
                 case 4:
                    ty->termstate.att.bg256 = 1;
                    ty->termstate.att.bg =
                       _handle_esc_csi_truecolor_cmyk(ty, &b);
                    DBG("truecolor CMYK bg: approximation got color %d",
                        ty->termstate.att.bg);
                    break;
                 case 5:
                    // then get next arg - should be color index 0-255
                    arg = _csi_arg_get(ty, &b);
                    if (arg <= -CSI_ARG_ERROR || arg > 255)
                      {
                         ERR("Invalid bg color %d", arg);
                         ty->decoding_error = EINA_TRUE;
                      }
                    else
                      {
                         if (arg == -CSI_ARG_NO_VALUE)
                           arg = 0;
                         ty->termstate.att.bg256 = 1;
                         ty->termstate.att.bg = arg;
                      }
                    break;
                 default:
                    ERR("Failed xterm 256 color bg (got %d)", arg);
                    ty->decoding_error = EINA_TRUE;
                }
              ty->termstate.att.bgintense = 0;
              break;
           case 49: // default bg color
              ty->termstate.att.bg256 = 0;
              ty->termstate.att.bg = COL_DEF;
              ty->termstate.att.bgintense = 0;
              break;
           case 51:
              WRN("TODO: support SGR 51 - framed attribute");
              ty->termstate.att.framed = 1;
              break;
           case 52:
              ty->termstate.att.encircled = 1;
              break;
           case 53:
              WRN("TODO: support SGR 51 - overlined attribute");
              ty->termstate.att.overlined = 1;
              break;
           case 54:
              ty->termstate.att.framed = 0;
              ty->termstate.att.encircled = 0;
              break;
           case 55:
              ty->termstate.att.overlined = 0;
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
           case 109: // default bg color
              ty->termstate.att.bg256 = 0;
              ty->termstate.att.bg = COL_DEF;
              ty->termstate.att.bgintense = 1;
              break;
           default: //  not handled???
              WRN("Unhandled color cmd [%i]", arg);
              ty->decoding_error = EINA_TRUE;
              break;
          }
     }
}

static void
_handle_esc_csi_cnl(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);
   int max = ty->h;

   if (arg == -CSI_ARG_ERROR)
     return;
   if (arg < 1)
     arg = 1;

   DBG("CNL - Cursor Next Line: %d", arg);
   ty->termstate.wrapnext = 0;
   ty->cursor_state.cy += arg;
   if (ty->termstate.bottom_margin)
     {
        max = ty->termstate.bottom_margin;
     }
   TERMPTY_RESTRICT_FIELD(ty->cursor_state.cy,
                          ty->termstate.top_margin,
                          max);
   ty->cursor_state.cx = ty->termstate.left_margin;
}

static void
_handle_esc_csi_cpl(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);
   int max = ty->h;

   if (arg == -CSI_ARG_ERROR)
     return;
   if (arg < 1)
     arg = 1;

   DBG("CPL - Cursor Previous Line: %d", arg);
   ty->termstate.wrapnext = 0;
   ty->cursor_state.cy -= arg;
   if (ty->termstate.bottom_margin)
     {
        max = ty->termstate.bottom_margin;
     }
   TERMPTY_RESTRICT_FIELD(ty->cursor_state.cy,
                          ty->termstate.top_margin,
                          max);
   ty->cursor_state.cx = ty->termstate.left_margin;
}

static void
_handle_esc_csi_dch(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);
   Termcell *cells;
   int x, lim, max;

   if (arg == -CSI_ARG_ERROR)
     return;

   DBG("DCH - Delete Character: %d chars", arg);

   cells = &(TERMPTY_SCREEN(ty, 0, ty->cursor_state.cy));
   max = ty->w;
   if (ty->termstate.left_margin)
     {
        if (ty->cursor_state.cx < ty->termstate.left_margin)
          return;
     }
   if (ty->termstate.right_margin)
     {
        if (ty->cursor_state.cx > ty->termstate.right_margin)
          return;
        max = ty->termstate.right_margin;
     }
   TERMPTY_RESTRICT_FIELD(arg, 1, max + 1);
   lim = max - arg;
   for (x = ty->cursor_state.cx; x < max; x++)
     {
        if (x < lim)
          TERMPTY_CELL_COPY(ty, &(cells[x + arg]), &(cells[x]), 1);
        else
          {
             cells[x].codepoint = ' ';
             if (EINA_UNLIKELY(cells[x].att.link_id))
               term_link_refcount_dec(ty, cells[x].att.link_id, 1);
             cells[x].att = ty->termstate.att;
             cells[x].att.link_id = 0;
             cells[x].att.dblwidth = 0;
          }
     }
}


static void
_handle_esc_csi_dsr(Termpty *ty, Eina_Unicode *b)
{
   int arg, len;
   char bf[32];
   Eina_Bool question_mark = EINA_FALSE;

   if (*b == '>')
     {
        WRN("TODO: disable key resources used by xterm");
        ty->decoding_error = EINA_TRUE;
        return;
     }
   if (*b == '?')
     {
        question_mark = EINA_TRUE;
        b++;
     }
   arg = _csi_arg_get(ty, &b);
   switch (arg)
     {
      case -CSI_ARG_ERROR:
         return;
      case 5:
         if (question_mark)
           {
              WRN("unhandled DSR (dec specific: %s) %d",
                  (question_mark)? "yes": "no", arg);
              ty->decoding_error = EINA_TRUE;
           }
         else
           {
              /* DSR-OS (Operating Status)
               * Reply Ok */
              termpty_write(ty, "\033[0n",
                            strlen("\033[0n"));
           }
         break;
      case 6:
         DBG("CPR - Cursor Position Report");
           {
              int cx = ty->cursor_state.cx,
                  cy = ty->cursor_state.cy;
              if (ty->termstate.restrict_cursor)
                {
                   if (ty->termstate.top_margin)
                     cy -= ty->termstate.top_margin;
                   if (ty->termstate.left_margin)
                     cx -= ty->termstate.left_margin;
                }
              if (question_mark)
                {
                   len = snprintf(bf, sizeof(bf), "\033[?%d;%d;1R",
                                  cy + 1,
                                  cx + 1);
                }
              else
                {
                   len = snprintf(bf, sizeof(bf), "\033[%d;%dR",
                                  cy + 1,
                                  cx + 1);
                }
              termpty_write(ty, bf, len);
           }
         break;
      case 15:
         if (question_mark)
           {
              /* DSR-PP (Printer Port)
               * Reply None */
              termpty_write(ty, "\033[?13n",
                            strlen("\033[?13n"));
           }
         else
           {
              WRN("unhandled DSR (dec specific: %s) %d",
                  (question_mark)? "yes": "no", arg);
              ty->decoding_error = EINA_TRUE;
           }
         break;
      case 25:
         if (question_mark)
           {
              /* DSR-UDK (User-Defined Keys)
               * Reply Unlocked */
              termpty_write(ty, "\033[?20n",
                            strlen("\033[?20n"));
           }
         else
           {
              WRN("unhandled DSR (dec specific: %s) %d",
                  (question_mark)? "yes": "no", arg);
              ty->decoding_error = EINA_TRUE;
           }
         break;
      case 26:
         if (question_mark)
           {
              /* DSR-KBD (Keyboard)
               * Reply North American */
              termpty_write(ty, "\033[?27;1;0;0n",
                            strlen("\033[?27;1;0;0n"));
           }
         else
           {
              WRN("unhandled DSR (dec specific: %s) %d",
                  (question_mark)? "yes": "no", arg);
              ty->decoding_error = EINA_TRUE;
           }
         break;
      case 62:
         if (question_mark)
           {
              /* DSR-MSR (Macro Space Report)
               * Reply 0 */
              termpty_write(ty, "\033[0000*{",
                            strlen("\033[0000*{"));
           }
         else
           {
              WRN("unhandled DSR (dec specific: %s) %d",
                  (question_mark)? "yes": "no", arg);
              ty->decoding_error = EINA_TRUE;
           }
         break;
      case 63:
         if (question_mark)
           {
              /* DSR-DECCKSR (Memory Checksum) */
              int pid = _csi_arg_get(ty, &b);
              len = snprintf(bf, sizeof(bf), "\033P%u!~0000\033\\", pid);
              termpty_write(ty, bf, len);
           }
         else
           {
              WRN("unhandled DSR (dec specific: %s) %d",
                  (question_mark)? "yes": "no", arg);
              ty->decoding_error = EINA_TRUE;
           }
         break;
      case 75:
         if (question_mark)
           {
              /* DSR-DIR (Data Integrity Report) */
              termpty_write(ty, "\033[?70n", strlen("\033[?70n"));
           }
         else
           {
              WRN("unhandled DSR (dec specific: %s) %d",
                  (question_mark)? "yes": "no", arg);
              ty->decoding_error = EINA_TRUE;
           }
         break;
      /* TODO: -CSI_ARG_NO_VALUE */
      default:
         WRN("unhandled DSR (dec specific: %s) %d",
             (question_mark)? "yes": "no", arg);
         ty->decoding_error = EINA_TRUE;
         break;
     }
}

static void
_handle_esc_csi_decslrm(Termpty *ty, Eina_Unicode **b)
{
  int left = _csi_arg_get(ty, b);
  int right = _csi_arg_get(ty, b);

  DBG("DECSLRM (%d;%d) Set Left and Right Margins", left, right);
  if ((left == -CSI_ARG_ERROR) || (right == -CSI_ARG_ERROR))
    goto bad;

  TERMPTY_RESTRICT_FIELD(left, 1, ty->w);
  if (right < 1)
    right = ty->w;
  TERMPTY_RESTRICT_FIELD(right, 3, ty->w+1);

  if (left >= right)
    goto bad;
  if (right - left < 2)
    goto bad;

  if (right == ty->w)
    right = 0;

  ty->termstate.left_margin = left  - 1;
  ty->termstate.right_margin = right;
  _move_cursor_to_origin(ty);

  return;

bad:
  ty->termstate.left_margin = 0;
  ty->termstate.right_margin = 0;
}

static void
_handle_esc_csi_decstbm(Termpty *ty, Eina_Unicode **b)
{
  int top = _csi_arg_get(ty, b);
  int bottom = _csi_arg_get(ty, b);

  DBG("DECSTBM (%d;%d) Set Top and Bottom Margins", top, bottom);
  if ((top == -CSI_ARG_ERROR) || (bottom == -CSI_ARG_ERROR))
    goto bad;

  TERMPTY_RESTRICT_FIELD(top, 1, ty->h);
  TERMPTY_RESTRICT_FIELD(bottom, 1, ty->h+1);

  if (top >= bottom) goto bad;

  if (bottom == ty->h)
    bottom = 0;

  ty->termstate.top_margin = top - 1;
  ty->termstate.bottom_margin = bottom;

  _move_cursor_to_origin(ty);
  return;

bad:
  ty->termstate.top_margin = 0;
  ty->termstate.bottom_margin = 0;
}

static int
_clean_up_rect_coordinates(Termpty *ty,
                           int *top_ptr,
                           int *left_ptr,
                           int *bottom_ptr,
                           int *right_ptr)
{
   int top = *top_ptr;
   int left = *left_ptr;
   int bottom = *bottom_ptr;
   int right = *right_ptr;

   TERMPTY_RESTRICT_FIELD(top, 1, ty->h);
   if (ty->termstate.restrict_cursor)
     {
        top += ty->termstate.top_margin;
        if (ty->termstate.bottom_margin && top >= ty->termstate.bottom_margin)
          top = ty->termstate.bottom_margin;
     }
   top--;

   TERMPTY_RESTRICT_FIELD(left, 1, ty->w);
   if (ty->termstate.restrict_cursor)
     {
        left += ty->termstate.left_margin;
        if (ty->termstate.right_margin && left >= ty->termstate.right_margin)
          left = ty->termstate.right_margin;
     }
   left--;

   if (right < 1)
     right = ty->w;
   if (ty->termstate.restrict_cursor)
     {
        right += ty->termstate.left_margin;
        if (ty->termstate.right_margin && right >= ty->termstate.right_margin)
          right = ty->termstate.right_margin;
     }
   if (right > ty->w)
     right = ty->w;

   if (bottom < 1)
     bottom = ty->h;
   if (ty->termstate.restrict_cursor)
     {
        bottom += ty->termstate.top_margin;
        if (ty->termstate.bottom_margin && bottom >= ty->termstate.bottom_margin)
          bottom = ty->termstate.bottom_margin;
     }
   bottom--;
   if (bottom > ty->h)
     bottom = ty->h - 1;

   if ((bottom < top) || (right < left))
     return -1;

   *top_ptr = top;
   *left_ptr = left;
   *bottom_ptr = bottom;
   *right_ptr = right;

   return 0;
}

static int
_clean_up_from_to_coordinates(Termpty *ty,
                              int *top_ptr,
                              int *left_ptr,
                              int *bottom_ptr,
                              int *right_ptr,
                              int *left_border,
                              int *right_border)
{
   int top = *top_ptr;
   int left = *left_ptr;
   int bottom = *bottom_ptr;
   int right = *right_ptr;

   TERMPTY_RESTRICT_FIELD(top, 1, ty->h);
   if (ty->termstate.restrict_cursor)
     {
        top += ty->termstate.top_margin;
        if (ty->termstate.bottom_margin && top >= ty->termstate.bottom_margin)
          top = ty->termstate.bottom_margin;
     }
   top--;

   TERMPTY_RESTRICT_FIELD(left, 1, ty->w);
   if (ty->termstate.restrict_cursor)
     {
        left += ty->termstate.left_margin;
        if (ty->termstate.right_margin && left >= ty->termstate.right_margin)
          left = ty->termstate.right_margin;
     }
   left--;

   if (right < 1)
     right = ty->w;
   if (ty->termstate.restrict_cursor)
     {
        right += ty->termstate.left_margin;
        if (ty->termstate.right_margin && right >= ty->termstate.right_margin)
          right = ty->termstate.right_margin;
     }
   if (right > ty->w)
     right = ty->w;

   if (bottom < 1)
     bottom = ty->h;
   if (ty->termstate.restrict_cursor)
     {
        bottom += ty->termstate.top_margin;
        if (ty->termstate.bottom_margin && bottom >= ty->termstate.bottom_margin)
          bottom = ty->termstate.bottom_margin;
     }
   bottom--;
   if (bottom > ty->h)
     bottom = ty->h - 1;

   if ((bottom == top) && (right < left))
     return -1;

   *left_border = 0;
   *right_border = ty->w-1;
   if (ty->termstate.restrict_cursor)
     {
        *left_border = ty->termstate.left_margin;
        *right_border = ty->termstate.right_margin;
     }

   *top_ptr = top;
   *left_ptr = left;
   *bottom_ptr = bottom;
   *right_ptr = right;

   return 0;
}

static void
_handle_esc_csi_decfra(Termpty *ty, Eina_Unicode **b)
{
   int c = _csi_arg_get(ty, b);

   int top = _csi_arg_get(ty, b);
   int left = _csi_arg_get(ty, b);
   int bottom = _csi_arg_get(ty, b);
   int right = _csi_arg_get(ty, b);
   int len;

   DBG("DECFRA (%d; %d;%d;%d;%d) Fill Rectangular Area",
       c, top, left, bottom, right);
   if ((c == -CSI_ARG_ERROR) ||
       (c == -CSI_ARG_NO_VALUE) ||
       (top == -CSI_ARG_ERROR) ||
       (left == -CSI_ARG_ERROR) ||
       (bottom == -CSI_ARG_ERROR) ||
       (right == -CSI_ARG_ERROR))
     return;

   if (! ((c >= 32 && c <= 126) || (c >= 160 && c <= 255)))
     return;

   if (_clean_up_rect_coordinates(ty, &top, &left, &bottom, &right) < 0)
     return;

   len = right - left;

   for (; top <= bottom; top++)
     {
        Termcell *cells = &(TERMPTY_SCREEN(ty, left, top));
        termpty_cells_att_fill_preserve_colors(ty, cells, c, len);
     }
}

static void
_deccara(Termcell *cells, int len,
         Eina_Bool set_bold, Eina_Bool reset_bold,
         Eina_Bool set_underline, Eina_Bool reset_underline,
         Eina_Bool set_blink, Eina_Bool reset_blink,
         Eina_Bool set_inverse, Eina_Bool reset_inverse)
{
   int i;

   for (i = 0; i < len; i++)
     {
        Termatt * att = &cells[i].att;
        if (set_bold)
          att->bold = 1;
        if (set_underline)
          att->underline = 1;
        if (set_blink)
          att->blink = 1;
        if (set_inverse)
          att->inverse = 1;
        if (reset_bold)
          att->bold = 0;
        if (reset_underline)
          att->underline = 0;
        if (reset_blink)
          att->blink = 0;
        if (reset_inverse)
          att->inverse = 0;
     }
}

static void
_handle_esc_csi_deccara(Termpty *ty, Eina_Unicode **ptr,
                        const Eina_Unicode * const end)
{
   Eina_Unicode *b = *ptr;
   Termcell *cells;
   int top;
   int left;
   int bottom;
   int right;
   int len;
   Eina_Bool set_bold = EINA_FALSE, reset_bold = EINA_FALSE;
   Eina_Bool set_underline = EINA_FALSE, reset_underline = EINA_FALSE;
   Eina_Bool set_blink = EINA_FALSE, reset_blink = EINA_FALSE;
   Eina_Bool set_inverse = EINA_FALSE, reset_inverse = EINA_FALSE;

   top = _csi_arg_get(ty, &b);
   left = _csi_arg_get(ty, &b);
   bottom = _csi_arg_get(ty, &b);
   right = _csi_arg_get(ty, &b);

   DBG("DECCARA (%d;%d;%d;%d) Change Attributes in Rectangular Area",
       top, left, bottom, right);
   if ((top == -CSI_ARG_ERROR) ||
       (left == -CSI_ARG_ERROR) ||
       (bottom == -CSI_ARG_ERROR) ||
       (right == -CSI_ARG_ERROR))
     return;

   while (b && b < end)
     {
        int arg = _csi_arg_get(ty, &b);
        switch (arg)
          {
           case -CSI_ARG_ERROR:
              return;
           case -CSI_ARG_NO_VALUE:
              EINA_FALLTHROUGH;
           case 0:
              set_bold = set_underline = set_blink = set_inverse = EINA_FALSE;
              reset_bold = reset_underline = reset_blink = reset_inverse = EINA_TRUE;
              break;
           case 1:
              set_bold = EINA_TRUE;
              reset_bold = EINA_FALSE;
              break;
           case 4:
              set_underline = EINA_TRUE;
              reset_underline = EINA_FALSE;
              break;
           case 5:
              set_blink = EINA_TRUE;
              reset_blink = EINA_FALSE;
              break;
           case 7:
              set_inverse = EINA_TRUE;
              reset_inverse = EINA_FALSE;
              break;
           case 22:
              set_bold = EINA_FALSE;
              reset_bold = EINA_TRUE;
              break;
           case 24:
              set_underline = EINA_FALSE;
              reset_underline = EINA_TRUE;
              break;
           case 25:
              set_blink = EINA_FALSE;
              reset_blink = EINA_TRUE;
              break;
           case 27:
              set_inverse = EINA_FALSE;
              reset_inverse = EINA_TRUE;
              break;
           default:
              WRN("Invalid change attribute [%i]", arg);
              ty->decoding_error = EINA_TRUE;
              return;
          }
     }

   if (ty->termstate.sace_rectangular)
     {
        if (_clean_up_rect_coordinates(ty, &top, &left, &bottom, &right) < 0)
          return;

        len = right - left;

        for (; top <= bottom; top++)
          {
             cells = &(TERMPTY_SCREEN(ty, left, top));
             _deccara(cells, len, set_bold, reset_bold, set_underline,
                      reset_underline, set_blink, reset_blink, set_inverse,
                      reset_inverse);
          }
     }
   else
     {
        int left_border = 0;
        int right_border = ty->w - 1;

        if (_clean_up_from_to_coordinates(ty, &top, &left, &bottom, &right,
                                          &left_border, &right_border) < 0)
          return;
        if (top == bottom)
          {
             cells = &(TERMPTY_SCREEN(ty, left, top));
             len = right - left;
             _deccara(cells, len, set_bold, reset_bold,
                      set_underline, reset_underline,
                      set_blink, reset_blink,
                      set_inverse, reset_inverse);
          }
        else
          {
             /* First line */
             cells = &(TERMPTY_SCREEN(ty, left, top));
             len = right_border - left;
             _deccara(cells, len, set_bold, reset_bold,
                      set_underline, reset_underline,
                      set_blink, reset_blink,
                      set_inverse, reset_inverse);

             /* Middle */
             len = right_border - left_border;
             for (top = top + 1; top < bottom; top++)
               {
                  cells = &(TERMPTY_SCREEN(ty, left_border, top));
                  _deccara(cells, len, set_bold, reset_bold,
                           set_underline, reset_underline,
                           set_blink, reset_blink,
                           set_inverse, reset_inverse);
               }

             /* Last line */
             cells = &(TERMPTY_SCREEN(ty, left_border, bottom));
             len = right - left_border;
             _deccara(cells, len, set_bold, reset_bold,
                      set_underline, reset_underline,
                      set_blink, reset_blink,
                      set_inverse, reset_inverse);
          }
     }
}

static void
_decrara(Termcell *cells, int len,
         Eina_Bool reverse_bold,
         Eina_Bool reverse_underline,
         Eina_Bool reverse_blink,
         Eina_Bool reverse_inverse)
{
   int i;

   for (i = 0; i < len; i++)
     {
        Termatt * att = &cells[i].att;
        if (reverse_bold)
          att->bold = !att->bold;
        if (reverse_underline)
          att->underline = !att->underline;
        if (reverse_blink)
          att->blink = !att->blink;
        if (reverse_inverse)
          att->inverse = !att->inverse;
     }
}

static void
_handle_esc_csi_decrara(Termpty *ty, Eina_Unicode **ptr,
                        const Eina_Unicode * const end)
{
   Eina_Unicode *b = *ptr;
   Termcell *cells;
   int top;
   int left;
   int bottom;
   int right;
   int len;
   Eina_Bool reverse_bold = EINA_FALSE;
   Eina_Bool reverse_underline = EINA_FALSE;
   Eina_Bool reverse_blink = EINA_FALSE;
   Eina_Bool reverse_inverse = EINA_FALSE;

   top = _csi_arg_get(ty, &b);
   left = _csi_arg_get(ty, &b);
   bottom = _csi_arg_get(ty, &b);
   right = _csi_arg_get(ty, &b);

   DBG("DECRARA (%d;%d;%d;%d) Reverse Attributes in Rectangular Area",
       top, left, bottom, right);
   if ((top == -CSI_ARG_ERROR) ||
       (left == -CSI_ARG_ERROR) ||
       (bottom == -CSI_ARG_ERROR) ||
       (right == -CSI_ARG_ERROR))
     return;

   while (b && b < end)
     {
        int arg = _csi_arg_get(ty, &b);
        switch (arg)
          {
           case -CSI_ARG_ERROR:
              return;
           case -CSI_ARG_NO_VALUE:
              EINA_FALLTHROUGH;
           case 0:
              reverse_bold = reverse_underline = reverse_blink = reverse_inverse = EINA_TRUE;
              break;
           case 1:
              reverse_bold = EINA_TRUE;
              break;
           case 4:
              reverse_underline = EINA_TRUE;
              break;
           case 5:
              reverse_blink = EINA_TRUE;
              break;
           case 7:
              reverse_inverse = EINA_TRUE;
              break;
           default:
              WRN("Invalid change attribute [%i]", arg);
              ty->decoding_error = EINA_TRUE;
              return;
          }
     }

   if (ty->termstate.sace_rectangular)
     {
        if (_clean_up_rect_coordinates(ty, &top, &left, &bottom, &right) < 0)
          return;

        len = right - left;

        for (; top <= bottom; top++)
          {
             cells = &(TERMPTY_SCREEN(ty, left, top));
             _decrara(cells, len, reverse_bold, reverse_underline,
                      reverse_blink, reverse_inverse);
          }
     }
   else
     {
        int left_border = 0;
        int right_border = ty->w - 1;

        if (_clean_up_from_to_coordinates(ty, &top, &left, &bottom, &right,
                                          &left_border, &right_border) < 0)
          return;
        if (top == bottom)
          {
             cells = &(TERMPTY_SCREEN(ty, left, top));
             len = right - left;
             _decrara(cells, len, reverse_bold, reverse_underline,
                      reverse_blink, reverse_inverse);
          }
        else
          {
             /* First line */
             cells = &(TERMPTY_SCREEN(ty, left, top));
             len = right_border - left;
             _decrara(cells, len, reverse_bold, reverse_underline,
                      reverse_blink, reverse_inverse);

             /* Middle */
             len = right_border - left_border;
             for (top = top + 1; top < bottom; top++)
               {
                  cells = &(TERMPTY_SCREEN(ty, left_border, top));
                  _decrara(cells, len, reverse_bold, reverse_underline,
                           reverse_blink, reverse_inverse);
               }

             /* Last line */
             cells = &(TERMPTY_SCREEN(ty, left_border, bottom));
             len = right - left_border;
             _decrara(cells, len, reverse_bold, reverse_underline,
                      reverse_blink, reverse_inverse);
          }
     }
}

static void
_handle_esc_csi_decera(Termpty *ty, Eina_Unicode **b)
{
   int top = _csi_arg_get(ty ,b);
   int left = _csi_arg_get(ty, b);
   int bottom = _csi_arg_get(ty, b);
   int right = _csi_arg_get(ty, b);
   int len;

   DBG("DECERA (%d;%d;%d;%d) Erase Rectangular Area",
       top, left, bottom, right);

   if ((top == -CSI_ARG_ERROR) ||
       (left == -CSI_ARG_ERROR) ||
       (bottom == -CSI_ARG_ERROR) ||
       (right == -CSI_ARG_ERROR))
     return;

   if (_clean_up_rect_coordinates(ty, &top, &left, &bottom, &right) < 0)
     return;

   len = right - left;
   for (; top <= bottom; top++)
     {
        Termcell *cells = &(TERMPTY_SCREEN(ty, left, top));
        termpty_cells_set_content(ty, cells, ' ', len);
     }
}

static void
_handle_esc_csi_deccra(Termpty *ty, Eina_Unicode **b)
{
   int top = _csi_arg_get(ty, b);
   int left = _csi_arg_get(ty, b);
   int bottom = _csi_arg_get(ty, b);
   int right = _csi_arg_get(ty, b);
   int p1 = _csi_arg_get(ty, b);
   int to_top = _csi_arg_get(ty, b);
   int to_left = _csi_arg_get(ty, b);
   int p2 = _csi_arg_get(ty, b);
   int to_bottom = ty->h - 1;
   int to_right = ty->w;
   int len;

   DBG("DECFRA (%d;%d;%d;%d -> %d;%d) Copy Rectangular Area",
       top, left, bottom, right, to_top, to_left);
   if ((top == -CSI_ARG_ERROR) ||
       (left == -CSI_ARG_ERROR) ||
       (bottom == -CSI_ARG_ERROR) ||
       (right == -CSI_ARG_ERROR) ||
       (p1 == -CSI_ARG_ERROR) ||
       (to_top == -CSI_ARG_ERROR) ||
       (to_left == -CSI_ARG_ERROR) ||
       (p2 == -CSI_ARG_ERROR))
     return;

   if (_clean_up_rect_coordinates(ty, &top, &left, &bottom, &right) < 0)
     return;

   TERMPTY_RESTRICT_FIELD(to_top, 1, ty->h);
   if (ty->termstate.restrict_cursor)
     {
        to_top += ty->termstate.top_margin;
        if (ty->termstate.bottom_margin)
          {
             if (to_top >= ty->termstate.bottom_margin)
               to_top = ty->termstate.bottom_margin;
             to_bottom = ty->termstate.bottom_margin - 1;
          }
     }
   to_top--;
   if (to_bottom - to_top > bottom - top)
     to_bottom = to_top + bottom - top;
   TERMPTY_RESTRICT_FIELD(to_left, 1, ty->w);
   if (ty->termstate.restrict_cursor)
     {
        to_left += ty->termstate.left_margin;
        if (ty->termstate.right_margin)
          {
             if (to_left >= ty->termstate.right_margin)
               to_left = ty->termstate.right_margin;
             to_right = ty->termstate.right_margin;
          }
     }
   to_left--;

   len = MIN(right - left, to_right - to_left);

   if (to_top < top)
     {
        /* Up -> Bottom */
        for (; top <= bottom && to_top <= to_bottom; top++, to_top++)
          {
             Termcell *cells_src = &(TERMPTY_SCREEN(ty, left, top));
             Termcell *cells_dst = &(TERMPTY_SCREEN(ty, to_left, to_top));
             int x;
             if (to_left <= left)
               {
                  /* -> */
                  for (x = 0; x < len; x++)
                    {
                       if (&(cells_src[x]) != &(cells_dst[x]))
                         {
                            TERMPTY_CELL_COPY(ty, &(cells_src[x]), &(cells_dst[x]), 1);
                         }
                    }
               }
             else
               {
                  /* <- */
                  for (x = len - 1; x >= 0; x--)
                    {
                       if (&(cells_src[x]) != &(cells_dst[x]))
                         {
                            TERMPTY_CELL_COPY(ty, &(cells_src[x]), &(cells_dst[x]), 1);
                         }
                    }
               }
          }
     }
   else
     {
        /* Bottom -> Up */
        for (; bottom >= top && to_bottom >= to_top; bottom--, to_bottom--)
          {
             Termcell *cells_src = &(TERMPTY_SCREEN(ty, left, bottom));
             Termcell *cells_dst = &(TERMPTY_SCREEN(ty, to_left, to_bottom));
             int x;
             if (to_left <= left)
               {
                  /* -> */
                  for (x = 0; x < len; x++)
                    {
                       if (&(cells_src[x]) != &(cells_dst[x]))
                         {
                            TERMPTY_CELL_COPY(ty, &(cells_src[x]), &(cells_dst[x]), 1);
                         }
                    }
               }
             else
               {
                  /* <- */
                  for (x = len - 1; x >= 0; x--)
                    {
                       if (&(cells_src[x]) != &(cells_dst[x]))
                         {
                            TERMPTY_CELL_COPY(ty, &(cells_src[x]), &(cells_dst[x]), 1);
                         }
                    }
               }
          }
     }
}

static void
_handle_esc_csi_cursor_pos_set(Termpty *ty, Eina_Unicode **b,
                               const Eina_Unicode *cc)
{
   int cx = 0, cy = 0;
   ty->termstate.wrapnext = 0;
   cy = _csi_arg_get(ty, b);
   cx = _csi_arg_get(ty, b);

   if ((cx == -CSI_ARG_ERROR) || (cy == -CSI_ARG_ERROR))
     return;

   DBG("cursor pos set (%s) (%d;%d)", (*cc == 'H') ? "CUP" : "HVP",
       cx, cy);
   cx--;
   if (cx < 0)
     cx = 0;
   if (ty->termstate.restrict_cursor)
     {
        cx += ty->termstate.left_margin;
        if (ty->termstate.right_margin > 0 && cx >= ty->termstate.right_margin)
          cx = ty->termstate.right_margin - 1;
     }
   if (cx >= ty->w)
     cx = ty->w -1;
   ty->cursor_state.cx = cx;


   cy--;
   if (cy < 0)
     cy = 0;
   if (ty->termstate.restrict_cursor)
     {
        cy += ty->termstate.top_margin;
        if (ty->termstate.bottom_margin > 0 && cy >= ty->termstate.bottom_margin)
          cy = ty->termstate.bottom_margin - 1;
     }
   if (cy >= ty->h)
     cy = ty->h - 1;
   ty->cursor_state.cy = cy;
}

static void
_handle_esc_csi_decscusr(Termpty *ty, Eina_Unicode **b)
{
  int arg = _csi_arg_get(ty, b);
  Cursor_Shape shape = CURSOR_SHAPE_BLOCK;

  DBG("DECSCUSR (%d) Set Cursor Shape", arg);

  switch (arg)
    {
     case -CSI_ARG_ERROR:
        return;
     case -CSI_ARG_NO_VALUE:
        EINA_FALLTHROUGH;
     case 0:
        EINA_FALLTHROUGH;
     case 1:
        EINA_FALLTHROUGH;
     case 2:
        shape = CURSOR_SHAPE_BLOCK;
        break;
     case 3:
        EINA_FALLTHROUGH;
     case 4:
        shape = CURSOR_SHAPE_UNDERLINE;
        break;
     case 5:
        EINA_FALLTHROUGH;
     case 6:
        shape = CURSOR_SHAPE_BAR;
        break;
     default:
        WRN("Invalid DECSCUSR %d", arg);
        ty->decoding_error = EINA_TRUE;
        return;
    }

  termio_set_cursor_shape(ty->obj, shape);
}

static void
_handle_esc_csi_decsace(Termpty *ty, Eina_Unicode **b)
{
  int arg = _csi_arg_get(ty, b);

  DBG("DECSACE (%d) Select Attribute Change Extent", arg);

  switch (arg)
    {
     case -CSI_ARG_ERROR:
        return;
     case -CSI_ARG_NO_VALUE:
        EINA_FALLTHROUGH;
     case 0:
        EINA_FALLTHROUGH;
     case 1:
        ty->termstate.sace_rectangular = 0;
        break;
     case 2:
        ty->termstate.sace_rectangular = 1;
        break;
     default:
        WRN("Invalid DECSACE %d", arg);
        ty->decoding_error = EINA_TRUE;
        return;
    }
}

static void
_handle_esc_csi_decic(Termpty *ty, Eina_Unicode **b)
{
   int arg = _csi_arg_get(ty, b);
   int old_insert = ty->termstate.insert;
   Eina_Unicode blank[1] = { ' ' };
   int top = 0;
   int bottom = ty->h;
   int old_cx = ty->cursor_state.cx;
   int old_cy = ty->cursor_state.cy;

   DBG("DECIC Insert %d Column", arg);

   if (arg == -CSI_ARG_ERROR)
     return;
   if (arg < 1)
     arg = 1;
   if (ty->termstate.top_margin > 0)
     {
        if (ty->cursor_state.cy < ty->termstate.top_margin)
          return;
        top = ty->termstate.top_margin;
     }
   if (ty->termstate.bottom_margin != 0)
     {
        if (ty->cursor_state.cy >= ty->termstate.bottom_margin)
          return;
        bottom = ty->termstate.bottom_margin;
     }

   if (((ty->termstate.left_margin > 0)
        && (ty->cursor_state.cx < ty->termstate.left_margin))
       || ((ty->termstate.right_margin > 0)
           && (ty->cursor_state.cx >= ty->termstate.right_margin)))
     return;

   ty->termstate.insert = 1;
   for (; top < bottom; top++)
     {
        int i;

        /* Insert a left column */
        ty->cursor_state.cy = top;
        ty->cursor_state.cx = old_cx;
        ty->termstate.wrapnext = 0;
        for (i = 0; i < arg; i++)
          termpty_text_append(ty, blank, 1);
     }
   ty->termstate.insert = old_insert;
   ty->cursor_state.cx = old_cx;
   ty->cursor_state.cy = old_cy;
}

static void
_handle_esc_csi_decdc(Termpty *ty, Eina_Unicode **b)
{
   int arg = _csi_arg_get(ty, b);
   int y = 0;
   int max_y = ty->h;
   int max_x = ty->w;
   int lim;

   DBG("DECDC Delete %d Column", arg);

   if (arg == -CSI_ARG_ERROR)
     return;
   if (arg < 1)
     arg = 1;
   if (ty->termstate.top_margin > 0)
     {
        if (ty->cursor_state.cy < ty->termstate.top_margin)
          return;
        y = ty->termstate.top_margin;
     }
   if (ty->termstate.bottom_margin != 0)
     {
        if (ty->cursor_state.cy >= ty->termstate.bottom_margin)
          return;
        max_y = ty->termstate.bottom_margin;
     }

   if (ty->termstate.right_margin > 0)
     {
        if (ty->cursor_state.cx >= ty->termstate.right_margin)
          return;
        max_x = ty->termstate.right_margin;
     }

   if (((ty->termstate.left_margin > 0)
        && (ty->cursor_state.cx < ty->termstate.left_margin)))
     return;

   TERMPTY_RESTRICT_FIELD(arg, 1, max_x + 1);
   lim = max_x - arg;
   for (; y < max_y; y++)
     {
        int x;
        Termcell *cells = &(TERMPTY_SCREEN(ty, 0, y));

        for (x = ty->cursor_state.cx; x < max_x; x++)
          {
             if (x < lim)
               TERMPTY_CELL_COPY(ty, &(cells[x + arg]), &(cells[x]), 1);
             else
               {
                  cells[x].codepoint = ' ';
                  if (EINA_UNLIKELY(cells[x].att.link_id))
                    term_link_refcount_dec(ty, cells[x].att.link_id, 1);
                  cells[x].att = ty->termstate.att;
                  cells[x].att.link_id = 0;
                  cells[x].att.dblwidth = 0;
               }
          }
     }
}

static void
_handle_esc_csi_ich(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode blank[1] = { ' ' };
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);
   int i;
   int old_insert = ty->termstate.insert;
   int old_cx = ty->cursor_state.cx;
   int max = ty->w;

   if (arg == -CSI_ARG_ERROR)
     return;
   TERMPTY_RESTRICT_FIELD(arg, 1, ty->w * ty->h);

   DBG("ICH - Insert %d Characters", arg);

   if (ty->termstate.lr_margins)
     {
        if ((ty->termstate.left_margin)
            && (ty->cursor_state.cx < ty->termstate.left_margin))
          {
             return;
          }
        if (ty->termstate.right_margin)
          {
             if (ty->cursor_state.cx >= ty->termstate.right_margin)
               {
                  return;
               }
             max = ty->termstate.right_margin;
          }
     }

   if (ty->cursor_state.cx + arg > max)
     {
        arg = max - ty->cursor_state.cx;
     }

   ty->termstate.wrapnext = 0;
   ty->termstate.insert = 1;
   for (i = 0; i < arg; i++)
     termpty_text_append(ty, blank, 1);
   ty->termstate.insert = old_insert;
   ty->cursor_state.cx = old_cx;
}

static void
_handle_esc_csi_cuu(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);

   if (arg == -CSI_ARG_ERROR)
     return;
   if (arg < 1)
     arg = 1;
   DBG("CUU - Cursor Up %d", arg);
   ty->termstate.wrapnext = 0;
   ty->cursor_state.cy = MAX(0, ty->cursor_state.cy - arg);
   TERMPTY_RESTRICT_FIELD(ty->cursor_state.cy, 0, ty->h);
   if (ty->termstate.restrict_cursor && (ty->termstate.top_margin > 0)
       && (ty->cursor_state.cy < ty->termstate.top_margin))
     {
        ty->cursor_state.cy = ty->termstate.top_margin;
     }
}

static void
_handle_esc_csi_cud(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);

   if (arg == -CSI_ARG_ERROR)
     return;

   if (arg < 1)
     arg = 1;

   DBG("CUD - Cursor Down %d", arg);
   ty->termstate.wrapnext = 0;
   ty->cursor_state.cy = MIN(ty->h - 1, ty->cursor_state.cy + arg);
   TERMPTY_RESTRICT_FIELD(ty->cursor_state.cy, 0, ty->h);
   if (ty->termstate.restrict_cursor && (ty->termstate.bottom_margin > 0)
       && (ty->cursor_state.cy >= ty->termstate.bottom_margin))
     {
        ty->cursor_state.cy = ty->termstate.bottom_margin - 1;
     }
}

static void
_handle_esc_csi_cuf(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);

   if (arg == -CSI_ARG_ERROR)
     return;
   if (arg < 1)
     arg = 1;
   DBG("CUF - Cursor Forward %d", arg);
   ty->termstate.wrapnext = 0;
   ty->cursor_state.cx += arg;
   TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
   if (ty->termstate.restrict_cursor && (ty->termstate.right_margin != 0)
       && (ty->cursor_state.cx >= ty->termstate.right_margin))
     {
        ty->cursor_state.cx = ty->termstate.right_margin - 1;
     }
}

static void
_handle_esc_csi_cub(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);

   if (arg == -CSI_ARG_ERROR)
     return;
   if (arg < 1)
     arg = 1;
   DBG("CUB - Cursor Backward %d", arg);
   ty->termstate.wrapnext = 0;
   ty->cursor_state.cx -= arg;
   TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
   if (ty->termstate.restrict_cursor && (ty->termstate.left_margin != 0)
       && (ty->cursor_state.cx < ty->termstate.left_margin))
     {
        ty->cursor_state.cx = ty->termstate.left_margin;
     }
}

static void
_handle_esc_csi_cha(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);
   int min = 0;
   int max = ty->w;

   DBG("CHA - Cursor Horizontal Absolute: %d", arg);
   if (arg == -CSI_ARG_ERROR)
     return;
   if (arg < 1)
     arg = 1;
   ty->termstate.wrapnext = 0;
   if (ty->termstate.restrict_cursor)
     {
        if (ty->termstate.left_margin)
          {
             arg += ty->termstate.left_margin;
          }
        if (ty->termstate.right_margin)
          {
             max = ty->termstate.right_margin;
          }
     }
   ty->cursor_state.cx = arg - 1;
   TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, min, max);
}

static void
_handle_esc_csi_cht(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);

   if (arg == -CSI_ARG_ERROR)
     return;
   TERMPTY_RESTRICT_FIELD(arg, 1, ty->w);
   DBG("CHT - Cursor Forward Tabulation: %d", arg);
   _tab_forward(ty, arg);
}

static int
_handle_esc_csi(Termpty *ty, const Eina_Unicode *c, const Eina_Unicode *ce)
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
   be = b;
   b = buf;
   DBG(" CSI: '%s' args '%s'", _safechar(*cc), (char *) buf);
   switch (*cc)
     {
        /* sorted by ascii value */
      case '@':
         /* TODO: SL */
         _handle_esc_csi_ich(ty, &b);
        break;
      case 'A':
        /* TODO: SR */
         _handle_esc_csi_cuu(ty, &b);
        break;
      case 'B':
        _handle_esc_csi_cud(ty, &b);
        break;
      case 'C':
        _handle_esc_csi_cuf(ty, &b);
        break;
      case 'D':
        _handle_esc_csi_cub(ty, &b);
        break;
      case 'E':
        _handle_esc_csi_cnl(ty, &b);
        break;
      case 'F':
        _handle_esc_csi_cpl(ty, &b);
        break;
      case 'G':
        _handle_esc_csi_cha(ty, &b);
        break;
      case 'H':
        _handle_esc_csi_cursor_pos_set(ty, &b, cc);
        break;
      case 'I':
        _handle_esc_csi_cht(ty, &b);
        break;
      case 'J':
        if (*b == '?')
          {
             b++;
             arg = _csi_arg_get(ty, &b);
             if (arg == -CSI_ARG_ERROR)
               goto error;
             WRN("Unsupported selected erase in display %d", arg);
             ty->decoding_error = EINA_TRUE;
             break;
          }
        else
          {
             arg = _csi_arg_get(ty, &b);
             if (arg == -CSI_ARG_ERROR)
               goto error;
          }
        if (arg < 1)
          arg = 0;
        /* 3J erases the backlog,
         * 2J erases the screen,
         * 1J erase from screen start to cursor,
         * 0J erase form cursor to end of screen
         */
        DBG("ED/DECSED %d: erase in display", arg);
        switch (arg)
          {
           case TERMPTY_CLR_END /* 0 */:
           case TERMPTY_CLR_BEGIN /* 1 */:
           case TERMPTY_CLR_ALL /* 2 */:
              termpty_clear_screen(ty, arg);
              break;
           case 3:
              termpty_clear_backlog(ty);
              break;
           default:
              ERR("invalid EL/DECSEL argument %d", arg);
              ty->decoding_error = EINA_TRUE;
          }
        break;
      case 'K': // 0K erase to end of line, 1K erase from screen start to cursor, 2K erase all of line
        if (*b == '?')
          {
             b++;
             arg = _csi_arg_get(ty, &b);
             if (arg == -CSI_ARG_ERROR)
               goto error;
             WRN("Unsupported selected erase in line %d", arg);
             ty->decoding_error = EINA_TRUE;
             break;
          }
        arg = _csi_arg_get(ty, &b);
        if (arg == -CSI_ARG_ERROR)
          goto error;
        if (arg < 1)
          arg = 0;
        DBG("EL/DECSEL %d: erase in line", arg);
        switch (arg)
          {
           case TERMPTY_CLR_END:
           case TERMPTY_CLR_BEGIN:
           case TERMPTY_CLR_ALL:
              termpty_clear_line(ty, arg, ty->w);
              break;
           default:
              ERR("invalid EL/DECSEL argument %d", arg);
              ty->decoding_error = EINA_TRUE;
          }
        break;
      case 'L': // insert N lines - cy
        EINA_FALLTHROUGH;
      case 'M': // delete N lines - cy
        arg = _csi_arg_get(ty, &b);
        if (arg == -CSI_ARG_ERROR)
          goto error;
        TERMPTY_RESTRICT_FIELD(arg, 1, ty->h);
        DBG("%s %d lines", (*cc == 'M') ? "delete" : "insert", arg);
        if ((ty->cursor_state.cy >= ty->termstate.top_margin) &&
            ((ty->termstate.bottom_margin == 0) ||
             (ty->cursor_state.cy < ty->termstate.bottom_margin)))
          {
             int sy1, sy2;

             sy1 = ty->termstate.top_margin;
             sy2 = ty->termstate.bottom_margin;
             if (ty->termstate.bottom_margin == 0)
               {
                  ty->termstate.top_margin = ty->cursor_state.cy;
                  ty->termstate.bottom_margin = ty->h;
               }
             else
               {
                  ty->termstate.top_margin = ty->cursor_state.cy;
                  if (ty->termstate.bottom_margin <= ty->termstate.top_margin)
                    ty->termstate.bottom_margin = ty->termstate.top_margin + 1;
               }
             for (i = 0; i < arg; i++)
               {
                  if (*cc == 'M')
                    termpty_text_scroll(ty, EINA_TRUE);
                  else
                    termpty_text_scroll_rev(ty, EINA_TRUE);
               }
             ty->termstate.top_margin = sy1;
             ty->termstate.bottom_margin = sy2;
          }
        break;
      case 'P': // erase and scrollback N chars
        _handle_esc_csi_dch(ty, &b);
        break;
      case 'S': // scroll up N lines
        arg = _csi_arg_get(ty, &b);
        if (arg == -CSI_ARG_ERROR)
          goto error;
        TERMPTY_RESTRICT_FIELD(arg, 1, ty->h);
        DBG("scroll up %d lines", arg);
        for (i = 0; i < arg; i++)
          termpty_text_scroll(ty, EINA_TRUE);
        break;
      case 'T': // scroll down N lines
        arg = _csi_arg_get(ty, &b);
        if (arg == -CSI_ARG_ERROR)
          goto error;
        TERMPTY_RESTRICT_FIELD(arg, 1, ty->h);
        DBG("scroll down %d lines", arg);
        for (i = 0; i < arg; i++)
          termpty_text_scroll_rev(ty, EINA_TRUE);
        break;
      case 'X': // erase N chars
        arg = _csi_arg_get(ty, &b);
        if (arg == -CSI_ARG_ERROR)
          goto error;
        TERMPTY_RESTRICT_FIELD(arg, 1, ty->w);
        DBG("ECH: erase %d chars", arg);
        termpty_clear_line(ty, TERMPTY_CLR_END, arg);
        break;
      case 'Z':
          {
             int cx = ty->cursor_state.cx;

             arg = _csi_arg_get(ty, &b);
             if (arg == -CSI_ARG_ERROR)
               goto error;
             DBG("Cursor Backward Tabulation (CBT): %d", arg);
             TERMPTY_RESTRICT_FIELD(arg, 1, ty->w);
             for (; arg > 0; arg--)
               {
                  do
                    {
                       cx--;
                    }
                  while ((cx >= 0) && (!TAB_TEST(ty, cx)));
               }

             ty->cursor_state.cx = cx;
             TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
          }
        break;
      case '`': // HPA
        arg = _csi_arg_get(ty, &b);
        if (arg == -CSI_ARG_ERROR)
          goto error;
        DBG("Horizontal Position Absolute (HPA): %d", arg);
        arg--;
        if (arg < 0)
          arg = 0;
        ty->termstate.wrapnext = 0;
        ty->cursor_state.cx = arg;
        TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
        if (ty->termstate.restrict_cursor)
          {
             if ((ty->termstate.right_margin != 0)
                 && (ty->cursor_state.cx >= ty->termstate.right_margin))
               {
                  ty->cursor_state.cx = ty->termstate.right_margin - 1;
               }
             if ((ty->termstate.left_margin != 0)
                 && (ty->cursor_state.cx < ty->termstate.left_margin))
               {
                  ty->cursor_state.cx = ty->termstate.left_margin;
               }
          }
        break;
      case 'a': // cursor right N (HPR)
        _handle_esc_csi_cuf(ty, &b);
        break;
      case 'b': // repeat last char
        if (ty->last_char)
          {
             arg = _csi_arg_get(ty, &b);
             if (arg == -CSI_ARG_ERROR)
               goto error;
             TERMPTY_RESTRICT_FIELD(arg, 1, ty->w * ty->h);
             DBG("REP: repeat %d times last char %x", arg, ty->last_char);
             for (i = 0; i < arg; i++)
               termpty_text_append(ty, &ty->last_char, 1);
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
                  snprintf(bf, sizeof(bf), "\033[>41;337;%ic", 0);
               }
             else
               {
                  // Secondary device attributes
                  snprintf(bf, sizeof(bf), "\033[?64;1;2;6;9;15;18;21;22c");
               }
             termpty_write(ty, bf, strlen(bf));
          }
        break;
      case 'd': // to row N
        arg = _csi_arg_get(ty, &b);
        if (arg == -CSI_ARG_ERROR)
          goto error;
        if (arg < 1)
          arg = 1;
        DBG("to row %d", arg);
        ty->termstate.wrapnext = 0;
        ty->cursor_state.cy = arg - 1;
        TERMPTY_RESTRICT_FIELD(ty->cursor_state.cy, 0, ty->h);
        break;
      case 'e':
        _handle_esc_csi_cud(ty, &b);
        break;
      case 'f':
        _handle_esc_csi_cursor_pos_set(ty, &b, cc);
       break;
      case 'g': // clear tabulation
        arg = _csi_arg_get(ty, &b);
        if (arg == -CSI_ARG_ERROR)
          goto error;
        if (arg < 0)
          arg = 0;
        if (arg == 0)
          {
             int cx = ty->cursor_state.cx;
             TAB_UNSET(ty, cx);
          }
        else if (arg == 3)
          {
             termpty_clear_tabs_on_screen(ty);
          }
        else
          {
             ERR("Tabulation Clear (TBC) with invalid argument: %d", arg);
             ty->decoding_error = EINA_TRUE;
          }
        break;
      case 'h':
        _handle_esc_csi_reset_mode(ty, *cc, b, be);
        break;
      case 'i':
        WRN("TODO: Media Copy (?:%s)", (*b == '?') ? "yes": "no");
        ty->decoding_error = EINA_TRUE;
        break;
      case 'l':
        _handle_esc_csi_reset_mode(ty, *cc, b, be);
        break;
      case 'm': // color set
        _handle_esc_csi_color_set(ty, &b, be);
        break;
      case 'n':
        _handle_esc_csi_dsr(ty, b);
        break;
      case 'p': // define key assignments based on keycode
        if (b && *b == '!')
          {
             DBG("soft reset (DECSTR)");
             termpty_soft_reset_state(ty);
          }
        else
          {
             goto unhandled;
          }
        break;
      case 'q':
        if (*(cc-1) == ' ')
          _handle_esc_csi_decscusr(ty, &b);
        else if (*(cc-1) == '"')
          {
             WRN("TODO: select character protection attribute (DECSCA)");
             ty->decoding_error = EINA_TRUE;
          }
        else
          {
             WRN("TODO: Load LEDs (DECLL)");
             ty->decoding_error = EINA_TRUE;
          }
        break;
      case 'r':
        if (*(cc-1) == '$')
          _handle_esc_csi_deccara(ty, &b, be-1);
        else
          _handle_esc_csi_decstbm(ty, &b);
        break;
      case 's':
        if (ty->termstate.lr_margins)
          {
            _handle_esc_csi_decslrm(ty, &b);
          }
        else
          {
            DBG("SCOSC: Save Current Cursor Position");
            termpty_cursor_copy(ty, EINA_TRUE);
          }
        break;
      case 't':
        if (*(cc-1) == '$')
          _handle_esc_csi_decrara(ty, &b, be-1);
        else
          {
             arg = _csi_arg_get(ty, &b);
             if (arg == -CSI_ARG_ERROR)
               goto error;
             WRN("TODO: window operation %d not supported", arg);
             ty->decoding_error = EINA_TRUE;
          }
        break;
      case 'u': // restore cursor pos
        termpty_cursor_copy(ty, EINA_FALSE);
        break;
      case 'v':
        if (*(cc-1) == '$')
          _handle_esc_csi_deccra(ty, &b);
        else
             ty->decoding_error = EINA_TRUE;
        break;
      case 'x':
        if (*(cc-1) == '$')
          _handle_esc_csi_decfra(ty, &b);
        else if (*(cc-1) == '*')
          _handle_esc_csi_decsace(ty, &b);
        else
             ty->decoding_error = EINA_TRUE;
        break;
      case 'z':
        if (*(cc-1) == '$')
          _handle_esc_csi_decera(ty, &b);
        else
             ty->decoding_error = EINA_TRUE;
        break;
      case '}':
        if (*(cc-1) == '\'')
          _handle_esc_csi_decic(ty, &b);
        else
             ty->decoding_error = EINA_TRUE;
        break;
      case '~':
        if (*(cc-1) == '\'')
          _handle_esc_csi_decdc(ty, &b);
        else
             ty->decoding_error = EINA_TRUE;
        break;
      default:
        goto unhandled;
     }
   cc++;
   return cc - c;
unhandled:
error:
#if !defined(ENABLE_FUZZING) && !defined(ENABLE_TESTS)
   if (eina_log_domain_level_check(_termpty_log_dom, EINA_LOG_LEVEL_WARN))
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
#endif
   ty->decoding_error = EINA_TRUE;
   cc++;
   return cc - c;
}

static int
_osc_arg_get(Eina_Unicode **ptr)
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
_xterm_parse_color(Termpty *ty, Eina_Unicode **ptr,
                   unsigned char *r, unsigned char *g, unsigned char *b,
                   int len)
{
   Eina_Unicode *p = *ptr;
   int i;

   if (*p != '#')
     {
        WRN("unsupported xterm color");
        ty->decoding_error = EINA_TRUE;
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
   ty->decoding_error = EINA_TRUE;
   return -1;
}

static void
_handle_hyperlink(Termpty *ty,
                  char *s,
                  int len)
{
    const char *url = NULL;
    const char *key = NULL;
    Term_Link *hl = NULL;

    if (!s || len <= 0)
      {
         ERR("invalid hyperlink escape code (len:%d s:%p)",
             len, s);
         ty->decoding_error = EINA_TRUE;
         return;
      }

    if (len == 1 && *s == ';')
      {
         /* Closing escape code */
         if (ty->termstate.att.link_id)
           {
              ty->termstate.att.link_id = 0;
           }
         else
           {
              ERR("invalid hyperlink escape code: no hyperlink to close"
                  " (len:%d s:%.*s)", len, len, s);
              ty->decoding_error = EINA_TRUE;
           }
         goto end;
      }

    if (*s != ';')
      {
         /* Parse parameters */
         char *end;

         /* /!\ we expect ';' and ':' to be escaped in params */
         end = memchr(s+1, ';', len);
         if (!end)
           {
              ERR("invalid hyperlink escape code: missing ';'"
                  " (len:%d s:%.*s)", len, len, s);
              ty->decoding_error = EINA_TRUE;
              goto end;
           }
         *end = '\0';
         do
           {
              char *end_param;

              end_param = strchrnul(s, ':');

              if (len > 3 && strncmp(s, "id=", 3) == 0)
                {
                   eina_stringshare_del(key);

                   s += 3;
                   len -= 3;
                   key = eina_stringshare_add_length(s, end_param - s);
                }
              len -= end_param - s;
              s = end_param;
              if (*end_param)
                {
                   s++;
                   len--;
                }
           }
         while (s < end);
         *end = ';';
      }
    s++;
    len--;

    url = eina_stringshare_add_length(s, len);
    if (!url)
      goto end;

    hl = term_link_new(ty);
    if (!hl)
      goto end;
    hl->key = key;
    hl->url = url;
    key = NULL;
    url = NULL;

    ty->termstate.att.link_id = hl - ty->hl.links;
    hl = NULL;

end:
    term_link_free(ty, hl);
    eina_stringshare_del(url);
    eina_stringshare_del(key);
}

static void
_handle_xterm_50_command(Termpty *ty,
                         char *s,
                         int len)
{
  int pattern_len = strlen(":size=");
  while (len > pattern_len)
    {
       if (strncmp(s, ":size=", pattern_len) == 0)
         {
            char *endptr = NULL;
            int size;

            s += pattern_len;
            errno = 0;
            size = strtol(s, &endptr, 10);
            if (endptr != s && errno == 0)
               termio_font_size_set(ty->obj, size);
         }
       len--;
       s++;
    }
}

static void
_handle_xterm_777_command(Termpty *ty,
                          char *s, int _len EINA_UNUSED)
{
   char *cmd_end = NULL,
        *title = NULL,
        *title_end = NULL,
        *message = NULL;

   if (strncmp(s, "notify;", strlen("notify;")))
     {
        WRN("unrecognized xterm 777 command %s", s);
        ty->decoding_error = EINA_TRUE;
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
}

static int
_handle_esc_osc(Termpty *ty, const Eina_Unicode *c, const Eina_Unicode *ce)
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
        if ((cc < ce - 1) &&
            (((*cc == ESC) && (*(cc + 1) == '\\')) ||
             ((*cc == UTF8CC) && (*(cc + 1) == ST))))
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
        ERR("OSC parsing overflowed, skipping the whole buffer (binary data?)");
        return cc - c;
     }
   *p = '\0';
   p = buf;
   if ((*cc == ST) || (*cc == BEL) || (*cc == '\\'))
     cc++;
   else
     return 0;

#define TERMPTY_WRITE_STR(_S) \
   termpty_write(ty, _S, strlen(_S))

   arg = _osc_arg_get(&p);
   switch (arg)
     {
      case -1:
         goto err;
      case 0:
        // title + icon name
        if (!*p)
          goto err;
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
        if (ty->cb.set_title.func)
          ty->cb.set_title.func(ty->cb.set_title.data);
        if (ty->cb.set_icon.func) ty->cb.set_icon.func(ty->cb.set_icon.data);
        break;
      case 1:
        // icon name
        if (!*p)
          goto err;
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
        break;
      case 2:
        // Title
        if (!*p)
          goto err;
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
        break;
      case 4:
        if (!*p)
          goto err;
        // XXX: set palette entry. not supported.
        ty->decoding_error = EINA_TRUE;
        WRN("set palette, not supported");
        if ((cc - c) < 3) return 0;
        break;
      case 8:
        DBG("hyperlink");
        s = eina_unicode_unicode_to_utf8(p, &len);
        _handle_hyperlink(ty, s, len);
        break;
      case 10:
        if (!*p)
          goto err;
        if (*p == '?')
          {
             char bf[7];
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
             if (_xterm_parse_color(ty, &p, &r, &g, &b, len) < 0)
               goto err;
#if !defined(ENABLE_FUZZING) && !defined(ENABLE_TESTS)
             evas_object_textgrid_palette_set(
                termio_textgrid_get(ty->obj),
                EVAS_TEXTGRID_PALETTE_STANDARD, 0,
                r, g, b, 0xff);
#endif
          }
        break;
      case 50:
        DBG("xterm font support");
        if (!*p)
          goto err;
        s = eina_unicode_unicode_to_utf8(p, &len);
        if (s)
          {
             _handle_xterm_50_command(ty, s, len);
             free(s);
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
        ty->decoding_error = EINA_TRUE;
        break;
     }

#undef TERMPTY_WRITE_STR

    return cc - c;
err:
    WRN("invalid xterm sequence");
    ty->decoding_error = EINA_TRUE;
    return cc - c;
}

static int
_handle_esc_terminology(Termpty *ty, const Eina_Unicode *c, const Eina_Unicode *ce)
{
   Eina_Unicode *cc, *cc_zero = NULL;
   const Eina_Unicode *buf;
   char *cmd;
   int blen = 0;
   Config *config;

   if (!ty->buf_have_zero) return 0;

   config = termio_config_get(ty->obj);

   cc = (Eina_Unicode *)c;
   if ((cc < ce) && (*cc == 0x0)) cc_zero = cc;
   while ((cc < ce) && (*cc != 0x0))
     {
        blen++;
        cc++;
     }
   if ((cc < ce) && (*cc == 0x0)) cc_zero = cc;
   if (!cc_zero) return 0;
   buf = (Eina_Unicode *)c;
   cc = cc_zero;

   // commands are stored in the buffer, 0 bytes not allowed (end marker)
   cmd = eina_unicode_unicode_to_utf8(buf, NULL);
   ty->cur_cmd = cmd;
   if ((!config->ty_escapes) || (!_termpty_ext_handle(ty, cmd, buf)))
     {
        if (ty->cb.command.func) ty->cb.command.func(ty->cb.command.data);
     }
   ty->cur_cmd = NULL;
   free(cmd);

   return cc - c;
}

static int
_handle_esc_dcs(Termpty *ty,
                const Eina_Unicode *c,
                const Eina_Unicode *ce)
{
   const Eina_Unicode *cc, *be;
   Eina_Unicode buf[4096], *b;
   int len = 0;

   cc = c;
   b = buf;
   be = buf + sizeof(buf) / sizeof(buf[0]);
   while ((cc < ce) && (*cc != ST) && (b < be))
     {
        if ((cc < ce - 1) &&
            (((*cc == ESC) && (*(cc + 1) == '\\')) ||
             ((*cc == UTF8CC) && (*(cc + 1) == ST))))
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
              ty->decoding_error = EINA_TRUE;
              /* TODO */
              goto end;
               break;
            case 'p':
              WRN("unhandled dsc request to set termcap/terminfo");
              ty->decoding_error = EINA_TRUE;
              /* TODO */
              goto end;
               break;
            default:
              ERR("invalid dsc request about termcap/terminfo");
              ty->decoding_error = EINA_TRUE;
              goto end;
           }
         break;
      case '$':
         /* Request status string */
         if (len > 1 && buf[1] != 'q')
           {
              WRN("invalid/unhandled dsc esc '$%s' (expected '$q')", _safechar(buf[1]));
              ty->decoding_error = EINA_TRUE;
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
                    ty->decoding_error = EINA_TRUE;
                    goto end;
                 }
               else
                 {
                    WRN("invalid/unhandled dsc esc '$q\"%s'", _safechar(buf[3]));
                    ty->decoding_error = EINA_TRUE;
                    goto end;
                 }
               break;
            case 'm': /* SGR */
               /* TODO: */
            case 'r': /* DECSTBM */
               /* TODO: */
            default:
               ty->decoding_error = EINA_TRUE;
               WRN("unhandled dsc request status string '$q%s'", _safechar(buf[2]));
               goto end;
           }
         break;
      default:
        // many others
        WRN("Unhandled DCS escape '%s'", _safechar(buf[0]));
        ty->decoding_error = EINA_TRUE;
        break;
     }
end:
   return len;
}

static void
_handle_decaln(Termpty *ty)
{
   int size;
   Termcell *cells;

   DBG("DECALN - fill screen with E");
   ty->termstate.top_margin = 0;
   ty->termstate.bottom_margin = 0;
   ty->termstate.left_margin = 0;
   ty->termstate.right_margin = 0;
   ty->termstate.had_cr_x = 0;
   ty->termstate.had_cr_y = 0;
   ty->cursor_state.cx = 0;
   ty->cursor_state.cy = 0;
   ty->termstate.att.link_id = 0;

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

static void
_handle_decbi(Termpty *ty)
{
   DBG("DECBI - Back Index");
   if (ty->cursor_state.cx == ty->termstate.left_margin)
     {
        Eina_Unicode blank[1] = { ' ' };
        int old_insert = ty->termstate.insert;
        int old_cx = ty->cursor_state.cx;
        int old_cy = ty->cursor_state.cy;
        int y;
        int max = ty->h;

        if (((ty->termstate.lr_margins != 0) && (ty->cursor_state.cx == 0))
            || ((ty->termstate.top_margin != 0)
                && (ty->cursor_state.cy < ty->termstate.top_margin))
            || ((ty->termstate.bottom_margin != 0)
                && (ty->cursor_state.cy >= ty->termstate.bottom_margin)))
          {
             return;
          }
        if (ty->termstate.bottom_margin != 0)
          max = ty->termstate.bottom_margin;
        ty->termstate.insert = 1;
        for (y = ty->termstate.top_margin; y < max; y++)
          {
             /* Insert a left column */
             ty->cursor_state.cy = y;
             ty->cursor_state.cx = old_cx;
             ty->termstate.wrapnext = 0;
             termpty_text_append(ty, blank, 1);
          }
        ty->termstate.insert = old_insert;
        ty->cursor_state.cx = old_cx;
        ty->cursor_state.cy = old_cy;
     }
   else
     {
        if ((ty->cursor_state.cx == 0) && (ty->termstate.lr_margins != 0))
          return;
        /* cursor backward */
        ty->cursor_state.cx--;
     }
}

static void
_handle_decfi(Termpty *ty)
{
   DBG("DECFI - Forward Index");
   if ((ty->cursor_state.cx == ty->w - 1)
       || ((ty->termstate.right_margin > 0)
           && (ty->cursor_state.cx == ty->termstate.right_margin - 1)))
     {
        int y;
        int max_x = ty->w - 1;
        int max_y = ty->h;

        if (((ty->termstate.lr_margins != 0)
             && (ty->cursor_state.cx == ty->w - 1))
            || ((ty->termstate.top_margin != 0)
                && (ty->cursor_state.cy < ty->termstate.top_margin))
            || ((ty->termstate.bottom_margin != 0)
                && (ty->cursor_state.cy >= ty->termstate.bottom_margin)))
          {
             return;
          }
        if (ty->termstate.bottom_margin != 0)
          max_y = ty->termstate.bottom_margin;
        if (ty->termstate.right_margin != 0)
          max_x = ty->termstate.right_margin - 1;

        for (y = ty->termstate.top_margin; y < max_y; y++)
          {
             int x;
             Termcell *cells = &(TERMPTY_SCREEN(ty, 0, y));

             for (x = ty->termstate.left_margin; x <= max_x; x++)
               {
                  if (x < max_x)
                    TERMPTY_CELL_COPY(ty, &(cells[x + 1]), &(cells[x]), 1);
                  else
                    {
                       cells[x].codepoint = ' ';
                       if (EINA_UNLIKELY(cells[x].att.link_id))
                         term_link_refcount_dec(ty, cells[x].att.link_id, 1);
                       cells[x].att = ty->termstate.att;
                       cells[x].att.link_id = 0;
                       cells[x].att.dblwidth = 0;
                    }
               }
          }
     }
   else
     {
        if ((ty->cursor_state.cx == ty->w - 1)
            && (ty->termstate.lr_margins != 0))
          return;
        ERR("ty->cursor_state.cx=%d", ty->cursor_state.cx);
        ERR("ty->cursor_state.cy=%d", ty->cursor_state.cy);
        /* cursor forward */
        ty->cursor_state.cx++;
     }
}

static int
_handle_esc(Termpty *ty, const Eina_Unicode *c, const Eina_Unicode *ce)
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
        len = _handle_esc_osc(ty, c + 1, ce);
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
        DBG("move to prev line");
        ty->termstate.wrapnext = 0;
        ty->cursor_state.cy--;
        termpty_text_scroll_rev_test(ty, EINA_TRUE);
        return 1;
      case 'D': // move to next line
        DBG("move to next line");
        ty->termstate.wrapnext = 0;
        ty->cursor_state.cy++;
        termpty_text_scroll_test(ty, EINA_FALSE);
        return 1;
      case 'E': // add \n\r
        DBG("add \\n\\r");
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
          _handle_decaln(ty);
        return 2;
      case '@': // just consume this plus next char
        if (len < 2) return 0;
        return 2;
      case '6':
        _handle_decbi(ty);
        return 1;
      case '7': // save cursor pos
        termpty_cursor_copy(ty, EINA_TRUE);
        return 1;
      case '8': // restore cursor pos
        termpty_cursor_copy(ty, EINA_FALSE);
        return 1;
      case '9':
        _handle_decfi(ty);
        return 1;
      case 'H': // set tab at current column
        DBG("Character Tabulation Set (HTS) at x:%d", ty->cursor_state.cx);
        TAB_SET(ty, ty->cursor_state.cx);
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
        ty->decoding_error = EINA_TRUE;
        WRN("Unhandled escape '%s' (0x%02x)", _safechar(c[0]), (unsigned int) c[0]);
        return 1;
     }
   return 0;
}

static int
_handle_utf8_control_code(Termpty *ty, const Eina_Unicode *c, const Eina_Unicode *ce)
{
   int len = ce - c;

   if (len < 1)
     return 0;
   DBG("c0 utf8: '%s' (0x%02x)", _safechar(c[0]), c[0]);
   switch (c[0])
     {
      case 0x9b:
        len = _handle_esc_csi(ty, c + 1, ce);
        if (len == 0) return 0;
        return 1 + len;
      case 0x9d:
        len = _handle_esc_osc(ty, c + 1, ce);
        if (len == 0) return 0;
        return 1 + len;
      default:
        ty->decoding_error = EINA_TRUE;
        WRN("Unhandled utf8 control code '%s' (0x%02x)", _safechar(c[0]), (unsigned int) c[0]);
        return 1;
     }
   return 0;
}


/* XXX: ce is excluded */
int
termpty_handle_seq(Termpty *ty, const Eina_Unicode *c, const Eina_Unicode *ce)
{
   Eina_Unicode *cc;
   Eina_Unicode last_char = 0;
   int len = 0;
   ty->decoding_error = EINA_FALSE;

   if (c[0] < 0x20)
     {
        switch (c[0])
          {
           case 0x07: // BEL '\a' (bell)
           case 0x08: // BS  '\b' (backspace)
           case 0x09: // HT  '\t' (horizontal tab)
           case 0x0a: // LF  '\n' (new line)
           case 0x0b: // VT  '\v' (vertical tab)
           case 0x0c: // FF  '\f' (form feed)
           case 0x0d: // CR  '\r' (carriage ret)
             _handle_cursor_control(ty, c);
             len = 1;
             goto end;

           case 0x0e: // SO  (shift out) // Maps G1 character set into GL.
             ty->termstate.charset = 1;
             ty->termstate.charsetch = ty->termstate.chset[1];
             len = 1;
             goto end;
           case 0x0f: // SI  (shift in) // Maps G0 character set into GL.
             ty->termstate.charset = 0;
             ty->termstate.charsetch = ty->termstate.chset[0];
             len = 1;
             goto end;
           case 0x1b: // ESC (escape)
             len = _handle_esc(ty, c + 1, ce);
             if (len == 0)
               goto end;
             len++;
             goto end;
           default:
             //ERR("unhandled char 0x%02x", c[0]);
             len = 1;
             goto end;
          }
     }
   else if (c[0] == 0x7f) // DEL
     {
        WRN("Unhandled char 0x%02x [DEL]", (unsigned int) c[0]);
        ty->decoding_error = EINA_TRUE;
        len = 1;
        goto end;
     }
   else if (c[0] == 0x9b) // ANSI ESC!!!
     {
        DBG("ANSI CSI!!!!!");
        len = _handle_esc_csi(ty, c + 1, ce);
        if (len == 0)
          goto end;
        len++;
        goto end;
     }
   else if (c[0] == UTF8CC)
     {
        len = _handle_utf8_control_code(ty, c + 1, ce);
        if (len == 0)
          goto end;
        len++;
        goto end;
     }
   else if ((ty->block.expecting) && (ty->block.on))
     {
        Termexp *ex;
        Eina_List *l;

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
                  len = 1;
                  goto end;
               }
          }
        termpty_text_append(ty, c, 1);
        len = 1;
        goto end;
     }
   cc = (Eina_Unicode *)c;

   DBG("txt: [");
   while ((cc < ce) && (*cc >= 0x20) && (*cc != 0x7f) && (*cc != 0x9b)
          && (*cc != UTF8CC))
     {
        DBG("%s", _safechar(*cc));
        cc++;
        len++;
     }
   DBG("]");
   termpty_text_append(ty, c, len);
   if (len > 0)
       last_char = c[len-1];

end:
#if !defined(ENABLE_FUZZING) && !defined(ENABLE_TESTS)
   if (ty->decoding_error)
     {
      int j;
      for (j = 0; c + j < ce && j < len; j++)
        {
           if ((c[j] < ' ') || (c[j] >= 0x7f))
             printf("\033[35m%08x\033[0m", c[j]);
           else
             printf("%s", _safechar(c[j]));
        }
      printf("\n");
     }
#endif
   ty->decoding_error = EINA_FALSE;
   ty->last_char = last_char;
   return len;
}
