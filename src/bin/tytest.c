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
#include "utf8.h"
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


/* Render a cell's codepoint into a dump-safe form.
 *
 * Anything that would move the cursor or otherwise talk back to the terminal
 * displaying the dump has to be escaped, or reading a dump would garble the
 * reader's own screen. */
static void
_dump_codepoint(Eina_Unicode g)
{
   char utf8[8];
   int n;

   if (g == 0)
     {
        putchar(' ');
        return;
     }
   /* Media blocks are encoded with bit 31 set and are not text at all. */
   if (g & 0x80000000)
     {
        printf("\\B");
        return;
     }
   if (g < 0x20 || g == 0x7f)
     {
        printf("\\x%02x", (unsigned int)g);
        return;
     }
   n = codepoint_to_utf8(g, utf8);
   if (n <= 0)
     {
        printf("\\u%04x", (unsigned int)g);
        return;
     }
   fwrite(utf8, 1, n, stdout);
}

/* Emit one row as text plus a run-length summary of its attributes.
 *
 * Attributes are summarised rather than printed per cell because the point is
 * to make a diff between two dumps land on the cell that actually differs,
 * without burying it in eighty identical attribute records. */
static void
_dump_row(const Termcell *cells, int w, const char *label)
{
   int x, start;

   printf("%s |", label);
   for (x = 0; x < w; x++)
     _dump_codepoint(cells[x].codepoint);
   printf("|\n");

   x = 0;
   while (x < w)
     {
        const Termatt *a = &cells[x].att;

        start = x;
        while ((x < w) &&
               (memcmp(&cells[x].att, a, sizeof(Termatt)) == 0))
          x++;
        /* Skip the default run: saying nothing is clearer than saying nothing
         * verbosely, and it keeps a clean screen's dump short. */
        if ((a->fg != 0) || (a->bg != 0) || a->bold || a->faint || a->italic ||
            a->underline || a->blink || a->blink2 || a->inverse ||
            a->invisible || a->strike || a->fg256 || a->bg256 ||
            a->fgintense || a->bgintense || a->dblwidth || a->autowrapped ||
            a->newline || a->fraktur || a->framed || a->encircled ||
            a->overlined || a->link_id)
          {
             printf("%s  att %d-%d fg=%u bg=%u", label, start, x - 1,
                    (unsigned)a->fg, (unsigned)a->bg);
             if (a->fg256)      printf(" fg256");
             if (a->bg256)      printf(" bg256");
             if (a->fgintense)  printf(" fgint");
             if (a->bgintense)  printf(" bgint");
             if (a->bold)       printf(" bold");
             if (a->faint)      printf(" faint");
             if (a->italic)     printf(" italic");
             if (a->underline)  printf(" underline");
             if (a->blink)      printf(" blink");
             if (a->blink2)     printf(" blink2");
             if (a->inverse)    printf(" inverse");
             if (a->invisible)  printf(" invisible");
             if (a->strike)     printf(" strike");
             if (a->dblwidth)   printf(" dblwidth");
             if (a->autowrapped) printf(" autowrapped");
             if (a->newline)    printf(" newline");
             if (a->fraktur)    printf(" fraktur");
             if (a->framed)     printf(" framed");
             if (a->encircled)  printf(" encircled");
             if (a->overlined)  printf(" overlined");
             if (a->link_id)    printf(" link=%u", (unsigned)a->link_id);
             printf("\n");
          }
     }
}

/* Human-readable counterpart to _tytest_checksum().
 *
 * The checksum answers "did anything change"; this answers "what changed",
 * which is the question a scalar-versus-SIMD parity failure actually raises.
 * Two dumps piped through diff point straight at the offending cell. */
static void
_tytest_dump(Termpty *ty)
{
   ssize_t backlog_len;
   int y;

   printf("geom w=%d h=%d\n", ty->w, ty->h);
   printf("cursor x=%d y=%d wrapnext=%d shape=%s\n",
          ty->cursor_state.cx, ty->cursor_state.cy,
          (int)ty->cursor_state.wrapnext, tytest_cursor_shape_get());
   printf("mode altbuf=%d insert=%d wrap=%d bracketed_paste=%d\n",
          (int)ty->altbuf, (int)ty->termstate.insert,
          (int)ty->termstate.wrap, (int)ty->bracketed_paste);
   printf("margin top=%d bottom=%d left=%d right=%d restrict=%d\n",
          ty->termstate.top_margin, ty->termstate.bottom_margin,
          ty->termstate.left_margin, ty->termstate.right_margin,
          (int)ty->termstate.restrict_cursor);
   printf("title=%s\n", ty->prop.title ? ty->prop.title : "(NULL)");
   printf("icon=%s\n", ty->prop.icon ? ty->prop.icon : "(NULL)");

   backlog_len = termpty_backlog_length(ty);
   printf("backlog rows=%d\n", (int)backlog_len);
   for (y = (int)backlog_len; y >= 1; y--)
     {
        const Termcell *cells;
        ssize_t w = 0;
        char label[32];

        cells = termpty_cellrow_get(ty, -y, &w);
        snprintf(label, sizeof(label), "b%-4d", -y);
        if (cells && (w > 0)) _dump_row(cells, (int)w, label);
        else printf("%s ||\n", label);
     }

   printf("screen\n");
   for (y = 0; y < ty->h; y++)
     {
        char label[32];

        snprintf(label, sizeof(label), "s%-4d", y);
        _dump_row(&(TERMPTY_SCREEN(ty, 0, y)), ty->w, label);
     }

   if (ty->write_buffer.len)
     {
        size_t i;

        printf("reply ");
        for (i = 0; i < ty->write_buffer.len; i++)
          {
             unsigned char ch = (unsigned char)ty->write_buffer.buf[i];

             if ((ch >= 0x20) && (ch < 0x7f)) putchar(ch);
             else printf("\\x%02x", ch);
          }
        printf("\n");
     }
}

static void
_usage(const char *argv0)
{
   fprintf(stderr,
           "usage: %s                 read escape codes on stdin, print a state checksum\n"
           "       %s --dump          same, but print the state in a diffable text form\n"
           "       %s <test>|all      run the built-in unit tests\n",
           argv0, argv0, argv0);
}

int
main(int argc, char **argv)
{
   Eina_Bool dump = EINA_FALSE;
   int chunk = 0;
   int i;

   for (i = 1; i < argc; i++)
     {
        if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help"))
          {
             _usage(argv[0]);
             return 0;
          }
        else if (!strcmp(argv[i], "--dump")) dump = EINA_TRUE;
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

   if (dump) _tytest_dump(tytest_termpty_get());
   else _tytest_checksum(tytest_termpty_get());

   tytest_common_shutdown();

   emile_shutdown();
   eina_shutdown();

   return 0;
}
