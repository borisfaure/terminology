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
static uint64_t _allocated = 0;
#endif
static unsigned char cur_gen = 0;
static Alloc *alloc[MEM_BLOCKS] =  { 0 };

#if 0
static int
roundup_block_size(int sz)
{
   return MEM_ALLOC_ALIGN * ((sz + MEM_ALLOC_ALIGN - 1) / MEM_ALLOC_ALIGN);
}

static Alloc *
_alloc_find(void *mem)
{
   unsigned char *memptr = mem;
   int i;

   for (i = 0; i < MEM_BLOCKS; i++)
     {
        unsigned char *al;

        al = (unsigned char *)alloc[i];
        if (!al) continue;
        if (memptr < al) continue;
        if ((al + TS_MMAP_SIZE) <= memptr) continue;
        return alloc[i];
     }
   return NULL;
}

static void *
_alloc_new(int size, unsigned char gen)
{
   Alloc *al;
   unsigned char *ptr;
   unsigned int newsize, sz;
   int i, firstnull = -1;

   // allocations sized up to nearest size alloc alignment
   newsize = roundup_block_size(size);
   for (i = 0; i < MEM_BLOCKS; i++)
     {
        if (!alloc[i])
          {
             if (firstnull < 0) firstnull = i;
             continue;
          }
        // if generation count matches
        if (alloc[i]->gen == gen)
          {
             // if there is space in the block
             if ((alloc[i]->size - alloc[i]->last) >= newsize)
               {
                  ptr = (unsigned char *)alloc[i];
                  ptr += alloc[i]->last;
                  alloc[i]->last += newsize;
                  alloc[i]->count++;
                  alloc[i]->allocated += newsize;
                  _allocated += newsize;
                  return ptr;
               }
          }
     }
   // out of slots for new blocks - no null blocks
   if (firstnull < 0) {
        ERR("Cannot find new null blocks");
        return NULL;
   }

   // so allocate a new block
   sz = TS_MMAP_SIZE;
   // get mmaped anonymous memory so when freed it goes away from the system
   ptr = (unsigned char*) mmap(NULL, sz, PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
   if (ptr == MAP_FAILED)
     {
        ERR("Cannot allocate more memory with mmap MAP_ANONYMOUS");
        return NULL;
     }

   // note - we SHOULD memset to 0, but we are assuming mmap anon give 0 pages
   //memset(ptr, 0, newsize);

   al = (Alloc *)ptr;
   al->size = sz;
   al->last = sizeof(Alloc) + newsize;
   al->count = 1;
   al->allocated = newsize;
   al->slot = firstnull;
   al->gen = gen;
   _allocated += newsize;
   alloc[al->slot] = al;
   ptr = (unsigned char *)al;
   ptr += sizeof(Alloc);
   return ptr;
}
#endif

static void *
_ts_new(int size)
{
   /* TODO: RESIZE rewrite that stuff */
   //void *ptr;

   if (!size) return NULL;
   //ptr = _alloc_new(size, cur_gen);

   return calloc(1, size);
}

static void
_ts_free(void *ptr)
{
   free(ptr);
#if 0
   Alloc *al;
   unsigned int sz;
   Termsavecomp *ts = ptr;

   if (!ptr) return;

   if (ts->comp)
     sz = sizeof(Termsavecomp) + ts->w;
   else
     sz = sizeof(Termsave) + ((ts->w - 1) * sizeof(Termcell));
   sz = roundup_block_size(sz);
   _allocated -= sz;

   al = _alloc_find(ptr);
   if (!al)
     {
        ERR("Cannot find %p in alloc blocks", ptr);
        return;
     }
   al->count--;
   al->allocated -= sz;
   if (al->count > 0) return;
   alloc[al->slot] = NULL;
#if defined (__sun) || defined (__sun__)
   munmap((caddr_t)al, al->size);
#else
   munmap(al, al->size);
#endif
#endif
}

static void
_mem_defrag(void)
{
   int i, j = 0;
   Alloc *alloc2[MEM_BLOCKS];

   for (i = 0; i < MEM_BLOCKS; i++)
     {
        if (alloc[i])
          {
//             printf("block %i @ %i [%i/%i] # %i\n",
//                    j, alloc[i]->gen, alloc[i]->last, alloc[i]->size, alloc[i]->count);
             alloc2[j] = alloc[i];
             alloc2[j]->slot = j;
             j++;
          }
     }
   // XXX: quicksort blocks with most space at start
   for (i = 0; i < j; i++) alloc[i] = alloc2[i];
   for (; i < MEM_BLOCKS; i++) alloc[i] = NULL;
}

static void
_mem_gen_next(void)
{
   cur_gen++;
}

static unsigned char
_mem_gen_get(void)
{
   return cur_gen;
}

static int ts_comp = 0;
static int ts_uncomp = 0;
static int ts_freeops = 0;
static int ts_compfreeze = 0;
static int freeze = 0;
static Eina_List *ptys = NULL;
static Ecore_Idler *idler = NULL;
static Ecore_Timer *timer = NULL;

#if 0
static Termsave *
_save_comp(Termsave *ts)
{
   Termsave *ts2;
   Termsavecomp *tsc;

   ERR("save comp");

   // already compacted
   if (ts->comp) return ts;
   // make new allocation for new generation
   ts_compfreeze++;
   if (!ts->z)
     {
        int bytes;
        char *buf;

        buf = alloca(LZ4_compressBound(ts->w * sizeof(Termcell)));
        bytes = LZ4_compress((char *)(&(ts->cells[0])), buf,
                             ts->w * sizeof(Termcell));
        tsc = _ts_new(sizeof(Termsavecomp) + bytes);
        if (!tsc)
          {
             ERR("Big problem. Can't allocate backscroll compress buffer");
             ts2 = ts;
             goto done;
          }
        tsc->comp = 1;
        tsc->z = 1;
        tsc->gen = _mem_gen_get();
        tsc->w = bytes;
        tsc->wout = ts->w;
        memcpy(((char *)tsc) + sizeof(Termsavecomp), buf, bytes);
        ts2 = (Termsave *)tsc;
     }
   else
     {
        tsc = (Termsavecomp *)ts;
        ts2 = _ts_new(sizeof(Termsavecomp) + tsc->w);
        if (!ts2)
          {
             ERR("Big problem. Can't allocate backscroll compress/copy buffer");
             ts2 = ts;
             goto done;
          }
        memcpy(ts2, ts, sizeof(Termsavecomp) + tsc->w);
        ts2->gen = _mem_gen_get();
        ts2->comp = 1;
     }
   termpty_save_free(ts);
done:
   ts_compfreeze--;
   return ts2;
}
#endif

static void
_walk_pty(Termpty *ty)
{
   int i;
//   int c0 = 0, c1 = 0;

   if (!ty->back) return;
   for (i = 0; i < ty->backsize; i++)
     {
        Termsavecomp *tsc = (Termsavecomp *)&ty->back[i];

        if (tsc)
          {
#if 0
             ty->back[i] = _save_comp(tsc);
             tsc = (Termsavecomp *)ty->back[i];
             if (tsc->comp) ts_comp++;
             else ts_uncomp++;
//             c0 += tsc->w;
//             c1 += tsc->wout * sizeof(Termcell);
#endif
          }
     }
//   printf("compress ratio: %1.3f\n", (double)c0 / (double)c1);
}

static Eina_Bool
_idler(void *data EINA_UNUSED)
{
   /* TODO: RESIZE : re-enable compression */
   return EINA_FALSE;

   Eina_List *l;
   Termpty *ty;
//   double t0, t;


   _mem_gen_next();

//   t0 = ecore_time_get();
   // start afresh and count comp/uncomp;
   ts_comp = 0;
   ts_uncomp = 0;
   EINA_LIST_FOREACH(ptys, l, ty)
     {
        _walk_pty(ty);
     }
//   t = ecore_time_get();
//   printf("comp/uncomp %i/%i time spent %1.5f\n", ts_comp, ts_uncomp, t - t0);
   _mem_defrag();
   ts_freeops = 0;

   _mem_gen_next();

   idler = NULL;
   return EINA_FALSE;
}

static Eina_Bool
_timer(void *data EINA_UNUSED)
{
   if (!idler) idler = ecore_idler_add(_idler, NULL);
   timer = NULL;
   return EINA_FALSE;
}

static inline void
_check_compressor(Eina_Bool frozen)
{
   /* TODO: RESIZE re-enable compressor */
   return;
   if (freeze) return;
   if (idler) return;
   if ((ts_uncomp > 256) || (ts_freeops > 256))
     {
        if (timer && !frozen) ecore_timer_reset(timer);
        else if (!timer) timer = ecore_timer_add(0.2, _timer, NULL);
     }
}

void
termpty_save_freeze(void)
{
   // XXX: suspend compressor - this probably should be in a thread but right
   // now it'll be fine here
   if (!freeze++)
     {
        if (timer) ecore_timer_freeze(timer);
     }
   if (idler)
     {
        ecore_idler_del(idler);
        idler = NULL;
     }
}

void
termpty_save_thaw(void)
{
   // XXX: resume compressor
   freeze--;
   if (freeze <= 0)
     {
        if (timer) ecore_timer_thaw(timer);
        _check_compressor(EINA_TRUE);
     }
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
   if (!ts) return NULL;
   if (ts->z)
     {
        Termsavecomp *tsc = (Termsavecomp *)ts;
        Termsave *ts2;
        char *buf;
        int bytes;

        ts2 = _ts_new(sizeof(Termsave) + ((tsc->wout - 1) * sizeof(Termcell)));
        if (!ts2) return NULL;
        ts2->gen = _mem_gen_get();
        ts2->w = tsc->wout;
        buf = ((char *)tsc) + sizeof(Termsavecomp);
        bytes = LZ4_uncompress(buf, (char *)(&(ts2->cells[0])),
                               tsc->wout * sizeof(Termcell));
        if (bytes < 0)
          {
             memset(&(ts2->cells[0]), 0, tsc->wout * sizeof(Termcell));
//             ERR("Decompress problem in row at byte %i", -bytes);
          }
        if (ts->comp) ts_comp--;
        else ts_uncomp--;
        ts_uncomp++;
        ts_freeops++;
        ts_compfreeze++;
        _ts_free(ts);
        ts_compfreeze--;
        _check_compressor(EINA_FALSE);
        return ts2;
     }
   _check_compressor(EINA_FALSE);
   return ts;
}

Termsave *
termpty_save_new(Termsave *ts, int w)
{
   termpty_save_free(ts);

   Termcell *cells = calloc(1, w * sizeof(Termcell));
   if (!cells ) return NULL;
   ts->cells = cells;
   ts->gen = _mem_gen_get();
   ts->w = w;
   if (!ts_compfreeze) ts_uncomp++;
   _check_compressor(EINA_FALSE);
   return ts;
}

Termsave *
termpty_save_expand(Termsave *ts, Termcell *cells, ssize_t delta)
{
   Termcell *newcells;

   newcells = realloc(ts->cells, (ts->w + delta) * sizeof(Termcell));
   if (!newcells)
     return NULL;
   newcells[ts->w - 1].att.autowrapped = 0;
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
   _check_compressor(EINA_FALSE);
}
