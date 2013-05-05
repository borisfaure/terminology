#include "private.h"
#include <Elementary.h>
#include "termpty.h"
#include "termptysave.h"

static int ts_uncomp = 0;
static int freeze = 0;
static Eina_List *ptys = NULL;

static void
_check_compressor(void)
{
   if (ts_uncomp > 1024)
     {
        // XXX: if no compressor start one if not frozen
     }
}

void
termpty_save_freeze(void)
{
   // XXX: suspend compressor
   freeze++;
}

void
termpty_save_thaw(void)
{
   // XXX: resume compressor
   freeze--;
   if (freeze <= 0) _check_compressor();
}

void
termpty_save_register(Termpty *ty)
{
   termpty_save_freeze();
   ptys = eina_list_append(ptys, ty);
   termpty_save_thaw();
}

void
termpty_save_unregister(Termpty *ty)
{
   termpty_save_freeze();
   ptys = eina_list_remove(ptys, ty);
   termpty_save_thaw();
}

Termsave *
termpty_save_extract(Termsave *ts)
{
   // XXX: decompress a Termsave struct from our save store using input ptr as
   // handle to find it
   if (!ts) return NULL;
   // XXX: if was compressed ts_comp--; ts_uncomp++;
   _check_compressor();
   return ts;
}

Termsave *
termpty_save_new(int w)
{
   Termsave *ts = calloc(1, sizeof(Termsave) + (w - 1) * sizeof(Termcell));
   if (!ts) return NULL;
   ts->w = w;
   ts_uncomp++;
   _check_compressor();
   return ts;
}

void
termpty_save_free(Termsave *ts)
{
   if (!ts) return;
   // XXX: if compressed mark region as free, if not then free ts
   // XXX: if compresses ts_comp--; else ts_uncomp--;
   ts_uncomp--;
   _check_compressor();
   free(ts);
}
