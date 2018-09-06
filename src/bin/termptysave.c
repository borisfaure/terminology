#include "private.h"
#include <Elementary.h>
#include "termpty.h"
#include "termptysave.h"

static void
_ts_free(void *ptr)
{
   free(ptr);
}

static int ts_comp = 0;
static int ts_uncomp = 0;
static int ts_freeops = 0;
static int ts_compfreeze = 0;
static Eina_List *ptys = NULL;

void
termpty_save_register(Termpty *ty)
{
   termpty_backlog_lock();
   ptys = eina_list_append(ptys, ty);
   termpty_backlog_unlock();
}

void
termpty_save_unregister(Termpty *ty)
{
   termpty_backlog_lock();
   ptys = eina_list_remove(ptys, ty);
   termpty_backlog_unlock();
}

Termsave *
termpty_save_extract(Termsave *ts)
{
   if (!ts) return NULL;
   return ts;
}

Termsave *
termpty_save_new(Termsave *ts, int w)
{
   termpty_save_free(ts);

   Termcell *cells = calloc(1, w * sizeof(Termcell));
   if (!cells ) return NULL;
   ts->cells = cells;
   ts->w = w;
   return ts;
}

Termsave *
termpty_save_expand(Termsave *ts, Termcell *cells, size_t delta)
{
   Termcell *newcells;

   newcells = realloc(ts->cells, (ts->w + delta) * sizeof(Termcell));
   if (!newcells)
     return NULL;

   memcpy(&newcells[ts->w], cells, delta * sizeof(Termcell));
   ts->w += delta;
   ts->cells = newcells;
   return ts;
}

void
termpty_save_free(Termsave *ts)
{
   if (!ts) return;
   if (!ts_compfreeze)
     {
        if (ts->comp) ts_comp--;
        else ts_uncomp--;
        ts_freeops++;
     }
   _ts_free(ts->cells);
   ts->cells = NULL;
   ts->w = 0;
}

void
termpty_backlog_lock(void)
{
}

void
termpty_backlog_unlock(void)
{
}
