#include "private.h"
#include <Elementary.h>
#include "termpty.h"
#include "termptysave.h"
#include "lz4/lz4.h"
#include <sys/mman.h>

#if defined (__MacOSX__) || (defined (__MACH__) && defined (__APPLE__))
# ifndef MAP_ANONYMOUS
#  define MAP_ANONYMOUS MAP_ANON
# endif
#endif

#define MEM_ALLOC_ALIGN  16
#define MEM_BLOCKS       1024

#define TS_MMAP_SIZE 131072
#define TS_ALLOC_MASK (TS_MMAP_SIZE - 1)

typedef struct _Alloc Alloc;

struct _Alloc
{
   unsigned int size, last, count, allocated;
   short slot;
   unsigned char gen;
   unsigned char __pad;
};


#if 0
static void *
_ts_new(int size)
{
   /* TODO: RESIZE rewrite that stuff */
   //void *ptr;

   if (!size) return NULL;
   //ptr = _alloc_new(size, cur_gen);

   return calloc(1, size);
}
#endif

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
#if 0
   if (ts->z) //TODO: unused
     {
        Termsavecomp *tsc = (Termsavecomp *)ts;
        Termsave *ts2;
        char *buf;
        int bytes;

        ts2 = _ts_new(sizeof(Termsave) + ((tsc->wout - 1) * sizeof(Termcell)));
        if (!ts2) return NULL;
        ts2->w = tsc->wout;
        buf = ((char *)tsc) + sizeof(Termsavecomp);
        bytes = LZ4_uncompress(buf, (char *)(&(ts2->cells[0])),
                               tsc->wout * sizeof(Termcell));
        if (bytes < 0)
          {
             memset(&(ts2->cells[0]), 0, tsc->wout * sizeof(Termcell));
//             ERR("Decompress problem in row at byte %i", -bytes);
          }
        return ts2;
     }
#endif
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
