#include "private.h"
#include <Elementary.h>
#include "termpty.h"
#include "backlog.h"


static int ts_comp = 0;
static int ts_uncomp = 0;
static int ts_freeops = 0;
static Eina_List *ptys = NULL;

static int64_t _mem_used = 0;

static void
_accounting_change(int64_t diff)
{
   if (diff > 0)
     {
        diff = ROUND_UP(diff, 16);
     }
   else
     {
        diff = DIV_ROUND_UP(-1 * diff, 16) * -16;
     }
   _mem_used += diff;
}

int64_t
termpty_backlog_memory_get(void)
{
   return _mem_used;
}


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
termpty_save_new(Termpty *ty, Termsave *ts, int w)
{
   termpty_save_free(ty, ts);

   Termcell *cells = calloc(1, w * sizeof(Termcell));
   if (!cells ) return NULL;
   ts->cells = cells;
   ts->w = w;
   _accounting_change(w * sizeof(Termcell));
   return ts;
}

Termsave *
termpty_save_expand(Termpty *ty, Termsave *ts, Termcell *cells, size_t delta)
{
   Termcell *newcells;

   newcells = realloc(ts->cells, (ts->w + delta) * sizeof(Termcell));
   if (!newcells)
     return NULL;

   memset(newcells + ts->w,
          0, delta * sizeof(Termcell));
   TERMPTY_CELL_COPY(ty, cells, &newcells[ts->w], (int)delta);

   _accounting_change((-1) * (int64_t)(ts->w * sizeof(Termcell)));
   ts->w += delta;
   _accounting_change(ts->w * sizeof(Termcell));
   ts->cells = newcells;
   return ts;
}

void
termpty_save_free(Termpty *ty, Termsave *ts)
{
   unsigned int i;
   if (!ts) return;
   if (ts->comp) ts_comp--;
   else ts_uncomp--;
   ts_freeops++;
   for (i = 0; i < ts->w; i++)
     {
        if (EINA_UNLIKELY(ts->cells[i].att.link_id))
          term_link_refcount_dec(ty, ts->cells[i].att.link_id, 1);
     }
   free(ts->cells);
   ts->cells = NULL;
   _accounting_change((-1) * (int64_t)(ts->w * sizeof(Termcell)));
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

void
termpty_backlog_free(Termpty *ty)
{
   size_t i;

   if (!ty || !ty->back)
     return;

   for (i = 0; i < ty->backsize; i++)
     termpty_save_free(ty, &ty->back[i]);
   _accounting_change((-1) * (int64_t)(sizeof(Termsave) * ty->backsize));
   free(ty->back);
   ty->back = NULL;
}

void
termpty_clear_backlog(Termpty *ty)
{
   int backsize;

   ty->backlog_beacon.screen_y = 0;
   ty->backlog_beacon.backlog_y = 0;

   termpty_backlog_lock();
   termpty_backlog_free(ty);
   ty->backpos = 0;
   backsize = ty->backsize;
   ty->backsize = 0;
   termpty_backlog_size_set(ty, backsize);
   termpty_backlog_unlock();
}

ssize_t
termpty_backlog_length(Termpty *ty)
{
   int backlog_y = ty->backlog_beacon.backlog_y;
   int screen_y = ty->backlog_beacon.screen_y;

   if (!ty->backsize)
     return 0;

   for (backlog_y++; backlog_y < (int)ty->backsize; backlog_y++)
     {
        int nb_lines;
        const Termsave *ts;

        ts = BACKLOG_ROW_GET(ty, backlog_y);
        if (!ts->cells)
          goto end;

        nb_lines = (ts->w == 0) ? 1 : DIV_ROUND_UP(ts->w, ty->w);
        screen_y += nb_lines;
        ty->backlog_beacon.screen_y = screen_y;
        ty->backlog_beacon.backlog_y = backlog_y;
     }
end:
     return ty->backlog_beacon.screen_y;
}


void
termpty_backlog_size_set(Termpty *ty, size_t size)
{
   Termsave *new_back;
   size_t i;

   if (ty->backsize == size)
     return;

   termpty_backlog_lock();

   if (size == 0)
     {
        termpty_backlog_free(ty);
        goto end;
     }
   if (size > ty->backsize)
     {
        new_back = realloc(ty->back, sizeof(Termsave) * size);
        if (!new_back)
          return;
        memset(new_back + ty->backsize, 0,
               sizeof(Termsave) * (size - ty->backsize));
        ty->back = new_back;
     }
   else
     {
        new_back = calloc(1, sizeof(Termsave) * size);
        if (!new_back)
          return;
        for (i = 0; i < size; i++)
          new_back[i] = ty->back[i];
        for (i = size; i < ty->backsize; i++)
          termpty_save_free(ty, &ty->back[i]);
        free(ty->back);
        ty->back = new_back;
     }
   _accounting_change((size - ty->backsize) * (int64_t)sizeof(Termsave));
end:
   ty->backpos = 0;
   ty->backsize = size;
   /* Reset beacon */
   ty->backlog_beacon.screen_y = 0;
   ty->backlog_beacon.backlog_y = 0;

   termpty_backlog_unlock();
}
