#include "private.h"
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include "private.h"
#include <Elementary.h>
#include "config.h"
#include "termpty.h"
#include "termptyops.h"
#include "backlog.h"
#include "termiointernals.h"
#include "tytest.h"
#include "unit_tests.h"
#include "tytest_common.h"

#include "md5.h"

int _log_domain = -1;

/* {{{ Unit tests */

static struct {
     const char *name;
     tytest_func func;
} _tytests[] = {
       { "dummy", tytest_dummy },
       { "sb_skip", tytest_sb_skip},
       { "sb_trim", tytest_sb_trim},
       { "sb_gap", tytest_sb_gap},
       { "sb_steal", tytest_sb_steal},
       { "color_parse_hex", tytest_color_parse_hex},
       { "color_parse_2hex", tytest_color_parse_2hex},
       { "color_parse_sharp", tytest_color_parse_sharp},
       { "color_parse_uint8", tytest_color_parse_uint8},
       { "color_parse_edc", tytest_color_parse_edc},
       { "color_parse_css_rgb", tytest_color_parse_css_rgb},
       { "color_parse_css_hsl", tytest_color_parse_css_hsl},
       { "extn_matching", tytest_extn_matching},
       { "base64", tytest_base64},
       { "shell_quote", tytest_shell_quote},
       { "sync_frame_coherence", tytest_sync_frame_coherence},
       { "sync_watchdog_teardown", tytest_sync_watchdog_teardown},
       { "sync_nested", tytest_sync_nested},
       { "sync_resize", tytest_sync_resize},
       { "sync_soft_reset", tytest_sync_soft_reset},
       { NULL, NULL},
};

static int
_run_this_tytest(const char *name, tytest_func func)
{
   int res;
   fprintf(stderr, "\033[0m%s...", name);
   res = func();
   fprintf(stderr, " %s\033[0m\n", res == 0 ? "\033[32m✔" : "\033[31;1m×");
   return res;
}

static tytest_func
_find_tytest(const char *name)
{
   int ntests = (sizeof(_tytests) / sizeof(_tytests[0])) - 1;
   int i;

   for (i = 0; i < ntests; i++)
     {
        if (strcmp(name, _tytests[i].name) == 0)
          return _tytests[i].func;
     }
   return NULL;
}

static int
_run_all_tytests(void)
{
   int ntests = (sizeof(_tytests) / sizeof(_tytests[0])) - 1;
   int i, res = 0;

   for (i = 0; res == 0 && i < ntests; i++)
     res = _run_this_tytest(_tytests[i].name, _tytests[i].func);
   return res;
}

static int
_run_tytests(int argc, char **argv)
{
   int i, res = 0;

   for (i = 1; res == 0 && i < argc; i++)
     {
        if (strncmp(argv[i], "all", strlen("all")) == 0)
          res = _run_all_tytests();
        else
          {
             tytest_func func = _find_tytest(argv[i]);
             if (!func)
               {
                  fprintf(stderr, "can not find test named '%s'\n", argv[i]);
                  return -1;
               }
             res = _run_this_tytest(argv[i], func);
          }
     }
   return res;
}

/* }}} */

typedef struct tag_Termpty_Tests
{
   uint64_t backsize, backpos;
   Backlog_Beacon backlog_beacon;
   Term_State termstate;
   Term_Cursor cursor_state;
   Term_Cursor cursor_save[2];
   int w, h;
   uint64_t altbuf     : 1;
   uint64_t mouse_mode : 3;
   uint64_t mouse_ext  : 2;
   uint64_t bracketed_paste : 1;
} __attribute((__packed__)) Termpty_Tests;

static void
_termpty_to_termpty_tests(Termpty *ty, Termpty_Tests *tt)
{
   memset(tt, '\0', sizeof(*tt));
   tt->backsize = ty->backsize;
   tt->backpos = ty->backpos;
   tt->backlog_beacon = ty->backlog_beacon;
   tt->termstate = ty->termstate;
   tt->cursor_state = ty->cursor_state;
   tt->cursor_save[0] = ty->cursor_save[0];
   tt->cursor_save[1] = ty->cursor_save[1];
   tt->w = ty->w;
   tt->h = ty->h;
   tt->altbuf = ty->altbuf;
   tt->mouse_mode = ty->mouse_mode;
   tt->mouse_ext = ty->mouse_ext;
   tt->bracketed_paste = ty->bracketed_paste;
}

/* Fold the scrollback into the checksum.
 *
 * Only backsize/backpos/beacon used to be covered, so everything that had
 * scrolled off the screen was invisible to the tests -- which is precisely
 * where a bug in the scroll or text-append path shows up first. Rows are read
 * back through termpty_cellrow_get() rather than reached into directly, so the
 * test keeps comparing what a reader of the backlog would see even if how a
 * row is stored changes. */
static void
_checksum_backlog(Termpty *ty, MD5_CTX *ctx)
{
   ssize_t len = termpty_backlog_length(ty);
   int y;

   for (y = 1; y <= (int)len; y++)
     {
        const Termcell *cells;
        ssize_t w = 0;
        uint32_t width;

        cells = termpty_cellrow_get(ty, -y, &w);
        if (!cells || (w < 0)) w = 0;
        /* Fixed width, not sizeof(ssize_t): the checksum is compared across
         * machines and must not depend on the size of a pointer. */
        width = (uint32_t)w;
        MD5Update(ctx, (unsigned char const*)&width, sizeof(width));
        if (cells && (w > 0))
          MD5Update(ctx, (unsigned char const*)cells, sizeof(Termcell) * w);
     }
}

static void
_tytest_checksum(Termpty *ty)
{
   MD5_CTX ctx;
   Termpty_Tests tests;
   char md5out[(2 * MD5_HASHBYTES) + 1];
   unsigned char hash[MD5_HASHBYTES];
   static const char hex[] = "0123456789abcdef";
   int n;

   _termpty_to_termpty_tests(ty, &tests);

   MD5Init(&ctx);
   /* Termpty */
   MD5Update(&ctx,
             (unsigned char const*)&tests,
             sizeof(tests));
   /* The screens */
   MD5Update(&ctx,
             (unsigned char const*)ty->screen,
             sizeof(Termcell) * ty->w * ty->h);
   MD5Update(&ctx,
             (unsigned char const*)ty->screen2,
             sizeof(Termcell) * ty->w * ty->h);
   /* The scrollback */
   _checksum_backlog(ty, &ctx);
   /* Icon/Title */
   if (ty->prop.icon)
     {
        MD5Update(&ctx,
                  (unsigned char const*)ty->prop.icon,
                  strlen(ty->prop.icon));
     }
   else
     {
        MD5Update(&ctx, (unsigned char const*)"(NULL)", 6);
     }
   if (ty->prop.title)
     {
        MD5Update(&ctx,
                  (unsigned char const*)ty->prop.title,
                  strlen(ty->prop.title));
     }
   else
     {
        MD5Update(&ctx, (unsigned char const*)"(NULL)", 6);
     }
   /* Cursor shape */
   const char *cursor_shape = tytest_cursor_shape_get();
   MD5Update(&ctx, (unsigned char const*)cursor_shape,
             strlen(cursor_shape));
   /* Write buffer */
   if (ty->write_buffer.len)
     {
        MD5Update(&ctx, (unsigned char const*)ty->write_buffer.buf,
                  ty->write_buffer.len);
     }

   MD5Final(hash, &ctx);

   for (n = 0; n < MD5_HASHBYTES; n++)
     {
        md5out[2 * n] = hex[hash[n] >> 4];
        md5out[2 * n + 1] = hex[hash[n] & 0x0f];
     }
   md5out[2 * MD5_HASHBYTES] = '\0';
   printf("%s", md5out);
}


static void
_usage(const char *argv0)
{
   fprintf(stderr,
           "usage: %s                 read escape codes on stdin, print a state checksum\n"
           "       %s <test>|all      run the built-in unit tests\n",
           argv0, argv0);
}

int
main(int argc, char **argv)
{
   int chunk = 0;
   int i;

   for (i = 1; i < argc; i++)
     {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
          {
             _usage(argv[0]);
             return 0;
          }
        else if (!strncmp(argv[i], "--chunk=", 8)) chunk = atoi(argv[i] + 8);
        /* Anything else is a unit test name, and those take over entirely. */
        else return _run_tytests(argc, argv);
     }

   eina_init();
   emile_init();

   _log_domain = eina_log_domain_register("tytest", NULL);

   tytest_common_init();
   if (chunk > 0) tytest_common_set_chunk(chunk);

   tytest_common_main_loop();

   _tytest_checksum(tytest_termpty_get());

   tytest_common_shutdown();

   emile_shutdown();
   eina_shutdown();

   return 0;
}
