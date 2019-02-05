#include "private.h"
#include <Elementary.h>
#include "termpty.h"
#include "termptyops.h"
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
             size_t blen)
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
#if defined(ENABLE_TESTS)
      case 't':
        tytest_handle_escape_codes(ty, buf + 1, blen - 1);
        return EINA_FALSE;
        break;
#endif
      default:
        break;
     }
   return EINA_FALSE;
}
