/* Throughput benchmark for the pty intake path.
 *
 * Feeds a corpus through exactly the code the shipping binary runs when bytes
 * arrive from the pty -- UTF-8 decode plus termpty_handle_buf() -- and reports
 * how fast it goes. The read() syscall itself is deliberately left out: it is
 * measured by changing the chunk size rather than by timing the kernel.
 *
 * Built without EINA_LOG_LEVEL_MAXIMUM (see private.h) so that logging calls
 * cost here what they cost in production.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/stat.h>

#include "private.h"
#include <Elementary.h>
#include "config.h"
#include "termpty.h"
#include "tytest_common.h"

int _log_domain = -1;

#define DEFAULT_ITERATIONS 10
#define DEFAULT_WARMUP      2
#define DEFAULT_CHUNK    4096

typedef struct tag_Corpus
{
   const char *name;
   char       *data;
   long        len;
} Corpus;

static double
_now(void)
{
   struct timespec ts;

   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static const char *
_basename(const char *path)
{
   const char *slash = strrchr(path, '/');

   return slash ? slash + 1 : path;
}

static Eina_Bool
_corpus_load(Corpus *c, const char *path)
{
   struct stat st;
   long got = 0;
   int fd;

   fd = open(path, O_RDONLY);
   if (fd < 0)
     {
        fprintf(stderr, "tybench: cannot open %s\n", path);
        return EINA_FALSE;
     }
   if (fstat(fd, &st) < 0 || st.st_size <= 0)
     {
        fprintf(stderr, "tybench: cannot size %s\n", path);
        close(fd);
        return EINA_FALSE;
     }

   c->len = (long)st.st_size;
   c->data = malloc(c->len);
   if (!c->data)
     {
        fprintf(stderr, "tybench: out of memory for %s\n", path);
        close(fd);
        return EINA_FALSE;
     }

   while (got < c->len)
     {
        ssize_t n = read(fd, c->data + got, c->len - got);

        if (n <= 0) break;
        got += n;
     }
   close(fd);

   if (got != c->len)
     {
        fprintf(stderr, "tybench: short read on %s\n", path);
        free(c->data);
        c->data = NULL;
        return EINA_FALSE;
     }

   c->name = _basename(path);
   return EINA_TRUE;
}

/* One pass over the corpus, split into chunk-sized pieces so the benchmark can
 * show what read() sizing is worth. */
static void
_feed_pass(const Corpus *c, int chunk)
{
   long off;

   for (off = 0; off < c->len; off += chunk)
     {
        int n = (c->len - off < chunk) ? (int)(c->len - off) : chunk;

        tytest_common_feed(c->data + off, n);
     }
}

/* Put the terminal back to a known state between corpora, so one corpus cannot
 * leave modes set that change how the next one parses. */
static void
_reset_terminal(void)
{
   static const char ris[] = "\033c";

   tytest_common_feed(ris, sizeof(ris) - 1);
}

static void
_usage(const char *argv0)
{
   fprintf(stderr,
           "usage: %s [options] <corpus>...\n"
           "\n"
           "  -i, --iterations=N  timed passes over each corpus (default %d)\n"
           "  -w, --warmup=N      untimed passes before timing (default %d)\n"
           "  -c, --chunk=N       bytes per simulated read() (default %d)\n"
           "  -t, --tsv           tab-separated output for scripting\n"
           "  -h, --help          this message\n",
           argv0, DEFAULT_ITERATIONS, DEFAULT_WARMUP, DEFAULT_CHUNK);
}

static int
_int_opt(const char *arg, const char *longform, int *out)
{
   size_t n = strlen(longform);

   if (strncmp(arg, longform, n) != 0) return 0;
   if (arg[n] != '=') return 0;
   *out = atoi(arg + n + 1);
   return 1;
}

int
main(int argc, char **argv)
{
   int iterations = DEFAULT_ITERATIONS;
   int warmup = DEFAULT_WARMUP;
   int chunk = DEFAULT_CHUNK;
   Eina_Bool tsv = EINA_FALSE;
   int i, ncorpus = 0;
   Corpus *corpus;

   corpus = calloc(argc, sizeof(Corpus));
   if (!corpus) return 1;

   for (i = 1; i < argc; i++)
     {
        const char *a = argv[i];

        if (!strcmp(a, "-h") || !strcmp(a, "--help"))
          {
             _usage(argv[0]);
             free(corpus);
             return 0;
          }
        else if (!strcmp(a, "-t") || !strcmp(a, "--tsv"))
          tsv = EINA_TRUE;
        else if (!strcmp(a, "-i") && i + 1 < argc)
          iterations = atoi(argv[++i]);
        else if (!strcmp(a, "-w") && i + 1 < argc)
          warmup = atoi(argv[++i]);
        else if (!strcmp(a, "-c") && i + 1 < argc)
          chunk = atoi(argv[++i]);
        else if (_int_opt(a, "--iterations", &iterations))
          continue;
        else if (_int_opt(a, "--warmup", &warmup))
          continue;
        else if (_int_opt(a, "--chunk", &chunk))
          continue;
        else if (a[0] == '-')
          {
             fprintf(stderr, "tybench: unknown option %s\n", a);
             _usage(argv[0]);
             free(corpus);
             return 1;
          }
        else
          {
             if (!_corpus_load(&corpus[ncorpus], a))
               {
                  free(corpus);
                  return 1;
               }
             ncorpus++;
          }
     }

   if (ncorpus == 0)
     {
        _usage(argv[0]);
        free(corpus);
        return 1;
     }
   if (iterations < 1) iterations = 1;
   if (warmup < 0) warmup = 0;
   if (chunk < 1) chunk = 1;

   eina_init();
   _log_domain = eina_log_domain_register("tybench", NULL);
   /* The parser logs to its own domain. Registering it matters here in a way it
    * does not for tytest/tyfuzz: those compile logging out entirely, whereas
    * this binary keeps it, and an unregistered domain sends every DBG down
    * eina's "unknown domain" complaint path instead of the cheap level check
    * that production takes. */
   termpty_init();
   tytest_common_init();

   if (!tsv)
     {
        printf("chunk=%d iterations=%d warmup=%d\n\n", chunk, iterations, warmup);
        printf("%-16s %10s %8s %10s %10s\n",
               "corpus", "bytes", "MB/s", "ns/byte", "best MB/s");
        printf("%-16s %10s %8s %10s %10s\n",
               "----------------", "----------", "--------",
               "----------", "----------");
     }

   for (i = 0; i < ncorpus; i++)
     {
        double total, best_pass = 0.0, mbs, best_mbs;
        int pass;

        _reset_terminal();
        for (pass = 0; pass < warmup; pass++)
          _feed_pass(&corpus[i], chunk);

        total = 0.0;
        for (pass = 0; pass < iterations; pass++)
          {
             double t0, dt;

             t0 = _now();
             _feed_pass(&corpus[i], chunk);
             dt = _now() - t0;

             total += dt;
             if (pass == 0 || dt < best_pass) best_pass = dt;
          }

        mbs = ((double)corpus[i].len * iterations) / total / 1e6;
        best_mbs = (double)corpus[i].len / best_pass / 1e6;

        if (tsv)
          printf("%s\t%ld\t%d\t%d\t%.6f\t%.3f\t%.3f\n",
                 corpus[i].name, corpus[i].len, chunk, iterations,
                 total, mbs, best_mbs);
        else
          printf("%-16s %10ld %8.2f %10.3f %10.2f\n",
                 corpus[i].name, corpus[i].len, mbs,
                 total * 1e9 / ((double)corpus[i].len * iterations), best_mbs);

        fflush(stdout);
     }

   for (i = 0; i < ncorpus; i++)
     free(corpus[i].data);
   free(corpus);

   tytest_common_shutdown();
   eina_shutdown();

   return 0;
}
