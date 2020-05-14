#include "private.h"
#include <Elementary.h>
#include "termio.h"
#include "termiolink.h"
#include "termpty.h"
#include "termptyops.h"
#include "termiointernals.h"
#if defined(BINARY_TYTEST) || defined(ENABLE_TEST_UI)
#include "tytest.h"
#endif
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

//// extended terminology escape handling goes in here
//
// this is where escapes get handled *IF* the termpty layer needs to interpret
// them itself for some reason. if it returns EINA_FALSE, it means the escape
// is to be passed onto termio layer as a callback and handled there after
// this code. an extended escape may be handled in here exclusively (return
// EINA_TRUE), handled here first, then in termio (EINA_FALSE return) or not
// handled here at all and just passed to termio to figure it out (return
// EINA_FALSE).

#if defined(BINARY_TYTEST) || defined(ENABLE_TEST_UI)

static int _mx;
static int _my;

void
test_set_mouse_pointer(int mx, int my)
{
   _mx = mx;
   _my = my;
}

void
test_pointer_canvas_xy_get(int *mx, int *my)
{
   if (mx)
     *mx = _mx;
   if (my)
     *my = _my;
}

static int
_tytest_arg_get(const Eina_Unicode *buf, int *value)
{
   int len = 0;
   int sum = 0;

   if (*buf == ';')
     {
        len++;
     }

   while (buf[len] >= '0' && buf[len] <= '9')
     {
        sum *= 10;
        sum += buf[len] - '0';
        len++;
     }
   *value = sum;
   return len;
}

/**
 * MODIFIERS is a bit field with the following values:
 *   - Alt
 *   - Shift
 *   - Ctrl
 *   - Super
 *   - Meta
 *   - Hyper
 *   - ISO_Level3_Shift
 *   - AltGr
 */
static int
_tytest_modifiers_get(const Eina_Unicode *buf, Termio_Modifiers *m)
{
   Termio_Modifiers modifier = {};
   int value = 0;
   int len = _tytest_arg_get(buf, &value);

   modifier.alt = !!(value & (1 << 0));
   modifier.shift = !!(value & (1 << 1));
   modifier.ctrl = !!(value & (1 << 2));
   modifier.super = !!(value & (1 << 3));
   modifier.meta = !!(value & (1 << 4));
   modifier.hyper = !!(value & (1 << 5));
   modifier.iso_level3_shift = !!(value & (1 << 6));
   modifier.altgr = !!(value & (1 << 7));

   *m = modifier;
   return len;
}

/**
 * FLAGS can be:
 * - 0
 * - 1: DOUBLE_CLICK
 * - 2: TRIPLE_CLICK
 */

/*
 * Format is td;X;Y;BUTTON;MODIFIERS;FLAGS
 */
static void
_handle_mouse_down(Termpty *ty,
                   const Eina_Unicode *buf)
{
   Evas_Event_Mouse_Down ev = {};
   Termio *sd = termio_get_from_obj(ty->obj);
   Termio_Modifiers modifiers = {};
   int value;

   /* X */
   value = 0;
   buf += _tytest_arg_get(buf, &value);
   ev.canvas.x = value;
   /* Y */
   value = 0;
   buf += _tytest_arg_get(buf, &value);
   ev.canvas.y = value;
   /* BUTTON */
   value = 0;
   buf += _tytest_arg_get(buf, &value);
   ev.button = value;
   /* MODIFIERS */
   buf += _tytest_modifiers_get(buf, &modifiers);
   /* FLAGS */
   value = 0;
   _tytest_arg_get(buf, &value);
   ev.flags = value;

#if defined(BINARY_TYTEST) || defined(ENABLE_TEST_UI)
   test_set_mouse_pointer(ev.canvas.x, ev.canvas.y);
#endif
   termio_internal_mouse_down(sd, &ev, modifiers);
}

/*
 * Format is tu;X;Y;BUTTON;MODIFIERS;FLAGS
 */
static void
_handle_mouse_up(Termpty *ty,
                 const Eina_Unicode *buf)
{
   Evas_Event_Mouse_Up ev = {};
   Termio *sd = termio_get_from_obj(ty->obj);
   Termio_Modifiers modifiers = {};
   int value;

   /* X */
   value = 0;
   buf += _tytest_arg_get(buf, &value);
   ev.canvas.x = value;
   /* Y */
   value = 0;
   buf += _tytest_arg_get(buf, &value);
   ev.canvas.y = value;
   /* BUTTON */
   value = 0;
   buf += _tytest_arg_get(buf, &value);
   ev.button = value;
   /* MODIFIERS */
   buf += _tytest_modifiers_get(buf, &modifiers);
   /* FLAGS */
   value = 0;
   _tytest_arg_get(buf, &value);
   ev.flags = value;

#if defined(BINARY_TYTEST) || defined(ENABLE_TEST_UI)
   test_set_mouse_pointer(ev.canvas.x, ev.canvas.y);
#endif
   termio_internal_mouse_up(sd, &ev, modifiers);
}

/*
 * Format is tm;X;Y;MODIFIERS
 */
static void
_handle_mouse_move(Termpty *ty,
                   const Eina_Unicode *buf)
{
   Evas_Event_Mouse_Move ev = {};
   Termio *sd = termio_get_from_obj(ty->obj);
   Termio_Modifiers modifiers = {};
   int value;

   /* X */
   value = 0;
   buf += _tytest_arg_get(buf, &value);
   ev.cur.canvas.x = value;
   /* Y */
   value = 0;
   buf += _tytest_arg_get(buf, &value);
   ev.cur.canvas.y = value;
   /* MODIFIERS */
   _tytest_modifiers_get(buf, &modifiers);

#if defined(BINARY_TYTEST) || defined(ENABLE_TEST_UI)
   test_set_mouse_pointer(ev.cur.canvas.x, ev.cur.canvas.y);
#endif
   termio_internal_mouse_move(sd, &ev, modifiers);
}

/*
 * Format is tw;X;Y;DIRECTION;VALUE;MODIFIERS
 * DIRECTION: 1 to go up, 0 to go down
 */
static void
_handle_mouse_wheel(Termpty *ty,
                    const Eina_Unicode *buf)
{
   Evas_Event_Mouse_Wheel ev = {};
   Termio *sd = termio_get_from_obj(ty->obj);
   Termio_Modifiers modifiers = {};
   int value;

   /* X */
   value = 0;
   buf += _tytest_arg_get(buf, &value);
   ev.canvas.x = value;
   /* Y */
   value = 0;
   buf += _tytest_arg_get(buf, &value);
   ev.canvas.y = value;
   /* DIRECTION */
   value = 0;
   buf += _tytest_arg_get(buf, &value);
   ev.z = (value == 0)? 1 : -1;
   /* VALUE */
   value = 0;
   buf += _tytest_arg_get(buf, &value);
   ev.z *= value;
   /* MODIFIERS */
   _tytest_modifiers_get(buf, &modifiers);

#if defined(BINARY_TYTEST) || defined(ENABLE_TEST_UI)
   test_set_mouse_pointer(ev.canvas.x, ev.canvas.y);
#endif
   termio_internal_mouse_wheel(sd, &ev, modifiers);
}

static void
_handle_color_link(Termpty *ty, const Eina_Unicode *buf)
{
   uint8_t r = 0, g = 0, b = 0, a = 0;
   int value;
   Eina_Bool found;
   int x1 = -1, y1 = -1, x2 = -1, y2 = -1;
   Termio *sd = termio_get_from_obj(ty->obj);

   found = termio_color_find(ty->obj, sd->mouse.cx, sd->mouse.cy,
                             &x1, &y1, &x2, &y2,
                             &r, &g, &b, &a);
   if (found)
     {
        ERR("printf '\\033}tlc;%d;%d;%d;%d;%d;%d;%d;%d\\0'",
           x1,y1,x2,y2, r,g,b,a);
     }
   else
     {
        ERR("printf '\\033}tlcn\\0'");
     }
   if (*buf == 'n')
     {
        assert(!found);
        return;
     }
   assert(found);

   /* Get numeric values */
   buf += _tytest_arg_get(buf, &value);
   assert(x1 == value);
   buf += _tytest_arg_get(buf, &value);
   assert(y1 == value);
   buf += _tytest_arg_get(buf, &value);
   assert(x2 == value);
   buf += _tytest_arg_get(buf, &value);
   assert(y2 == value);
   /* skip ; */
   buf++;

   /* Get numeric values */
   buf += _tytest_arg_get(buf, &value);
   assert(r == value);
   buf += _tytest_arg_get(buf, &value);
   assert(g == value);
   buf += _tytest_arg_get(buf, &value);
   assert(b == value);
   _tytest_arg_get(buf, &value);
   assert(a == value);
}

/*
 * Format is:
 * - ln : no link found under cursor
 * - lT;X1;Y1;X2;Y2;LINK
 *   where T is
 *     e: link is an email
 *     u: link is an url
 *     p: link is a file path
 *     c: link is a color
 */
static void
_handle_link(Termpty *ty, const Eina_Unicode *buf)
{
   const Eina_Unicode type = buf[0];
   Termio *sd = termio_get_from_obj(ty->obj);
   char *link, *c;
   int x1 = -1, y1 = -1, x2 = -1, y2 = -1;
   int value;

   /* highlight where the mouse is */
     {
        Termcell *cells = NULL;
        ssize_t w;

        cells = termpty_cellrow_get(ty, sd->mouse.cy, &w);
        termpty_reset_att(&cells[sd->mouse.cx].att);
        cells[sd->mouse.cx].att.bold = 1;
        cells[sd->mouse.cx].att.fg = COL_WHITE;
        cells[sd->mouse.cx].att.bg = COL_RED;
     }

   /* skip type */
   buf++;

   if (type == 'c')
     {
        _handle_color_link(ty, buf);
        return;
     }
   link = termio_link_find(ty->obj, sd->mouse.cx, sd->mouse.cy,
                           &x1, &y1, &x2, &y2);

   ERR("x1:%d y1:%d x2:%d y2:%d link:'%s'", x1, y1, x2, y2, link);
   if (type == 'n')
     {
        assert (link == NULL);
        return;
     }

   /* Get numeric values */
   buf += _tytest_arg_get(buf, &value);
   assert(x1 == value);
   buf += _tytest_arg_get(buf, &value);
   assert(y1 == value);
   buf += _tytest_arg_get(buf, &value);
   assert(x2 == value);
   buf += _tytest_arg_get(buf, &value);
   assert(y2 == value);
   /* skip ; */
   buf++;

   /* Compare strings */
   c = link;
   while (*buf)
     {
        int idx = 0;
        Eina_Unicode u = eina_unicode_utf8_next_get(c, &idx);

        ERR("%c vs %c", *buf, u);
        assert(*buf == u && "unexpected character in selection");
        c += idx;
        buf++;
     }

   switch (type)
     {
      case 'u':
         assert(link_is_url(link));
         break;
      case 'p':
         assert(link_is_file(link));
         break;
      case 'e':
         assert(link_is_email(link));
         break;
      default:
         abort();
     }
   free(link);
}

static void
_handle_selection_active(Termpty *ty,
                         const Eina_Unicode *buf)
{
   if (*buf == '!')
     assert(ty->selection.is_active);
   else
     assert(!ty->selection.is_active);
}

static void
_handle_selection_is(Termpty *ty,
                     const Eina_Unicode *buf)
{
   size_t len = 0;
   Termio *sd;
   const char *sel, *s;

   assert(ty->selection.is_active);

   sd = termio_get_from_obj(ty->obj);
   sel = s = termio_internal_get_selection(sd, &len);

   assert(s != NULL && "no selection");

   while (*buf)
     {
        int idx = 0;
        Eina_Unicode u = eina_unicode_utf8_next_get(s, &idx);

        /* skip spurious carriage returns */
        if (*buf != '\r')
          {
             assert(*buf == u && "unexpected character in selection");
             s += idx;
          }
        buf++;
     }
   eina_stringshare_del(sel);
}

static void
_handle_force_render(Termpty *ty)
{
   int preedit_x = 0, preedit_y = 0;
   Termio *sd = termio_get_from_obj(ty->obj);

   termio_internal_render(sd, 0, 0, &preedit_x, &preedit_y);
}

/*
 * Format is tc;C;V
 * where C is 0 for top-left, 1 for down-right
 * and V is 0 to unset, 1 to set
 */
static void
_handle_corner(Termpty *ty, const Eina_Unicode *buf)
{
   Termio *sd = termio_get_from_obj(ty->obj);
   int value;
   int corner;

   /* C */
   corner = 0;
   buf += _tytest_arg_get(buf, &corner);

   /* V */
   value = 0;
   _tytest_arg_get(buf, &value);

   if (corner == 0)
     {
        sd->top_left = !! value;
     }
   else
     {
        sd->bottom_right = !! value;
     }
}

/* Testing escape codes that start with '\033}t' and end with '\0'
 * Then,
 * - 'c': set/unset top-left/down-right
 * - 'd': mouse down:
 * - 'u': mouse up;
 * - 'm': mouse move;
 * - 'l': assert mouse is over a link
 * - 'r': force rendering and possibly remove selection;
 * - 'n': assert there is no selection
 * - 's': assert selection is what follows till '\0'
 */
static void
tytest_handle_escape_codes(Termpty *ty,
                           const Eina_Unicode *buf)
{
   switch (buf[0])
     {
      case 'c':
        _handle_corner(ty, buf + 1);
         break;
      case 'd':
        _handle_mouse_down(ty, buf + 1);
        break;
      case 'l':
        _handle_link(ty, buf + 1);
        break;
      case 'm':
        _handle_mouse_move(ty, buf + 1);
        break;
      case 'n':
        _handle_selection_active(ty, buf + 1);
        break;
      case 'r':
        _handle_force_render(ty);
        break;
      case 's':
        _handle_selection_is(ty, buf + 1);
        break;
      case 'u':
        _handle_mouse_up(ty, buf + 1);
        break;
      case 'w':
        _handle_mouse_wheel(ty, buf + 1);
        break;
      default:
        abort();
        break;
     }
}
#endif

#if defined(BINARY_TYTEST) || defined(ENABLE_TEST_UI)
#define ARG_USED_FOR_TESTS
#else
#define ARG_USED_FOR_TESTS EINA_UNUSED
#endif

Eina_Bool
termpty_ext_handle(Termpty *ty ARG_USED_FOR_TESTS,
                   const Eina_Unicode *buf ARG_USED_FOR_TESTS,
                   size_t blen EINA_UNUSED)
{
   switch (buf[0]) // major opcode
     {
#if defined(BINARY_TYTEST) || defined(ENABLE_TEST_UI)
      case 't':
        tytest_handle_escape_codes(ty, buf + 1);
        return EINA_TRUE;
        break;
#endif
      default:
        break;
     }
   return EINA_FALSE;
}
#undef ARG_USED_FOR_TESTS
