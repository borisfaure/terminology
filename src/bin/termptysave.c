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

#define MEM_PAGE_SIZE    4096
#define MEM_ALLOC_ALIGN  16
#define MEM_BLOCK_PAGES  32
#define MEM_BLOCKS       1024

typedef struct _Alloc Alloc;

struct _Alloc
{
   int size, last, count;
   short slot;
   unsigned char gen;
   unsigned char __pad;
};

static unsigned char cur_gen = 0;
static Alloc *alloc[MEM_BLOCKS] =  { 0 };

static void *
_alloc_new(int size, unsigned char gen)
{
   Alloc *al;
   unsigned char *ptr;
   int newsize, sz, i, firstnull = -1;

   // allocations sized up to nearest size alloc alignment
   newsize = MEM_ALLOC_ALIGN * ((size + MEM_ALLOC_ALIGN - 1) / MEM_ALLOC_ALIGN);
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
   size = MEM_BLOCK_PAGES * MEM_PAGE_SIZE;
   // size up to page size
   sz = MEM_PAGE_SIZE * ((size + MEM_PAGE_SIZE - 1) / MEM_PAGE_SIZE);
   // get mmaped anonymous memory so when freed it goes away from the system
   ptr = mmap(NULL, sz, PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
   if (ptr == MAP_FAILED) {
	   ERR("Cannot allocate more memory with mmap MAP_ANONYMOUS");
	   return NULL;
   }

   // note - we SHOULD memset to 0, but we are assuming mmap anon give 0 pages
   //memset(ptr, 0, newsize);

   al = (Alloc *)ptr;
   al->size = sz;
   al->last = sizeof(Alloc) + newsize;
   al->count = 1;
   al->slot = firstnull;
   al->gen = gen;
   alloc[al->slot] = al;
   ptr = (unsigned char *)al;
   ptr += sizeof(Alloc);
   return ptr;
}

static void
_alloc_free(Alloc *al)
{
   al->count--;
   if (al->count > 0) return;
   alloc[al->slot] = NULL;
   munmap(al, al->size);
}

static Alloc *
_alloc_find(void *mem)
{
   unsigned char *memptr = mem;
   int i;
   
   for (i = 0; i < MEM_BLOCKS; i++)
     {
        unsigned char *ptr;
        
        ptr = (unsigned char *)alloc[i];
        if (!ptr) continue;
        if (memptr < ptr) continue;
        if ((memptr - ptr) > 0x0fffffff) continue;
        if (((size_t)memptr - (size_t)ptr) < (size_t)(alloc[i]->size))
          return alloc[i];
     }
   return NULL;
}

static void *
_mem_new(int size)
{
   void *ptr;
   
   if (!size) return NULL;
   ptr = _alloc_new(size, cur_gen);
   return ptr;
}

static void
_mem_free(void *ptr)
{
   Alloc *al;
   
   if (!ptr) return;
   al = _alloc_find(ptr);
   if (!al)
     {
        ERR("Cannot find %p in alloc blocks", ptr);
        return;
     }
   _alloc_free(al);
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

static Termsave *
_save_comp(Termsave *ts)
{
   Termsave *ts2;
   Termsavecomp *tsc;

   // already compacted
   if (ts->comp) return ts;
   // make new allocation for new generation
   ts_compfreeze++;
   if (!ts->z)
     {
        int bytes;
        char *buf;
        
        buf = alloca(LZ4_compressBound(ts->w * sizeof(Termcell)));
        bytes = LZ4_compress((char *)(&(ts->cell[0])), buf,
                             ts->w * sizeof(Termcell));
        tsc = _mem_new(sizeof(Termsavecomp) + bytes);
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
        ts2 = _mem_new(sizeof(Termsavecomp) + tsc->w);
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

static void
_walk_pty(Termpty *ty)
{
   int i;
//   int c0 = 0, c1 = 0;

   if (!ty->back) return;
   for (i = 0; i < ty->backmax; i++)
     {
        Termsavecomp *tsc = (Termsavecomp *)ty->back[i];

        if (tsc)
          {
             ty->back[i] = _save_comp(ty->back[i]);
             tsc = (Termsavecomp *)ty->back[i];
             if (tsc->comp) ts_comp++;
             else ts_uncomp++;
//             c0 += tsc->w;
//             c1 += tsc->wout * sizeof(Termcell);
          }
     }
//   printf("compress ratio: %1.3f\n", (double)c0 / (double)c1);
}

static Eina_Bool
_idler(void *data EINA_UNUSED)
{
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
        
        ts2 = _mem_new(sizeof(Termsave) + ((tsc->wout - 1) * sizeof(Termcell)));
        if (!ts2) return NULL;
        ts2->gen = _mem_gen_get();
        ts2->w = tsc->wout;
        buf = ((char *)tsc) + sizeof(Termsavecomp);
        bytes = LZ4_uncompress(buf, (char *)(&(ts2->cell[0])),
                               tsc->wout * sizeof(Termcell));
        if (bytes < 0)
          {
             memset(&(ts2->cell[0]), 0, tsc->wout * sizeof(Termcell));
//             ERR("Decompress problem in row at byte %i", -bytes);
          }
        if (ts->comp) ts_comp--;
        else ts_uncomp--;
        ts_uncomp++;
        ts_freeops++;
        ts_compfreeze++;
        _mem_free(ts);
        ts_compfreeze--;
        _check_compressor(EINA_FALSE);
        return ts2;
     }
   _check_compressor(EINA_FALSE);
   return ts;
}

Termsave *
termpty_save_new(int w)
{
   Termsave *ts = _mem_new(sizeof(Termsave) + ((w - 1) * sizeof(Termcell)));
   if (!ts) return NULL;
   ts->gen = _mem_gen_get();
   ts->w = w;
   if (!ts_compfreeze) ts_uncomp++;
   _check_compressor(EINA_FALSE);
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
   _mem_free(ts);
   _check_compressor(EINA_FALSE);
}
