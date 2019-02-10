#include "private.h"
#include <Elementary.h>
#include "termio.h"
#include "termpty.h"
#include "termptyops.h"
#include "termiointernals.h"
#include "tytest.h"
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

#if defined(ENABLE_TESTS)
 #define WITH_TESTS 1
#else
 #define WITH_TESTS 0
#endif
// Uncomment following to enable testing escape codes within terminology
//#define WITH_TESTS  1

//// extended terminology escape handling goes in here
//
// this is where escapes get handled *IF* the termpty layer needs to interpret
// them itself for some reason. if it returns EINA_FALSE, it means the escape
// is to be passed onto termio layer as a callback and handled there after
// this code. an extended escape may be handled in here exclusively (return
// EINA_TRUE), handled here first, then in termio (EINA_FALSE return) or not
// handled here at all and just passed to termio to figure it out (return
// EINA_FALSE).
//
// command strings like like this:
//   axBLAHBLAH
// where 'a' is the major opcode char.
// and 'x' is the minor opcode char.
// and 'BLAHBLAH' is an optional data payload string

static Eina_Bool
_handle_op_a(Termpty *_ty EINA_UNUSED,
             const Eina_Unicode *buf EINA_UNUSED,
             size_t blen EINA_UNUSED)
{
   switch (buf[0])
     {
      case 'x': // command ax*
        break;
        // room here for more minor opcode chars like 'b', 'c' etc.
      default:
        break;
     }
   return EINA_FALSE;
}

#if WITH_TESTS

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

/**
 * FLAGS can be:
 * - 0
 * - 1: SINGLE_CLICK
 * - 2: DOUBLE_CLICK
 * - 3: TRIPLE_CLICK
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
   value = 0;
   buf += _tytest_arg_get(buf, &value);
   union {
        Termio_Modifiers m;
        uint8_t u;
   } u;
   u.u = value;
   modifiers = u.m;
   /* FLAGS */
   value = 0;
   buf +=_tytest_arg_get(buf, &value);
   ev.flags = value;

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
   value = 0;
   buf += _tytest_arg_get(buf, &value);
   union {
        Termio_Modifiers m;
        uint8_t u;
   } u;
   u.u = value;
   modifiers = u.m;
   /* FLAGS */
   value = 0;
   buf +=_tytest_arg_get(buf, &value);
   ev.flags = value;

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
   value = 0;
   buf += _tytest_arg_get(buf, &value);
   union {
        Termio_Modifiers m;
        uint8_t u;
   } u;
   u.u = value;
   modifiers = u.m;

   termio_internal_mouse_move(sd, &ev, modifiers);
}

/* Testing escape codes that start with '\033}t' and end with '\0'
 * Then,
 * - 'd': mouse down:
 * - 'u': mouse up;
 * - 'm': mouse move;
 */
static void
tytest_handle_escape_codes(Termpty *ty,
                           const Eina_Unicode *buf)
{
   switch (buf[0])
     {
      case 'd':
        return _handle_mouse_down(ty, buf + 1);
        break;
      case 'm':
        return _handle_mouse_move(ty, buf + 1);
        break;
      case 'u':
        return _handle_mouse_up(ty, buf + 1);
        break;
      default:
        break;
     }
}
#endif


Eina_Bool
termpty_ext_handle(Termpty *ty,
                   const Eina_Unicode *buf,
                   size_t blen)
{
   switch (buf[0]) // major opcode
     {
      case 'a': // command a*
        return _handle_op_a(ty, buf + 1, blen - 1);
        break;
        // room here for more major opcode chars like 'b', 'c' etc.
#if WITH_TESTS
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
