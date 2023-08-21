#include "private.h"
#include <Elementary.h>
#include <stdint.h>
#include <assert.h>
#include "colors.h"
#include "termio.h"
#include "termpty.h"
#include "termptydbl.h"
#include "termptyesc.h"
#include "termptyops.h"
#include "termptyext.h"
#include "theme.h"
#if defined(BINARY_TYTEST)
#include "tytest.h"
#endif
#include "utils.h"

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
#define CSI 0x9b
#define OSC 0x9d
#define DEL 0x7f


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

const char * EINA_PURE
termptyesc_safechar(const unsigned int c)
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

static Eina_Bool
_cursor_is_within_margins(const Termpty *ty)
{
   return !(
      ((ty->termstate.top_margin > 0)
       && (ty->cursor_state.cy < ty->termstate.top_margin))
      || ((ty->termstate.bottom_margin > 0)
          && (ty->cursor_state.cy >= ty->termstate.bottom_margin))
      || ((ty->termstate.left_margin > 0)
          && (ty->cursor_state.cx < ty->termstate.left_margin))
      || ((ty->termstate.right_margin > 0)
          && (ty->cursor_state.cx >= ty->termstate.right_margin))
      );
}

enum esc_arg_error {
     ESC_ARG_NO_VALUE = 1,
     ESC_ARG_ERROR = 2
};

static int
_csi_arg_get(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int sum = 0;

   if ((b == NULL) || (*b == '\0'))
     {
        *ptr = NULL;
        return -ESC_ARG_NO_VALUE;
     }

   /* Skip potential '?', '>'.... */
   while ((*b) && ( (*b) != ';' && ((*b) < '0' || (*b) > '9')))
     b++;

   if (*b == ';')
     {
        b++;
        *ptr = b;
        return -ESC_ARG_NO_VALUE;
     }

   if (*b == '\0')
     {
        *ptr = NULL;
        return -ESC_ARG_NO_VALUE;
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
   ERR("Invalid CSI argument");
   ty->decoding_error = EINA_TRUE;
   *ptr = NULL;
   return -ESC_ARG_ERROR;
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
   Termcell *cell;

   switch (*cc)
     {
      case 0x07: // BEL '\a' (bell)
        DBG("->BEL");
         if (ty->cb.bell.func) ty->cb.bell.func(ty->cb.bell.data);
         return;
      case 0x08: // BS  '\b' (backspace)
         DBG("->BS");
         ty->cursor_state.wrapnext = 0;
         ty->cursor_state.cx--;
         TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
         return;
      case 0x09: // HT  '\t' (horizontal tab)
         DBG("->HT");
         cell = &(TERMPTY_SCREEN(ty, ty->cursor_state.cx, ty->cursor_state.cy));
         cell->att.tab_inserted = 1;
         _tab_forward(ty, 1);
         cell = &(TERMPTY_SCREEN(ty, ty->cursor_state.cx -1, ty->cursor_state.cy));
         cell->att.tab_last = 1;
         return;
      case 0x0a: // LF  '\n' (new line)
      case 0x0b: // VT  '\v' (vertical tab)
      case 0x0c: // FF  '\f' (form feed)
         DBG("->LF");
         ty->cursor_state.wrapnext = 0;
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
              ty->cursor_state.wrapnext = 0;
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
   Eina_Bool mode = EINA_FALSE;
   Eina_Bool priv = EINA_FALSE;
   int arg;

   if (cc == 'h')
     mode = EINA_TRUE;
   if (*b == '?')
     {
        priv = EINA_TRUE;
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
                case -ESC_ARG_ERROR:
                   return;
              /* TODO: -ESC_ARG_NO_VALUE */
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
                case 1004:
                   DBG("%s focus reporting", mode ? "enable" : "disable");
                   if (mode)
                     {
                        ty->focus_reporting = EINA_TRUE;
                        termpty_focus_report(ty, termio_is_focused(ty->obj));
                     }
                   else
                     {
                        ty->focus_reporting = EINA_FALSE;
                     }
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
                   WRN("Ignored screen mode %i", arg);
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
                case 7700: // ignore
                   WRN("TODO: ambigous width reporting %i", mode);
                   ty->decoding_error = EINA_TRUE;
                   break;
                case 7727:
                   DBG("%s application escape mode", mode ? "enable" : "disable");
                   ty->termstate.esc_keycode = !!mode;
                   break;
                case 7728:
                   DBG("%s alternate escape", mode ? "enable" : "disable");
                   ty->termstate.alternate_esc = !!mode;
                   break;
                case 7766: // ignore
                   WRN("TODO: %s scrollbar", mode ? "hide" : "show");
                   ty->decoding_error = EINA_TRUE;
                   break;
                case 7783: // ignore
                   WRN("TODO: shortcut override mode %i", mode);
                   ty->decoding_error = EINA_TRUE;
                   break;
                case 7786: // ignore
                   EINA_FALLTHROUGH;
                case 7787: // ignore
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
                case -ESC_ARG_ERROR:
                   return;
              /* TODO: -ESC_ARG_NO_VALUE */
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
        return -ESC_ARG_NO_VALUE;
     }

   /* by construction, shall be the same separator as the following ones */
   separator = *(b-1);
   if ((separator != ';') && (separator != ':'))
     {
        *ptr = NULL;
        return -ESC_ARG_NO_VALUE;
     }

   if (*b == separator)
     {
        b++;
        *ptr = b;
        return -ESC_ARG_NO_VALUE;
     }

   if (*b == '\0')
     {
        *ptr = NULL;
        return -ESC_ARG_NO_VALUE;
     }
   /* invalid values */
   if ((*b < '0') || (*b > '9'))
     {
        *ptr = NULL;
        return -ESC_ARG_ERROR;
     }

   while ((*b >= '0') && (*b <= '9'))
     {
        if (sum > INT32_MAX/10 )
          {
             *ptr = NULL;
             ERR("Invalid sequence: argument is too large");
             ty->decoding_error = EINA_TRUE;
             return -ESC_ARG_ERROR;
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

#if !defined(BINARY_TYFUZZ)
/*********************************
 * cache true color approximations
 *********************************
 * Approximating true colors is costly since it needs to compare 256 colors.
 * The following considers that a few colors are used a lot.  For example, one
 * can consider that a text editor with syntax highlighting would only use a
 * small palette.
 * Given that, using the cache needs to be efficient: it needs to speed up the
 * approximation when the color is already in the cache but not knowing the
 * color requested is not in the cache needs to be efficient too.
 *
 * The cache is an array of @TCC_LEN uint32_t.
 * Each entry has on the MSB the color as 3 uint8_t (R,G,B) and the LSB is
 * the approximated color.
 * Searching a color is simply going through the array and testing the mask on
 * the 3 most significant bytes.
 * When a color is found, it is bubbled up towards the start of the array by
 * swapping this entry with the one above.  This way the array is ordered to
 * get most used colors as fast as possible.
 * When a color is not found, the slow process of comparing it with the 256
 * colors is used.  Then the result is inserted in the lower half of the
 * array.  This ensures that new entries can live a bit and not be removed by
 * the next new color.  Using a modulo with a prime number makes for a
 * nice-enough random generator to figure out where to insert.
 */
#define TCC_LEN 32
#define TCC_PRIME 17 /* smallest prime number larger than @TCC_LEN/2 */
static struct {
     uint32_t colors[TCC_LEN];
} _truecolor_cache;
static int _tcc_random_pos = 0;

static Eina_Bool
_tcc_find(const uint32_t color_msb, uint8_t *chosen_color)
{
   int i;
   for (i = 0; i < TCC_LEN; i++)
     {
        if ((_truecolor_cache.colors[i] & 0xffffff00) == color_msb)
          {
             *chosen_color = _truecolor_cache.colors[i] & 0xff;
             /* bubble up this result */
             if (i > 0)
               {
                  uint32_t prev = _truecolor_cache.colors[i - 1];
                  _truecolor_cache.colors[i - 1] = _truecolor_cache.colors[i];
                  _truecolor_cache.colors[i] = prev;
               }
             return EINA_TRUE;
          }
     }
   return EINA_FALSE;
}

static void
_tcc_insert(const uint32_t color_msb, const uint8_t approximated)
{
   uint32_t c = color_msb | approximated;
   int i;

   _tcc_random_pos = ((_tcc_random_pos + TCC_PRIME) % (TCC_LEN / 2));
   i = (TCC_LEN / 2) + _tcc_random_pos;

  _truecolor_cache.colors[i] = c;
}
#endif

static uint8_t
_approximate_truecolor_rgb(Termpty *ty, uint8_t r0, uint8_t g0, uint8_t b0)
{
   uint8_t chosen_color = COL_DEF;
#if defined(BINARY_TYFUZZ)
   (void) ty;
   (void) r0;
   (void) g0;
   (void) b0;
#else
   int c;
   int distance_min = INT_MAX;
   Evas_Object *textgrid;
   const uint32_t color_msb = 0
      | (((uint32_t)r0) << 24)
      | (((uint32_t)g0) << 16)
      | (((uint32_t)b0) << 8);

   if (_tcc_find(color_msb, &chosen_color))
     return chosen_color;

   textgrid = termio_textgrid_get(ty->obj);
   for (c = 0; c < 256; c++)
     {
        int r1 = 0, g1 = 0, b1 = 0, a1 = 0;
        int delta_red_sq, delta_green_sq, delta_blue_sq, red_mean;
        int distance;

        evas_object_textgrid_palette_get(textgrid,
                                         EVAS_TEXTGRID_PALETTE_EXTENDED,
                                         c, &r1, &g1, &b1, &a1);
        /* Compute the color distance
         * XXX: this is inacurate but should give good enough results.
         * See https://en.wikipedia.org/wiki/Color_difference
         */
        red_mean = (r0 + r1) / 2;
        delta_red_sq = (r0 - r1) * (r0 - r1);
        delta_green_sq = (g0 - g1) * (g0 - g1);
        delta_blue_sq = (b0 - b1) * (b0 - b1);

#if 1
        distance = 2 * delta_red_sq
           + 4 * delta_green_sq
           + 3 * delta_blue_sq
           + ((red_mean) * (delta_red_sq - delta_blue_sq) / 256);
#else
        /* from https://www.compuphase.com/cmetric.htm */
        distance = (((512 + red_mean) * delta_red_sq) >> 8)
                 + 4 * delta_green_sq
                 + (((767 - red_mean) * delta_blue_sq) >> 8);
        /* euclidian distance */
        distance = delta_red_sq + delta_green_sq + delta_blue_sq;
        (void)red_mean;
#endif
        if (distance < distance_min)
          {
             distance_min = distance;
             chosen_color = c;
          }
     }
   _tcc_insert(color_msb, chosen_color);
#endif
   return chosen_color;
}

static uint8_t
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
   if ((r == -ESC_ARG_ERROR) ||
       (g == -ESC_ARG_ERROR) ||
       (b == -ESC_ARG_ERROR))
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
                  if (arg == -ESC_ARG_ERROR)
                    break;
               }
          }
        if ((*ptr) && (**ptr == ';'))
          {
             *ptr = (*ptr) + 1;
          }
     }

   if (r == -ESC_ARG_NO_VALUE)
     r = 0;
   if (g == -ESC_ARG_NO_VALUE)
     g = 0;
   if (b == -ESC_ARG_NO_VALUE)
     b = 0;

   return _approximate_truecolor_rgb(ty, (uint8_t)r, (uint8_t)g, (uint8_t)b);
}

static uint8_t
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

   if ((c == -ESC_ARG_ERROR) ||
       (m == -ESC_ARG_ERROR) ||
       (y == -ESC_ARG_ERROR))
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
                  if (arg == -ESC_ARG_ERROR)
                    break;
               }
          }
        if ((*ptr) && (**ptr == ';'))
          {
             *ptr = (*ptr) + 1;
          }
     }

   if (c == -ESC_ARG_NO_VALUE)
     c = 0;
   if (m == -ESC_ARG_NO_VALUE)
     m = 0;
   if (y == -ESC_ARG_NO_VALUE)
     y = 0;

   r = 255 - ((c * 255) / 100);
   g = 255 - ((m * 255) / 100);
   b = 255 - ((y * 255) / 100);

   return _approximate_truecolor_rgb(ty, (uint8_t)r, (uint8_t)g, (uint8_t)b);
}

static uint8_t
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

   if ((c == -ESC_ARG_ERROR) ||
       (m == -ESC_ARG_ERROR) ||
       (y == -ESC_ARG_ERROR) ||
       (k == -ESC_ARG_ERROR))
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
                  if (arg == -ESC_ARG_ERROR)
                    break;
               }
          }
        if ((*ptr) && (**ptr == ';'))
          {
             *ptr = (*ptr) + 1;
          }
     }

   if (c == -ESC_ARG_NO_VALUE)
     c = 0;
   if (m == -ESC_ARG_NO_VALUE)
     m = 0;
   if (y == -ESC_ARG_NO_VALUE)
     y = 0;
   if (k == -ESC_ARG_NO_VALUE)
     k = 0;

   r = (255 * (100 - c) * (100 - k)) / 100 / 100;
   g = (255 * (100 - m) * (100 - k)) / 100 / 100;
   b = (255 * (100 - y) * (100 - k)) / 100 / 100;

   return _approximate_truecolor_rgb(ty, (uint8_t)r, (uint8_t)g, (uint8_t)b);
}

static void
_handle_esc_csi_color_set(Termpty *ty, Eina_Unicode **ptr,
                          const Eina_Unicode * const end)
{
   Eina_Unicode *b = *ptr;

   DBG("color set");
   while (b && b <= end)
     {
        int arg = _csi_arg_get(ty, &b);
        switch (arg)
          {
           case -ESC_ARG_ERROR:
              return;
           case -ESC_ARG_NO_VALUE:
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
                 case -ESC_ARG_ERROR:
                    EINA_FALLTHROUGH;
                 case -ESC_ARG_NO_VALUE:
                    return;
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
                    if (arg <= -ESC_ARG_ERROR || arg > 255)
                      {
                         ERR("Invalid fg color %d", arg);
                         ty->decoding_error = EINA_TRUE;
                      }
                    else
                      {
                         if (arg == -ESC_ARG_NO_VALUE)
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
                 case -ESC_ARG_ERROR:
                    EINA_FALLTHROUGH;
                 case -ESC_ARG_NO_VALUE:
                    return;
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
                    if (arg <= -ESC_ARG_ERROR || arg > 255)
                      {
                         ERR("Invalid bg color %d", arg);
                         ty->decoding_error = EINA_TRUE;
                      }
                    else
                      {
                         if (arg == -ESC_ARG_NO_VALUE)
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

   if (arg == -ESC_ARG_ERROR)
     return;
   if (arg < 1)
     arg = 1;

   DBG("CNL - Cursor Next Line: %d", arg);
   ty->cursor_state.wrapnext = 0;
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

   if (arg == -ESC_ARG_ERROR)
     return;
   if (arg < 1)
     arg = 1;

   DBG("CPL - Cursor Previous Line: %d", arg);
   ty->cursor_state.wrapnext = 0;
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

   if (arg == -ESC_ARG_ERROR)
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

   if (*b == '?')
     {
        question_mark = EINA_TRUE;
        b++;
     }
   arg = _csi_arg_get(ty, &b);
   switch (arg)
     {
      case -ESC_ARG_ERROR:
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
              TERMPTY_WRITE_STR("\033[0n");
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
              if (pid == -ESC_ARG_NO_VALUE)
                pid = 65535;
              len = snprintf(bf, sizeof(bf), "\033P%u!~0000\033\\",
                             ((unsigned int)pid) % 65536);
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
      /* TODO: -ESC_ARG_NO_VALUE */
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
  if ((left == -ESC_ARG_ERROR) || (right == -ESC_ARG_ERROR))
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
  if ((top == -ESC_ARG_ERROR) || (bottom == -ESC_ARG_ERROR))
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
   if ((c == -ESC_ARG_ERROR) ||
       (c == -ESC_ARG_NO_VALUE) ||
       (top == -ESC_ARG_ERROR) ||
       (left == -ESC_ARG_ERROR) ||
       (bottom == -ESC_ARG_ERROR) ||
       (right == -ESC_ARG_ERROR))
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
   if ((top == -ESC_ARG_ERROR) ||
       (left == -ESC_ARG_ERROR) ||
       (bottom == -ESC_ARG_ERROR) ||
       (right == -ESC_ARG_ERROR))
     return;

   while (b && b < end)
     {
        int arg = _csi_arg_get(ty, &b);
        switch (arg)
          {
           case -ESC_ARG_ERROR:
              return;
           case -ESC_ARG_NO_VALUE:
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
   if ((top == -ESC_ARG_ERROR) ||
       (left == -ESC_ARG_ERROR) ||
       (bottom == -ESC_ARG_ERROR) ||
       (right == -ESC_ARG_ERROR))
     return;

   while (b && b < end)
     {
        int arg = _csi_arg_get(ty, &b);
        switch (arg)
          {
           case -ESC_ARG_ERROR:
              return;
           case -ESC_ARG_NO_VALUE:
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

   if ((top == -ESC_ARG_ERROR) ||
       (left == -ESC_ARG_ERROR) ||
       (bottom == -ESC_ARG_ERROR) ||
       (right == -ESC_ARG_ERROR))
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
   if ((top == -ESC_ARG_ERROR) ||
       (left == -ESC_ARG_ERROR) ||
       (bottom == -ESC_ARG_ERROR) ||
       (right == -ESC_ARG_ERROR) ||
       (p1 == -ESC_ARG_ERROR) ||
       (to_top == -ESC_ARG_ERROR) ||
       (to_left == -ESC_ARG_ERROR) ||
       (p2 == -ESC_ARG_ERROR))
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
   ty->cursor_state.wrapnext = 0;
   cy = _csi_arg_get(ty, b);
   cx = _csi_arg_get(ty, b);

   if ((cx == -ESC_ARG_ERROR) || (cy == -ESC_ARG_ERROR))
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
     case -ESC_ARG_ERROR:
        return;
     case -ESC_ARG_NO_VALUE:
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
_handle_esc_csi_term_version(Termpty *ty, Eina_Unicode **b)
{
  int arg = _csi_arg_get(ty, b);

  DBG("CSI Term version (%d)", arg);

  switch (arg)
    {
     case -ESC_ARG_ERROR:
        return;
     case -ESC_ARG_NO_VALUE:
        EINA_FALLTHROUGH;
     case 0:
        break;
     default:
        WRN("Invalid Term Version %d", arg);
        ty->decoding_error = EINA_TRUE;
        return;
    }
  TERMPTY_WRITE_STR("\033P>|" PACKAGE_NAME "(" PACKAGE_VERSION ")\033\\");
}

static void
_handle_esc_csi_decsace(Termpty *ty, Eina_Unicode **b)
{
  int arg = _csi_arg_get(ty, b);

  DBG("DECSACE (%d) Select Attribute Change Extent", arg);

  switch (arg)
    {
     case -ESC_ARG_ERROR:
        return;
     case -ESC_ARG_NO_VALUE:
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

   if (arg == -ESC_ARG_ERROR)
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
        ty->cursor_state.wrapnext = 0;
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

   if (arg == -ESC_ARG_ERROR)
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

   if (arg == -ESC_ARG_ERROR)
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

   ty->cursor_state.wrapnext = 0;
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

   if (arg == -ESC_ARG_ERROR)
     return;
   if (arg < 1)
     arg = 1;
   DBG("CUU - Cursor Up %d", arg);
   ty->cursor_state.wrapnext = 0;
   ty->cursor_state.cy = MAX(0, ty->cursor_state.cy - arg);
   TERMPTY_RESTRICT_FIELD(ty->cursor_state.cy, 0, ty->h);
   if (ty->termstate.restrict_cursor && (ty->termstate.top_margin > 0)
       && (ty->cursor_state.cy < ty->termstate.top_margin))
     {
        ty->cursor_state.cy = ty->termstate.top_margin;
     }
}

static void
_handle_esc_csi_cud_or_vpr(Termpty *ty, Eina_Unicode **ptr,
                           const Eina_Unicode *cc)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);

   if (arg == -ESC_ARG_ERROR)
     return;

   if (arg < 1)
     arg = 1;

   if (*cc == 'e')
     {
        DBG("VPR - Vertical Position Relative: %d", arg);
     }
   else
     {
        DBG("CUD - Cursor Down: %d", arg);
     }

   ty->cursor_state.wrapnext = 0;
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

   if (arg == -ESC_ARG_ERROR)
     return;
   if (arg < 1)
     arg = 1;
   DBG("CUF - Cursor Forward %d", arg);
   ty->cursor_state.wrapnext = 0;
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

   if (arg == -ESC_ARG_ERROR)
     return;
   if (arg < 1)
     arg = 1;
   DBG("CUB - Cursor Backward %d", arg);
   ty->cursor_state.wrapnext = 0;
   ty->cursor_state.cx -= arg;
   TERMPTY_RESTRICT_FIELD(ty->cursor_state.cx, 0, ty->w);
   if (ty->termstate.restrict_cursor && (ty->termstate.left_margin != 0)
       && (ty->cursor_state.cx < ty->termstate.left_margin))
     {
        ty->cursor_state.cx = ty->termstate.left_margin;
     }
}

static void
_handle_esc_csi_cha(Termpty *ty, Eina_Unicode **ptr,
                    const Eina_Unicode *cc)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);
   int min = 0;
   int max = ty->w;

   if (*cc == '`')
     {
        DBG("HPA - Horizontal Position Absolute: %d", arg);
     }
   else
     {
        DBG("CHA - Cursor Horizontal Absolute: %d", arg);
     }
   if (arg == -ESC_ARG_ERROR)
     return;
   if (arg < 1)
     arg = 1;
   ty->cursor_state.wrapnext = 0;
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

   if (arg == -ESC_ARG_ERROR)
     return;
   TERMPTY_RESTRICT_FIELD(arg, 1, ty->w);
   DBG("CHT - Cursor Forward Tabulation: %d", arg);
   _tab_forward(ty, arg);
}

static void
_handle_esc_csi_ed(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);

   if (arg == -ESC_ARG_ERROR)
     return;
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
   TERMPTY_RESTRICT_FIELD(arg, 1, ty->w);
}

static void
_handle_esc_csi_decsed(Termpty *ty, Eina_Unicode **ptr)
{
   WRN("DECSED - Selective Erase in Display: Unsupported");
   _handle_esc_csi_ed(ty, ptr);
}

static void
_handle_esc_csi_el(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);

   if (arg == -ESC_ARG_ERROR)
     return;
   if (arg < 1)
     arg = 0;
   DBG("EL - Erase in Line: %d", arg);
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
}

static void
_handle_esc_csi_decsel(Termpty *ty, Eina_Unicode **ptr)
{
   WRN("DECSEL - Selective Erase in Line: Unsupported");
   _handle_esc_csi_el(ty, ptr);
}

static void
_handle_esc_csi_il(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);
   int sy1, sy2, i;

   if (arg == -ESC_ARG_ERROR)
     return;

   TERMPTY_RESTRICT_FIELD(arg, 1, ty->h);
   DBG("IL - Insert Lines: %d", arg);

   if (!_cursor_is_within_margins(ty))
     return;

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
        termpty_text_scroll_rev(ty, EINA_TRUE);
     }
   ty->termstate.top_margin = sy1;
   ty->termstate.bottom_margin = sy2;
   ty->cursor_state.cx = ty->termstate.left_margin;
}

static void
_handle_esc_csi_dl(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);
   int sy1, sy2, i;

   if (arg == -ESC_ARG_ERROR)
     return;

   TERMPTY_RESTRICT_FIELD(arg, 1, ty->h);
   DBG("DL - Delete Lines: %d", arg);

   if (!_cursor_is_within_margins(ty))
     return;

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
        termpty_text_scroll(ty, EINA_TRUE);
     }
   ty->termstate.top_margin = sy1;
   ty->termstate.bottom_margin = sy2;
   ty->cursor_state.cx = ty->termstate.left_margin;
}

static void
_handle_esc_csi_su(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);
   int i;

   if (arg == -ESC_ARG_ERROR)
     return;

   TERMPTY_RESTRICT_FIELD(arg, 1, ty->h);
   DBG("SU - Scroll Up: %d", arg);
   for (i = 0; i < arg; i++)
     termpty_text_scroll(ty, EINA_TRUE);
}

static void
_handle_esc_csi_sd(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);
   int i;

   if (arg == -ESC_ARG_ERROR)
     return;
   if (arg == 0)
     {
        WRN("Track Mouse: TODO");
        return;
     }

   TERMPTY_RESTRICT_FIELD(arg, 1, ty->h);
   DBG("SD - Scroll Down: %d", arg);
   for (i = 0; i < arg; i++)
     termpty_text_scroll_rev(ty, EINA_TRUE);
}

static void
_handle_xterm_unset_title_modes(Termpty *ty EINA_UNUSED,
                                Eina_Unicode **ptr EINA_UNUSED,
                                const Eina_Unicode * const end EINA_UNUSED)
{
   DBG("Unset Title Modes: TODO");
}

static void
_handle_sixel_regis_graphics_attributes(Termpty *ty EINA_UNUSED,
                                        Eina_Unicode **ptr EINA_UNUSED)
{
   DBG("Sixel/ReGIS Graphics Attributes: TODO");
}

static void
_handle_esc_csi_decst8c(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);
   int i;

   if (arg == -ESC_ARG_ERROR)
     return;
   if ((arg != -ESC_ARG_NO_VALUE) && (arg != 5))
     {
        ERR("Invalid DECST8C argument");
        ty->decoding_error = EINA_TRUE;
        return;
     }

   DBG("DECST8C - Set Tab at Every 8 Columns: %d", arg);
   termpty_clear_tabs_on_screen(ty);
   for (i = 0; i < ty->w; i += TAB_WIDTH)
     {
        TAB_SET(ty, i);
     }
}

static void
_handle_esc_csi_ctc(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);

   if (arg == -ESC_ARG_ERROR)
     return;

   DBG("CTC - Cursor Tab Control: %d", arg);
   switch (arg)
     {
      case -ESC_ARG_NO_VALUE:
         EINA_FALLTHROUGH;
      case 0:
        TAB_SET(ty, ty->cursor_state.cx);
        break;
      case 2:
        TAB_UNSET(ty, ty->cursor_state.cx);
        break;
      case 4:
        EINA_FALLTHROUGH;
      case 5:
        termpty_clear_tabs_on_screen(ty);
        break;
      default:
         ERR("invalid CTC argument %d", arg);
         ty->decoding_error = EINA_TRUE;
     }
}

static void
_handle_esc_csi_tbc(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);

   if (arg == -ESC_ARG_ERROR)
     return;
   DBG("TBC - Tab Clear: %d", arg);
   switch (arg)
     {
      case -ESC_ARG_NO_VALUE:
         EINA_FALLTHROUGH;
      case 0:
        TAB_UNSET(ty, ty->cursor_state.cx);
        break;
      case 2:
         EINA_FALLTHROUGH;
      case 3:
         EINA_FALLTHROUGH;
      case 5:
        termpty_clear_tabs_on_screen(ty);
        break;
      default:
        ERR("Tabulation Clear (TBC) with invalid argument: %d", arg);
        ty->decoding_error = EINA_TRUE;
     }
}

static void
_handle_esc_csi_ech(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);

   if (arg == -ESC_ARG_ERROR)
     return;
   DBG("ECH - Erase Character: %d", arg);
   TERMPTY_RESTRICT_FIELD(arg, 1, ty->w);
   termpty_clear_line(ty, TERMPTY_CLR_END, arg);
}

static void
_handle_esc_csi_cbt(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int cx = ty->cursor_state.cx;
   int arg = _csi_arg_get(ty, &b);

   if (arg == -ESC_ARG_ERROR)
     return;
   DBG("CBT - Cursor Horizontal Backward Tabulation: %d", arg);

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

static void
_handle_esc_csi_rep(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);

   if (arg == -ESC_ARG_ERROR)
     return;
   DBG("REP - Repeat last character %d times", arg);

   if (ty->last_char)
     {
        int screen_size = ty->w * ty->h;
        int i;

        if (arg <= 0)
          {
             arg = 1;
          }
        else if (arg > screen_size)
          {
             /* if value is too large, restrict it to something that makes
              * rendering correct (but not in the backlog, but who cares…)
              */
             arg = screen_size + (arg % screen_size);
          }
        for (i = 0; i < arg; i++)
          {
             termpty_text_append(ty, &ty->last_char, 1);
          }
     }
}

static void
_handle_esc_csi_da(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   char bf[32];
   char start = 'X';
   int arg;
   int len;

   if (b)
     {
        start = *b;
     }
   arg = _csi_arg_get(ty, &b);

   if ((arg == -ESC_ARG_ERROR) || (arg > 0))
     return;

   DBG("DA - Device Attributes");
   switch (start)
     {
      case '=':
        /* Tertiary device attributes
         * Device ID is set to ~~TY in ascii:
         * - '~' is 7E
         * - 'T' is 54
         * - 'Y' is 59
         */
        len = snprintf(bf, sizeof(bf), "\033P!|7E7E5459\033\\");
        break;
      case '>':
        /* Secondary device attributes
         *  0 → VT100
         *  1 → VT220
         *  2 → VT240
         * 18 → VT330
         * 19 → VT340
         * 24 → VT320
         * 41 → VT420
         * 61 → VT510
         * 64 → VT520
         * 65 → VT525
         */
        len = snprintf(bf, sizeof(bf), "\033[>61;337;%ic", 0);
        break;
      default:
        /* Primary device attributes
         * 1       132 columns
         * 2       Printer port
         * 4       Sixel
         * 6       Selective erase
         * 7       Soft character set (DRCS)
         * 8       User-defined keys (UDKs)
         * 9       National replacement character sets (NRCS) (International terminal only)
         * 12      Yugoslavian (SCS)
         * 15      Technical character set
         * 18      Windowing capability
         * 21      Horizontal scrolling
         * 22      Color
         * 23      Greek
         * 24      Turkish
         * 42      ISO Latin-2 character set
         * 44      PCTerm
         * 45      Soft key map
         * 46      ASCII emulation
         */
        len = snprintf(bf, sizeof(bf), "\033[?64;1;9;15;18;21;22c");
     }
   termpty_write(ty, bf, len);
}

static void
_handle_esc_csi_uts(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);

   if (arg == -ESC_ARG_ERROR)
     return;
   DBG("UTS - Unset Tab Stop: %d", arg);
   TERMPTY_RESTRICT_FIELD(arg, 0, ty->w);
   TAB_UNSET(ty, arg);
}

static void
_handle_esc_csi_vpa(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);
   int max = ty->h + 1;

   if (arg == -ESC_ARG_ERROR)
     return;
   DBG("VPA - Cursor Vertical Position Absolute: %d", arg);
   if (ty->termstate.restrict_cursor && (ty->termstate.bottom_margin > 0))
     {
        max = ty->termstate.bottom_margin + 1;
     }
   TERMPTY_RESTRICT_FIELD(arg, 1, max);
   ty->cursor_state.wrapnext = 0;
   ty->cursor_state.cy = arg - 1;
}

static void
_handle_esc_csi_decswbv(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);

   if (arg == -ESC_ARG_ERROR)
     return;
   DBG("DECSWBV - Set Warning Bell Volume: %d", arg);
   switch (arg)
     {
      case 1:
         DBG("Bell is off");
         break;
      case 2:
         EINA_FALLTHROUGH;
      case 3:
         EINA_FALLTHROUGH;
      case 4:
         DBG("Bell volume is low");
         break;
      default:
         DBG("Bell volume is high");
     }
}

static void
_handle_resize_by_chars(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int w, h;

   h = _csi_arg_get(ty, &b);
   w = _csi_arg_get(ty, &b);

   if ((w == -ESC_ARG_ERROR) || (h == -ESC_ARG_ERROR))
     return;
   if (w == -ESC_ARG_NO_VALUE)
     {
        w = ty->w;
     }
   if (h == -ESC_ARG_NO_VALUE)
     {
        h = ty->h;
     }
   if ((w == ty->w) && (h == ty->h))
     {
        return;
     }
   DBG("Window manipulation: resize to %dx%d", w, h);

   /* ONLY FOR TESTING PURPOSE FTM */
#if defined(BINARY_TYTEST) || defined(ENABLE_TEST_UI)
     {
#if defined(ENABLE_TEST_UI)
        Evas_Object *wn;
        int step_w = 0, step_h = 0, base_w = 0, base_h = 0;

        wn = termio_win_get(ty->obj);
        elm_win_size_base_get(wn, &base_w, &base_h);
        elm_win_size_step_get(wn, &step_w, &step_h);
        evas_object_resize(wn,
                           base_w + step_w * w,
                           base_h + step_h * h);
#else
        tytest_termio_resize(w, h);
#endif
        termpty_resize(ty, w, h);
     }
#endif
}

struct tag_TitleIconElem {
     const char *title;
     const char *icon;
     struct tag_TitleIconElem *next;
};

static void
_title_icon_stack_push(Termpty *ty, Eina_Unicode **p)
{
   int arg = _csi_arg_get(ty, p);
   TitleIconElem *elem = calloc(sizeof(*elem), 1);

   if (!elem)
     return;


   switch (arg)
     {
      case -ESC_ARG_ERROR:
         free(elem);
         return;
      case -ESC_ARG_NO_VALUE:
         EINA_FALLTHROUGH;
      case 0:
         elem->title = eina_stringshare_ref(ty->prop.title);
         elem->icon = eina_stringshare_ref(ty->prop.icon);
         break;
      case 1:
         elem->icon = eina_stringshare_ref(ty->prop.icon);
         break;
      case 2:
         elem->title = eina_stringshare_ref(ty->prop.title);
         break;
      default:
         break;
     }
   if (!elem->title)
     elem->title = ty->title_icon_stack ?
        eina_stringshare_ref(ty->title_icon_stack->title) :
        eina_stringshare_ref(ty->prop.title);
   if (!elem->icon)
     elem->icon = ty->title_icon_stack ?
        eina_stringshare_ref(ty->title_icon_stack->icon) :
        eina_stringshare_ref(ty->prop.icon);
   elem->next = ty->title_icon_stack;
   ty->title_icon_stack = elem;
}

static void
_title_icon_stack_pop(Termpty *ty, Eina_Unicode **p)
{
   int arg = _csi_arg_get(ty, p);
   TitleIconElem *elem = ty->title_icon_stack;

   if (!elem)
     return;

   switch (arg)
     {
      case -ESC_ARG_ERROR:
         return;
      case -ESC_ARG_NO_VALUE:
         EINA_FALLTHROUGH;
      case 0:
         eina_stringshare_del(ty->prop.icon);
         ty->prop.icon = eina_stringshare_ref(elem->icon);
         if (ty->cb.set_icon.func)
           ty->cb.set_icon.func(ty->cb.set_icon.data);
         eina_stringshare_del(ty->prop.title);
         ty->prop.title = eina_stringshare_ref(elem->title);
         if (ty->cb.set_title.func)
           ty->cb.set_title.func(ty->cb.set_title.data);
         break;
      case 1:
         eina_stringshare_del(ty->prop.icon);
         ty->prop.icon = eina_stringshare_ref(elem->icon);
         if (ty->cb.set_icon.func)
           ty->cb.set_icon.func(ty->cb.set_icon.data);
         break;
      case 2:
         eina_stringshare_del(ty->prop.title);
         ty->prop.title = eina_stringshare_ref(elem->title);
         if (ty->cb.set_title.func)
           ty->cb.set_title.func(ty->cb.set_title.data);
         break;
      default:
         break;
     }
   eina_stringshare_del(elem->icon);
   eina_stringshare_del(elem->title);
   ty->title_icon_stack = elem->next;
   free(elem);
}

static void
_handle_window_manipulation(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int arg = _csi_arg_get(ty, &b);

   if (arg == -ESC_ARG_ERROR)
     return;
   DBG("Window manipulation: %d", arg);
   switch (arg)
     {
      case 8:
         _handle_resize_by_chars(ty, &b);
         break;
      case 22:
        _title_icon_stack_push(ty, &b);
        break;
      case 23:
        _title_icon_stack_pop(ty, &b);
        break;
      default:
        // many others
        WRN("unhandled window manipulation %d", arg);
        ty->decoding_error = EINA_TRUE;
        break;
     }
}


static void
_handle_xmodkeys(Termpty *ty,
                 Eina_Unicode cmd, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   Eina_Bool set = (cmd == 'm');
   Eina_Unicode param = *b;
   b++;
   if (param == '?')
     return; // Not actually supported by xterm
   if (param != '>')
     {
        ERR("XMODKEYS: Invalid sequence");
        ty->decoding_error = EINA_TRUE;
        return;
     }
   if (set)
     {
        int arg1 = _csi_arg_get(ty, &b);
        int arg2 = _csi_arg_get(ty, &b);
        int v, mod;
        if (arg1 == -ESC_ARG_ERROR)
          {
             ERR("XMODKEYS set: Invalid sequence");
             ty->decoding_error = EINA_TRUE;
             return;
          }
        if (arg2 == -ESC_ARG_NO_VALUE)
          {
             mod = arg1;
             v = 0;
          }
        else
          {
             mod = arg2;
             v = arg1;
          }
        switch (mod)
          {
           case XMOD_KEYBOARD:
              EINA_FALLTHROUGH;
           case XMOD_CURSOR:
              EINA_FALLTHROUGH;
           case XMOD_FUNCTIONS:
              EINA_FALLTHROUGH;
           case XMOD_KEYPAD:
              EINA_FALLTHROUGH;
           case XMOD_OTHER:
              EINA_FALLTHROUGH;
           case XMOD_STRING:
              break;
           default:
              ERR("XMODKEYS set: Invalid sequence");
              ty->decoding_error = EINA_TRUE;
              return;
          }
        ty->termstate.xmod[mod] = v;
     }
   else
     { /* reset */
        int arg = _csi_arg_get(ty, &b);
        switch (arg)
          {
           case XMOD_KEYBOARD:
              EINA_FALLTHROUGH;
           case XMOD_CURSOR:
              EINA_FALLTHROUGH;
           case XMOD_FUNCTIONS:
              EINA_FALLTHROUGH;
           case XMOD_KEYPAD:
              EINA_FALLTHROUGH;
           case XMOD_OTHER:
              EINA_FALLTHROUGH;
           case XMOD_STRING:
              break;
           default:
              ERR("XMODKEYS reset: Invalid sequence");
              ty->decoding_error = EINA_TRUE;
              return;
          }
        ty->termstate.xmod[arg] = 0;
     }
}
static int
_handle_esc_csi(Termpty *ty, const Eina_Unicode *c, const Eina_Unicode *ce)
{
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
   DBG(" CSI: '%s' args '%s'", termptyesc_safechar(*cc), (char *) buf);
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
        _handle_esc_csi_cud_or_vpr(ty, &b, cc);
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
        _handle_esc_csi_cha(ty, &b, cc);
        break;
      case 'H':
        _handle_esc_csi_cursor_pos_set(ty, &b, cc);
        break;
      case 'I':
        _handle_esc_csi_cht(ty, &b);
        break;
      case 'J':
        if (*b == '?')
          _handle_esc_csi_decsed(ty, &b);
        else
          _handle_esc_csi_ed(ty, &b);
        break;
      case 'K':
        if (*b == '?')
          _handle_esc_csi_decsel(ty, &b);
        else
          _handle_esc_csi_el(ty, &b);
        break;
      case 'L':
        _handle_esc_csi_il(ty, &b);
        break;
      case 'M':
        _handle_esc_csi_dl(ty, &b);
        break;
      case 'P':
        _handle_esc_csi_dch(ty, &b);
        break;
      case 'S':
        if (*b == '?')
          _handle_sixel_regis_graphics_attributes(ty, &b);
        else
          _handle_esc_csi_su(ty, &b);
        break;
      case 'T':
        if (*b == '?')
          _handle_xterm_unset_title_modes(ty, &b, be);
        else
          _handle_esc_csi_sd(ty, &b);
        break;
      case 'W':
        if (*b == '?')
          _handle_esc_csi_decst8c(ty, &b);
        else
          _handle_esc_csi_ctc(ty, &b);
        break;
      case 'X':
        _handle_esc_csi_ech(ty, &b);
        break;
      case 'Z':
        _handle_esc_csi_cbt(ty, &b);
        break;
      case '`':
        _handle_esc_csi_cha(ty, &b, cc);
        break;
      case 'a':
        _handle_esc_csi_cuf(ty, &b);
        break;
      case 'b':
        _handle_esc_csi_rep(ty, &b);
        break;
      case 'c':
        _handle_esc_csi_da(ty, &b);
        break;
      case 'd':
        if (*(cc-1) == ' ')
          _handle_esc_csi_uts(ty, &b);
        else
          _handle_esc_csi_vpa(ty, &b);
        break;
      case 'e':
        _handle_esc_csi_cud_or_vpr(ty, &b, cc);
        break;
      case 'f':
        _handle_esc_csi_cursor_pos_set(ty, &b, cc);
       break;
      case 'g':
        _handle_esc_csi_tbc(ty, &b);
        break;
      case 'h':
        _handle_esc_csi_reset_mode(ty, *cc, b, be);
        break;
      case 'i':
        WRN("TODO: Media Copy (?:%s)", (*b == '?') ? "yes": "no");
        ty->decoding_error = EINA_TRUE;
        break;
      case 'j':
        _handle_esc_csi_cub(ty, &b);
        break;
      case 'k':
        _handle_esc_csi_cuu(ty, &b);
        break;
      case 'l':
        _handle_esc_csi_reset_mode(ty, *cc, b, be);
        break;
      case 'm': // color set
        if (b && (*b == '>' || *b == '?'))
          _handle_xmodkeys(ty, *cc, &b);
        else
          _handle_esc_csi_color_set(ty, &b, be);
        break;
      case 'n':
        if (*b == '>')
          _handle_xmodkeys(ty, *cc, &b);
        else
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
             if (*b == '>')
               _handle_esc_csi_term_version(ty, &b);
             else
               {
                  WRN("TODO: Load LEDs (DECLL)");
                  ty->decoding_error = EINA_TRUE;
               }
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
        else if (*(cc-1) == ' ')
          {
             _handle_esc_csi_decswbv(ty, &b);
          }
        else
          {
             _handle_window_manipulation(ty, &b);
          }
        break;
      case 'u': // restore cursor pos
        termpty_cursor_copy(ty, EINA_FALSE);
        break;
      case 'v':
        if (*(cc-1) == '$')
          _handle_esc_csi_deccra(ty, &b);
        else
          {
             ERR("unhandled 'v' CSI escape code");
             ty->decoding_error = EINA_TRUE;
          }
        break;
      case 'x':
        if (*(cc-1) == '$')
          _handle_esc_csi_decfra(ty, &b);
        else if (*(cc-1) == '*')
          _handle_esc_csi_decsace(ty, &b);
        else
          {
             ERR("unhandled 'x' CSI escape code");
             ty->decoding_error = EINA_TRUE;
          }
        break;
      case 'z':
        if (*(cc-1) == '$')
          _handle_esc_csi_decera(ty, &b);
        else
          {
             ERR("unhandled 'z' CSI escape code");
             ty->decoding_error = EINA_TRUE;
          }
        break;
      case '}':
        if (*(cc-1) == '\'')
          _handle_esc_csi_decic(ty, &b);
        else
          {
             ERR("unhandled '}' CSI escape code");
             ty->decoding_error = EINA_TRUE;
          }
        break;
      case '~':
        if (*(cc-1) == '\'')
          _handle_esc_csi_decdc(ty, &b);
        else
          {
             ERR("unhandled '~' CSI escape code");
             ty->decoding_error = EINA_TRUE;
          }
        break;
      default:
        ERR("unhandled '0x%x' CSI escape code", (int)*cc);
        goto unhandled;
     }
   cc++;
   return cc - c;
unhandled:
#if !defined(BINARY_TYFUZZ) && !defined(BINARY_TYTEST)
   if (eina_log_domain_level_check(_termpty_log_dom, EINA_LOG_LEVEL_WARN))
     {
        int i;
        Eina_Strbuf *bf = eina_strbuf_new();

        for (i = 0; c + i <= cc && i < 100; i++)
          {
             if ((c[i] < ' ') || (c[i] >= 0x7f))
               eina_strbuf_append_printf(bf, "\033[35m%08x\033[0m",
                                         (unsigned int) c[i]);
             else
               eina_strbuf_append_char(bf, c[i]);
          }
        WRN("unhandled CSI '%s': %s", termptyesc_safechar(*cc), eina_strbuf_string_get(bf));
        eina_strbuf_free(bf);
     }
#endif
   ty->decoding_error = EINA_TRUE;
   cc++;
   return cc - c;
}

static int
_osc_arg_get(Termpty *ty, Eina_Unicode **ptr)
{
   Eina_Unicode *b = *ptr;
   int sum = 0;

   if ((b == NULL) || (*b == '\0'))
     {
        *ptr = NULL;
        sum = -ESC_ARG_NO_VALUE;
        goto error;
     }

   while (*b >= '0' && *b <= '9')
     {
        sum *= 10;
        sum += *b - '0';
        b++;
        if (sum >= 65536)
          {
             sum = -ESC_ARG_ERROR;
             goto error;
          }
     }
   if (*b != ';' && *b != BEL && *b != 0)
     {
        sum = -ESC_ARG_ERROR;
        goto error;
     }
   else
     b++;
   *ptr = b;

   return sum;

error:
   ERR("Invalid OSC argument");
   ty->decoding_error = EINA_TRUE;
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
_xterm_parse_color_sharp(Eina_Unicode *p,
                   unsigned char *r, unsigned char *g, unsigned char *b,
                   int len)
{
   int i;

   switch (len)
     {
      case 3*4+1:
         i = _eina_unicode_to_hex(p[0]);
         if (i < 0) return -1;
         *r = i;
         i = _eina_unicode_to_hex(p[1]);
         if (i < 0) return -1;
         *r = *r * 16 + i;
         i = _eina_unicode_to_hex(p[2]);
         if (i < 0) return -1;
         i = _eina_unicode_to_hex(p[3]);
         if (i < 0) return -1;

         i = _eina_unicode_to_hex(p[4]);
         if (i < 0) return -1;
         *g = i;
         i = _eina_unicode_to_hex(p[5]);
         if (i < 0) return -1;
         *g = *g * 16 + i;
         i = _eina_unicode_to_hex(p[6]);
         if (i < 0) return -1;
         i = _eina_unicode_to_hex(p[7]);
         if (i < 0) return -1;

         i = _eina_unicode_to_hex(p[8]);
         if (i < 0) return -1;
         *b = i;
         i = _eina_unicode_to_hex(p[9]);
         if (i < 0) return -1;
         *b = *b * 16 + i;
         i = _eina_unicode_to_hex(p[10]);
         if (i < 0) return -1;
         i = _eina_unicode_to_hex(p[11]);
         if (i < 0) return -1;
         break;
      case 3*3+1:
         i = _eina_unicode_to_hex(p[0]);
         if (i < 0) return -1;
         *r = i;
         i = _eina_unicode_to_hex(p[1]);
         if (i < 0) return -1;
         *r = *r * 16 + i;
         i = _eina_unicode_to_hex(p[3]);
         if (i < 0) return -1;

         i = _eina_unicode_to_hex(p[4]);
         if (i < 0) return -1;
         *g = i;
         i = _eina_unicode_to_hex(p[5]);
         if (i < 0) return -1;
         *g = *g * 16 + i;
         i = _eina_unicode_to_hex(p[3]);
         if (i < 0) return -1;

         i = _eina_unicode_to_hex(p[7]);
         if (i < 0) return -1;
         *b = i;
         i = _eina_unicode_to_hex(p[8]);
         if (i < 0) return -1;
         *b = *b * 16 + i;
         i = _eina_unicode_to_hex(p[3]);
         if (i < 0) return -1;
         break;
      case 3*2+1:
         i = _eina_unicode_to_hex(p[0]);
         if (i < 0) return -1;
         *r = i;
         i = _eina_unicode_to_hex(p[1]);
         if (i < 0) return -1;
         *r = *r * 16 + i;

         i = _eina_unicode_to_hex(p[2]);
         if (i < 0) return -1;
         *g = i;
         i = _eina_unicode_to_hex(p[3]);
         if (i < 0) return -1;
         *g = *g * 16 + i;

         i = _eina_unicode_to_hex(p[4]);
         if (i < 0) return -1;
         *b = i;
         i = _eina_unicode_to_hex(p[5]);
         if (i < 0) return -1;
         *b = *b * 16 + i;
         break;
      case 3*1+1:
         i = _eina_unicode_to_hex(p[0]);
         if (i < 0) return -1;
         *r = i;
         i = _eina_unicode_to_hex(p[1]);
         if (i < 0) return -1;
         *g = i;
         i = _eina_unicode_to_hex(p[2]);
         if (i < 0) return -1;
         *b = i;
         break;
      default:
         return -1;
     }
   return 0;
}

/* isnan() in musl generates  ' runtime error: negation of 1 cannot be
 * represented in type 'unsigned long long'
 * under ubsan
 */
#if defined(__clang__) && !defined(__GLIBC__)
__attribute__((no_sanitize("unsigned-integer-overflow")))
#endif
/* returns len read or -1 in case of error */
static int
_xterm_parse_intensity(Eina_Unicode *p, unsigned char *c, int len)
{
   int l = 0;
   char buf[64];
   char *endptr_double;
   double d;

   while (l < 63 && len && p[0] && p[0] != '/' && p[0] != '\007' && p[0] < 128)
     {
        buf[l++] = p[0];
        len--;
        p++;
     }
   if (l == 0)
     return -1;
   buf[l] = '\0';

   d = eina_convert_strtod_c(buf, &endptr_double);
   if (endptr_double == buf || d < 0 || d > 1.0 || isnan(d))
       return -1;

   d *= 255.0;
   if (d > 255.0)
     *c = 255;
   else
     *c = round(d);
   return endptr_double - buf;
}

static int
_xterm_parse_color_rgbi(Eina_Unicode *p,
                        unsigned char *r,
                        unsigned char *g,
                        unsigned char *b,
                        int len)
{
   int l;

   /* parse r */
   l = _xterm_parse_intensity(p, r, len);
   if (l <= 0)
     return -1;
   p += l;
   if (p[0] != '/')
     return -1;
   p++;
   /* parse g */
   l = _xterm_parse_intensity(p, g, len);
   if (l <= 0)
     return -1;
   p += l;
   if (p[0] != '/')
     return -1;
   p++;
   /* parse b */
   l = _xterm_parse_intensity(p, b, len);
   if (l <= 0)
     return -1;

   return 0;
}

/* returns len read or -1 in case of error */
static int
_xterm_parse_hex(Eina_Unicode *p, unsigned char *c, int len)
{
   int l = 0;
   int v = 0;
   int i;

   i = _eina_unicode_to_hex(p[0]);
   if (i < 0) return -1;
   p++;
   l++;
   len--;
   if (len <= 0)
     goto end;
   v = i;

   i = _eina_unicode_to_hex(p[0]);
   if (i < 0) goto end;
   p++;
   l++;
   len--;
   if (len <= 0)
     goto end;
   v = v * 16 + i;

   i = _eina_unicode_to_hex(p[0]);
   if (i < 0) goto end;
   p++;
   l++;
   len--;
   if (len <= 0)
     goto end;

   i = _eina_unicode_to_hex(p[0]);
   if (i < 0) goto end;
   p++;
   l++;
   len--;
   if (len <= 0)
     goto end;

end:
   if (l == 1)
     v <<= 4;
   *c = v;
   return l;
}

static int
_xterm_parse_color_rgb(Eina_Unicode *p,
                       unsigned char *r, unsigned char *g, unsigned char *b,
                       int len)
{
   int l;

   /* parse r */
   l = _xterm_parse_hex(p, r, len);
   if (l <= 0)
     return -1;
   p += l;
   if (p[0] != '/')
     return -1;
   p++;
   /* parse g */
   l = _xterm_parse_hex(p, g, len);
   if (l <= 0)
     return -1;
   p += l;
   if (p[0] != '/')
     return -1;
   p++;
   /* parse b */
   l = _xterm_parse_hex(p, b, len);
   if (l <= 0)
     return -1;

   return 0;
}


static int
_xterm_parse_color(Termpty *ty, Eina_Unicode **ptr,
                   unsigned char *r, unsigned char *g, unsigned char *b,
                   int len)
{
   Eina_Unicode *p = *ptr;

   if (*p == '#')
     {
        p++;
        len--;
        if (_xterm_parse_color_sharp(p, r, g, b, len))
          goto err;
     }
   else if (len > 5 && p[0] == 'r' && p[1] == 'g' && p[2] == 'b')
     {
        if (p[3] == 'i' && p[4] == ':')
          {
             if (_xterm_parse_color_rgbi(p+5, r, g, b, len-5))
               goto err;
          }
        else if (p[3] == ':')
          {
             if (_xterm_parse_color_rgb(p+4, r, g, b, len-4))
               goto err;
          }
        else
          goto err;
     }
   else
     goto err;

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

static void
_handle_xterm_10_command(Termpty *ty, Eina_Unicode *p, int len)
{
   if (!p || !*p)
     goto err;
   if (*p == '?')
     {
        int r, g, b;
        char bf[32];
        int l;
#if !defined(BINARY_TYFUZZ) && !defined(BINARY_TYTEST)
        evas_object_textgrid_palette_get(
           termio_textgrid_get(ty->obj),
           EVAS_TEXTGRID_PALETTE_STANDARD, 0,
           &r, &g, &b, NULL);
#else
        Config *config = termio_config_get(ty->obj);
        r = config->colors[0].r;
        g = config->colors[0].g;
        b = config->colors[0].b;
#endif
        l = snprintf(bf, sizeof(bf), "\033]10;#%.2X%.2X%.2X\007", r, g, b);
        termpty_write(ty, bf, l);
     }
   else
     {
        unsigned char r, g, b;
        if (_xterm_parse_color(ty, &p, &r, &g, &b, len) < 0)
          goto err;
#if !defined(BINARY_TYFUZZ) && !defined(BINARY_TYTEST)
        evas_object_textgrid_palette_set(
           termio_textgrid_get(ty->obj),
           EVAS_TEXTGRID_PALETTE_STANDARD, 0,
           r, g, b, 0xff);
#endif
     }
   return;
err:
   ty->decoding_error = EINA_TRUE;
}

static void
_handle_xterm_set_color_class(Termpty *ty, Eina_Unicode *p, int len,
                              Evas_Object *obj,
                              const char *color_class,
                              uint8_t number)
{
   if (!p || !*p)
     goto err;
#if !defined(BINARY_TYFUZZ) && !defined(BINARY_TYTEST)
   if (!obj)
     goto err;
#endif

   if (*p == '?')
     {
        int r = 0, g = 0, b = 0;
        char buf[64];
        int l;

        if (edje_object_color_class_get(obj, color_class, &r, &g, &b, NULL,
                                        NULL, NULL, NULL, NULL,
                                        NULL, NULL, NULL, NULL) != EINA_TRUE)
          {
             ERR("error getting color class '%s' on obj %p", color_class, obj);
          }
        l = snprintf(buf, sizeof(buf),
                     "\033]%d;rgb:%.2x%.2x/%.2x%.2x/%.2x%.2x\033\\",
                     number, r, r, g, g, b, b);
        termpty_write(ty, buf, l);
     }
   else
     {
        unsigned char r, g, b;
        if (_xterm_parse_color(ty, &p, &r, &g, &b, len) < 0)
          goto err;
        if (edje_object_color_class_set(obj, color_class,
                                        r, g, b, 0xff,
                                        r, g, b, 0xff,
                                        r, g, b, 0xff) != EINA_TRUE)
          {
             ERR("error setting color class '%s' on obj %p", color_class, obj);
          }
     }

   return;
err:
   ty->decoding_error = EINA_TRUE;
}

static Elm_Sel_Type
_elm_sel_type_from_osc52(Eina_Unicode c)
{
   Elm_Sel_Type sel_type;
   switch (c)
     {
      case 'c':
         sel_type = ELM_SEL_TYPE_CLIPBOARD;
         break;
      case ';':
         EINA_FALLTHROUGH;
      case 'p':
         EINA_FALLTHROUGH;
      default:
         sel_type = ELM_SEL_TYPE_PRIMARY;
         break;
     }
   return sel_type;
}

typedef struct _Osc52_Cb {
     Termpty *ty;
     Elm_Sel_Type sel;
     Eina_Bool has_data;
} Osc52_Cb;

static Eina_Bool
_osc52_report_cb(void *data, Evas_Object *obj EINA_UNUSED, Elm_Selection_Data *ev)
{
   Osc52_Cb *cb = data;
   Termpty *ty = cb->ty;
   if (ev && ev->len > 0)
     {
        Eina_Binbuf *bb = eina_binbuf_new();
        Eina_Strbuf *sb;
        char bf[32];
        size_t len;
        char c;

        if (!bb || ev->len <= 1)
          return EINA_FALSE;

        eina_binbuf_append_length(bb, ev->data, ev->len-1);
        sb = emile_base64_encode(bb);
        if (!sb)
          goto end;
        switch (cb->sel)
          {
           case ELM_SEL_TYPE_CLIPBOARD:
              c = 'c';
              break;
           default:
              c = 'p';
              break;
          }
        /* Write header */
        len = snprintf(bf, sizeof(bf), "\033]52;%c;", c);
        termpty_write(ty, bf, len);
        /* Write data */
        termpty_write(ty, eina_strbuf_string_get(sb), eina_strbuf_length_get(sb));
        /* Write end*/
        TERMPTY_WRITE_STR("\033\\");
        cb->has_data = EINA_TRUE;
end:
        eina_binbuf_free(bb);
        eina_strbuf_free(sb);
     }

   return EINA_TRUE;
}

static void
_handle_osc_selection(Termpty *ty, Eina_Unicode *p, int len)
{
   Eina_Unicode *c;
   Elm_Sel_Type sel_type;

   if (!p || !*p || len <= 0)
     goto err;
   c = p;
   while (*c != ';' && (c - p) < len)
     c++;
   if (*c != ';')
     goto err;
   c++;
   if (*c == '?')
     {
        /* Report */
        Osc52_Cb cb;

        cb.ty = ty;
        cb.has_data = EINA_FALSE;
        c = p;
        while (!cb.has_data && *c != ';')
          {
             sel_type = _elm_sel_type_from_osc52(*p);
             cb.sel = sel_type;
             termio_selection_buffer_get_cb(ty->obj, cb.sel, ELM_SEL_FORMAT_TEXT,
                                   _osc52_report_cb, &cb);
             c++;
          }
        if (!cb.has_data)
          {
             cb.sel = ELM_SEL_TYPE_PRIMARY;
             termio_selection_buffer_get_cb(ty->obj, cb.sel, ELM_SEL_FORMAT_TEXT,
                                   _osc52_report_cb, &cb);
          }
     }
   else
     {
        /* Set */
        sel_type = _elm_sel_type_from_osc52(*p);
        /* Decode base64 from the request */
        p[len] = '\0';
        char *out = ty_eina_unicode_base64_decode(c);

        if (out)
          {
             termio_set_selection_text(ty->obj, sel_type, out);
             free(out);
          }
     }
   return;
err:
   ty->decoding_error = EINA_TRUE;
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
             (*cc == ST)))
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

   arg = _osc_arg_get(ty, &p);
   switch (arg)
     {
      case -ESC_ARG_ERROR:
         goto err;
      case -ESC_ARG_NO_VALUE:
         EINA_FALLTHROUGH;
      case 0:
        // title + icon name
        if (!p || !*p)
          goto err;
        s = eina_unicode_unicode_to_utf8(p, &len);
        eina_stringshare_del(ty->prop.title);
        eina_stringshare_del(ty->prop.icon);
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
        if (ty->cb.set_icon.func)
          ty->cb.set_icon.func(ty->cb.set_icon.data);
        break;
      case 1:
        // icon name
        if (!p || !*p)
          goto err;
        s = eina_unicode_unicode_to_utf8(p, &len);
        eina_stringshare_del(ty->prop.icon);
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
        if (!p || !*p)
          goto err;
        s = eina_unicode_unicode_to_utf8(p, &len);
        eina_stringshare_del(ty->prop.title);
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
        if (!p || !*p)
          goto err;
        // XXX: set palette entry. not supported.
        ty->decoding_error = EINA_TRUE;
        WRN("set palette, not supported");
        if ((cc - c) < 3)
          return 0;
        break;
      case 8:
        DBG("hyperlink");
        if (!p || !*p)
          goto err;
        s = eina_unicode_unicode_to_utf8(p, &len);
        _handle_hyperlink(ty, s, len);
        break;
      case 10:
        DBG("Set foreground color");
        _handle_xterm_10_command(ty, p, cc - c - (p - buf));
        break;
      case 11:
        DBG("Set background color");
        _handle_xterm_set_color_class(ty, p, cc - c - (p - buf),
                                      termio_bg_get(ty->obj),
                                      "BG", 11);
        break;
      case 12:
        DBG("Set cursor color");
        _handle_xterm_set_color_class(ty, p, cc - c - (p - buf),
                                      termio_get_cursor(ty->obj),
                                      "CURSOR", 12);
        break;
      case 50:
        DBG("xterm font support");
        if (!p || !*p)
          goto err;
        s = eina_unicode_unicode_to_utf8(p, &len);
        if (s)
          {
             _handle_xterm_50_command(ty, s, len);
             free(s);
          }
        break;
      case 52:
        DBG("Manipulate selection data");
        if (ty->config->selection_escapes)
          _handle_osc_selection(ty, p, cc - c - (p - buf));
        break;
      case 110:
        DBG("Reset VT100 text foreground color");
        break;
      case 111:
        DBG("Reset VT100 text background color");
        break;
      case 112:
        DBG("Reset text cursor color");
        break;
      case 113:
        DBG("Reset mouse foreground color");
        break;
      case 114:
        DBG("Reset mouse background color");
        break;
      case 115:
        DBG("Reset Tektronix foreground color");
        break;
      case 116:
        DBG("Reset Tektronix background color");
        break;
      case 117:
        DBG("Reset highlight color");
        break;
      case 118:
        DBG("Reset Tektronix cursor color");
        break;
      case 119:
        DBG("Reset highlight foreground color");
        break;
      case 777:
        DBG("xterm notification support");
        if (!p || !*p)
          goto err;
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
   size_t blen = 0;
   Config *config;

   if (!ty->buf_have_zero)
     return 0;

   config = termio_config_get(ty->obj);

   cc = (Eina_Unicode *)c;
   if ((cc < ce) && (*cc == 0x0))
     cc_zero = cc;
   while ((cc < ce) && (*cc != 0x0))
     {
        blen++;
        cc++;
     }
   if ((cc < ce) && (*cc == 0x0))
     cc_zero = cc;
   if (!cc_zero)
     return 0;
   buf = (Eina_Unicode *)c;
   cc = cc_zero;

   // commands are stored in the buffer, 0 bytes not allowed (end marker)
   cmd = eina_unicode_unicode_to_utf8(buf, NULL);
   ty->cur_cmd = cmd;
   if ((!config->ty_escapes) || (!termpty_ext_handle(ty, buf, blen)))
     {
        if (ty->cb.command.func)
          ty->cb.command.func(ty->cb.command.data);
     }
   ty->cur_cmd = NULL;
   free(cmd);

   assert((size_t)(cc - c) == blen);

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
             (*cc == ST)))
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
              WRN("invalid/unhandled dsc esc '$%s' (expected '$q')",
                  termptyesc_safechar(buf[1]));
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
                    WRN("invalid/unhandled dsc esc '$q\"%s'",
                        termptyesc_safechar(buf[3]));
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
               WRN("unhandled dsc request status string '$q%s'",
                   termptyesc_safechar(buf[2]));
               goto end;
           }
         break;
      default:
        // many others
        WRN("Unhandled DCS escape '%s'", termptyesc_safechar(buf[0]));
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
             ty->cursor_state.wrapnext = 0;
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
        /* cursor forward */
        ty->cursor_state.cx++;
     }
}

static int
_handle_esc(Termpty *ty, const Eina_Unicode *c, const Eina_Unicode *ce)
{
   int len = ce - c;

   if (len < 1) return 0;
   DBG("ESC: '%s'", termptyesc_safechar(c[0]));
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
        ty->cursor_state.wrapnext = 0;
        ty->cursor_state.cy--;
        termpty_text_scroll_rev_test(ty, EINA_TRUE);
        return 1;
      case 'D': // move to next line
        DBG("move to next line");
        ty->cursor_state.wrapnext = 0;
        ty->cursor_state.cy++;
        termpty_text_scroll_test(ty, EINA_FALSE);
        return 1;
      case 'E': // add \n\r
        DBG("add \\n\\r");
        ty->cursor_state.wrapnext = 0;
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
      case '"':
        if (len < 2)
          return 0;
        /* Seems like this sequence activates C1... */
        if (c[1] != 'C')
          {
             ERR("invalid 0x1b 0x22 sequence");
             ty->decoding_error = 0;
          }
        return 2;
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
        WRN("Unhandled escape '%s' (0x%02x)",
            termptyesc_safechar(c[0]), (unsigned int) c[0]);
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
   else if (c[0] == DEL)
     {
        WRN("Unhandled char 0x%02x [DEL]", (unsigned int) c[0]);
        ty->decoding_error = EINA_TRUE;
        len = 1;
        goto end;
     }
   else if (c[0] == CSI)
     {
        len = _handle_esc_csi(ty, c + 1, ce);
        if (len == 0)
          goto end;
        len++;
        goto end;
     }
   else if (c[0] == OSC)
     {
        len = _handle_esc_osc(ty, c + 1, ce);
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
   while ((cc < ce) && (*cc >= 0x20) && (*cc != DEL) && (*cc != CSI)
          && (*cc != OSC))
     {
        DBG("%s", termptyesc_safechar(*cc));
        cc++;
        len++;
     }
   DBG("]");
   termpty_text_append(ty, c, len);
   if (len > 0)
       last_char = c[len-1];

end:
#if !defined(BINARY_TYFUZZ) && !defined(BINARY_TYTEST)
   if (ty->decoding_error)
     {
      int j;
      for (j = 0; c + j < ce && j < len; j++)
        {
           if ((c[j] < ' ') || (c[j] >= 0x7f))
             printf("\033[35m%08x\033[0m", (int)c[j]);
           else
             printf("%s", termptyesc_safechar(c[j]));
        }
      printf("\n");
     }
#endif
   ty->decoding_error = EINA_FALSE;
   ty->last_char = last_char;
   return len;
}
