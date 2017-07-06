#include "private.h"
#include <Elementary.h>
#include "termpty.h"
#include "termptyops.h"

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
             const char *txt,
             const Eina_Unicode *_utxt EINA_UNUSED)
{
   switch (txt[1])
     {
      case 'a': // command aa*
        break;
        // room here for more minor opcode chars like 'b', 'c' etc.
      default:
        break;
     }
   return EINA_FALSE;
}

Eina_Bool
_termpty_ext_handle(Termpty *ty, const char *txt, const Eina_Unicode *utxt)
{
   switch (txt[0]) // major opcode
     {
      case 'a': // command a*
        return _handle_op_a(ty, txt, utxt);
        break;
        // room here for more major opcode chars like 'b', 'c' etc.
      default:
        break;
     }
   return EINA_FALSE;
}
